/*
 * Copyright (c) 2020, STMICROELECTRONICS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "openamp/rpmsg.h"
#include "zephyr/sys/printk.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/timing/timing.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/drivers/ipm.h>

#include <openamp/open_amp.h>
#include <metal/device.h>
#include <resource_table.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(openamp_rsc_table, LOG_LEVEL_INF);

#define SHM_DEVICE_NAME "shm"

#define MBOX_TX_CHAN 2
#define MBOX_RX_CHAN 3

#if !DT_HAS_CHOSEN(zephyr_ipc_shm)
#error "Sample requires definition of shared memory for rpmsg"
#endif

/* Constants derived from device tree */
#define SHM_NODE       DT_CHOSEN(zephyr_ipc_shm)
#define SHM_START_ADDR DT_REG_ADDR(SHM_NODE)
#define SHM_SIZE       DT_REG_SIZE(SHM_NODE)

#define APP_MNG_TASK_STACK_SIZE (1024)
#define APP_SC_TASK_STACK_SIZE  (1024)
#define APP_TTY_TASK_STACK_SIZE (1024)

#define MESSAGE_SIZE 4
#define NUM_MESSAGES 5000

K_THREAD_STACK_DEFINE(thread_mng_stack, APP_MNG_TASK_STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_rp__client_stack, APP_SC_TASK_STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_tty_stack, APP_TTY_TASK_STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_netlink_stack, APP_TTY_TASK_STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_nocopy_stack, APP_SC_TASK_STACK_SIZE);

static struct k_thread thread_mng_data;
static struct k_thread thread_rp__client_data;
static struct k_thread thread_tty_data;
static struct k_thread thread_netlink_data;
static struct k_thread thread_nocopy_data;

static const struct device *const ipm_handle = DEVICE_DT_GET(DT_CHOSEN(zephyr_ipc));

static metal_phys_addr_t shm_physmap = SHM_START_ADDR;

struct metal_device shm_device = {.name = SHM_DEVICE_NAME,
				  .num_regions = 2,
				  .regions =
					  {
						  {.virt = NULL}, /* shared memory */
						  {.virt = NULL}, /* rsc_table memory */
					  },
				  .node = {NULL},
				  .irq_num = 0,
				  .irq_info = NULL};

struct rpmsg_rcv_msg {
	void *data;
	size_t len;
};

static struct metal_io_region *shm_io;
static struct rpmsg_virtio_shm_pool shpool;

static struct metal_io_region *rsc_io;
static struct rpmsg_virtio_device rvdev;

static void *rsc_table;
static struct rpmsg_device *rpdev;

static char rx_sc_msg[MESSAGE_SIZE];
static struct rpmsg_endpoint sc_ept;
static struct rpmsg_rcv_msg sc_msg = {.data = rx_sc_msg};

static struct rpmsg_endpoint nocopy_ept;
static struct rpmsg_rcv_msg nocopy_msg = {.data = NULL};

static struct rpmsg_endpoint tty_ept;
static struct rpmsg_rcv_msg tty_msg;

static struct rpmsg_endpoint netlink_ept;
static struct rpmsg_rcv_msg netlink_msg;

static K_SEM_DEFINE(data_sem, 0, 1);
static K_SEM_DEFINE(data_sc_sem, 0, 1);
static K_SEM_DEFINE(data_nocopy_sem, 0, 1);
static K_SEM_DEFINE(data_tty_sem, 0, 1);
static K_SEM_DEFINE(data_netlink_sem, 0, 1);

static void platform_ipm_callback(const struct device *dev, void *context, uint32_t id,
				  volatile void *data)
{
	LOG_DBG("msg received from mailbox chan %d\n", id);
	if (id == MBOX_RX_CHAN) {
		k_sem_give(&data_sem);
	}
}

static int rpmsg_recv_nocopy_callback(struct rpmsg_endpoint *ept, void *data, size_t len,
				      uint32_t src, void *priv)
{
	uint32_t max_size = rpmsg_virtio_get_tx_buffer_size(rpdev);
	if ((int32_t)max_size <= 0) {
		printk("rpmsg-nocopy: Failed to get tx buffer size\n");
		return RPMSG_ERR_NO_BUFF;
	}

	nocopy_msg.data = rpmsg_get_tx_payload_buffer(ept, &max_size, true);
	nocopy_msg.len = max_size;

	if (!nocopy_msg.data) {
		printk("rpmsg-nocopy: Failed to get tx buffer\n");
		return RPMSG_ERR_NO_BUFF;
	}

	if (len > max_size) {
		printk("rpmsg-nocopy: Message is too big for tx buffer\n");
		rpmsg_release_tx_buffer(ept, nocopy_msg.data);
		nocopy_msg.data = NULL;
		return RPMSG_ERR_NO_BUFF;
	}

	memcpy(nocopy_msg.data, data, len);
	k_sem_give(&data_nocopy_sem);
	return RPMSG_SUCCESS;
}

static int rpmsg_recv_cs_callback(struct rpmsg_endpoint *ept, void *data, size_t len, uint32_t src,
				  void *priv)
{
	memcpy(sc_msg.data, data, len);
	sc_msg.len = len;
	k_sem_give(&data_sc_sem);

	return RPMSG_SUCCESS;
}

static int rpmsg_recv_tty_callback(struct rpmsg_endpoint *ept, void *data, size_t len, uint32_t src,
				   void *priv)
{
	struct rpmsg_rcv_msg *msg = priv;

	rpmsg_hold_rx_buffer(ept, data);
	msg->data = data;
	msg->len = len;
	k_sem_give(&data_tty_sem);

	return RPMSG_SUCCESS;
}

static int rpmsg_recv_netlink_callback(struct rpmsg_endpoint *ept, void *data, size_t len,
				       uint32_t src, void *priv)
{
	struct rpmsg_rcv_msg *msg = priv;

	rpmsg_hold_rx_buffer(ept, data);
	msg->data = data;
	msg->len = len;
	k_sem_give(&data_netlink_sem);

	return RPMSG_SUCCESS;
}

static void receive_message(unsigned char **msg, unsigned int *len)
{
	int status = k_sem_take(&data_sem, K_FOREVER);

	if (status == 0) {
		rproc_virtio_notified(rvdev.vdev, VRING1_ID);
	}
}

static void new_service_cb(struct rpmsg_device *rdev, const char *name, uint32_t src)
{
	LOG_ERR("%s: unexpected ns service receive for name %s\n", __func__, name);
}

int mailbox_notify(void *priv, uint32_t id)
{
	ARG_UNUSED(priv);

	if (id == 0) {
		LOG_DBG("sending kick to mailbox chan %d\n", MBOX_TX_CHAN);
		ipm_send(ipm_handle, 0, MBOX_TX_CHAN, NULL, 0);
	}

	return 0;
}

unsigned long phys_to_offset(struct metal_io_region *io, metal_phys_addr_t phys)
{
	return (unsigned long)phys - (unsigned long)io->virt;
}

int platform_init(void)
{
	void *rsc_tab_addr;
	int rsc_size;
	struct metal_device *device;
	struct metal_init_params metal_params = METAL_INIT_DEFAULTS;
	int status;

	const struct metal_io_ops nops = {
		.phys_to_offset = phys_to_offset,
	};

	status = metal_init(&metal_params);
	if (status) {
		LOG_DBG("metal_init: failed: %d\n", status);
		return -1;
	}

	status = metal_register_generic_device(&shm_device);
	if (status) {
		LOG_DBG("Couldn't register shared memory: %d\n", status);
		return -1;
	}

	status = metal_device_open("generic", SHM_DEVICE_NAME, &device);
	if (status) {
		LOG_DBG("metal_device_open failed: %d\n", status);
		return -1;
	}

	/* declare shared memory region */
	metal_io_init(&device->regions[0], (void *)SHM_START_ADDR, &shm_physmap, SHM_SIZE, -1, 0,
		      &nops);

	shm_io = metal_device_io_region(device, 0);
	if (!shm_io) {
		LOG_DBG("Failed to get shm_io region\n");
		return -1;
	}

	/* declare resource table region */
	rsc_table_get(&rsc_tab_addr, &rsc_size);
	rsc_table = (struct st_resource_table *)rsc_tab_addr;

	metal_io_init(&device->regions[1], rsc_table, (metal_phys_addr_t *)rsc_table, rsc_size, -1,
		      0, NULL);

	rsc_io = metal_device_io_region(device, 1);
	if (!rsc_io) {
		LOG_DBG("Failed to get rsc_io region\n");
		return -1;
	}

	/* setup IPM */
	if (!device_is_ready(ipm_handle)) {
		LOG_DBG("IPM device is not ready\n");
		return -1;
	}

	ipm_register_callback(ipm_handle, platform_ipm_callback, NULL);

	status = ipm_set_enabled(ipm_handle, 1);
	if (status) {
		LOG_DBG("ipm_set_enabled failed\n");
		return -1;
	}

	return 0;
}

static void cleanup_system(void)
{
	ipm_set_enabled(ipm_handle, 0);
	rpmsg_deinit_vdev(&rvdev);
	metal_finish();
}

struct rpmsg_device *platform_create_rpmsg_vdev(unsigned int vdev_index, unsigned int role,
						void (*rst_cb)(struct virtio_device *vdev),
						rpmsg_ns_bind_cb ns_cb)
{
	struct fw_rsc_vdev_vring *vring_rsc;
	struct virtio_device *vdev;
	int ret;

	vdev = rproc_virtio_create_vdev(VIRTIO_DEV_DEVICE, VDEV_ID, rsc_table_to_vdev(rsc_table),
					rsc_io, NULL, mailbox_notify, NULL);

	if (!vdev) {
		LOG_DBG("failed to create vdev\r\n");
		return NULL;
	}

	/* wait master rpmsg init completion */
	rproc_virtio_wait_remote_ready(vdev);

	vring_rsc = rsc_table_get_vring0(rsc_table);
	ret = rproc_virtio_init_vring(vdev, 0, vring_rsc->notifyid,
				      (void *)(uintptr_t)(vring_rsc->da), rsc_io, vring_rsc->num,
				      vring_rsc->align);
	if (ret) {
		LOG_DBG("failed to init vring 0\r\n");
		goto failed;
	}

	vring_rsc = rsc_table_get_vring1(rsc_table);
	ret = rproc_virtio_init_vring(vdev, 1, vring_rsc->notifyid,
				      (void *)(uintptr_t)vring_rsc->da, rsc_io, vring_rsc->num,
				      vring_rsc->align);
	if (ret) {
		LOG_DBG("failed to init vring 1\r\n");
		goto failed;
	}

	rpmsg_virtio_init_shm_pool(&shpool, NULL, SHM_SIZE);
	ret = rpmsg_init_vdev(&rvdev, vdev, ns_cb, shm_io, &shpool);

	if (ret) {
		LOG_DBG("failed rpmsg_init_vdev\r\n");
		goto failed;
	}

	return rpmsg_virtio_get_rpmsg_device(&rvdev);

failed:
	rproc_virtio_remove_vdev(vdev);

	return NULL;
}

void app_rpmsg_send_nocopy(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	timing_t start_time, end_time;
	timing_t start_send_time, end_send_time;
	uint64_t total_cycles, total_ns;
	uint64_t total_send_cycles, total_send_ns;
	unsigned int msg_cnt = 0;
	int ret = 0;

	k_sem_take(&data_nocopy_sem, K_FOREVER);

	printk("\r\nOpenAMP[remote] Linux sample client nocopy responder started\r\n");

	ret = rpmsg_create_ept(&nocopy_ept, rpdev, "rpmsg-nocopy", RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
			       rpmsg_recv_nocopy_callback, NULL);
	total_send_cycles = 0;

	while (msg_cnt < NUM_MESSAGES) {
		k_sem_take(&data_nocopy_sem, K_FOREVER);
		if (msg_cnt == 0) {
			start_time = timing_counter_get();
		}
		msg_cnt++;
		start_send_time = timing_counter_get();
		rpmsg_send_nocopy(&nocopy_ept, nocopy_msg.data, nocopy_msg.len);
		end_send_time = timing_counter_get();
		total_send_cycles += timing_cycles_get(&start_send_time, &end_send_time);
	}

	total_send_ns = timing_cycles_to_ns(total_send_cycles);

	end_time = timing_counter_get();
	total_cycles = timing_cycles_get(&start_time, &end_time);
	total_ns = timing_cycles_to_ns(total_cycles);

	rpmsg_destroy_ept(&nocopy_ept);

	printk("OpenAMP Linux nocopy send client responder ended\n");

	printk("\n------ RESULTS: rpmsg-nocopy ------\n");
	printk("Total messages received: %d\n", msg_cnt);
	printk("Total time: %lld ms\n", total_ns / 1000000);
	printk("Total send time: %lld ms\n", total_send_ns / 1000000);
}

void app_rpmsg_client_sample(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	timing_t start_time, end_time;
	timing_t start_send_time, end_send_time;
	uint64_t total_cycles, total_ns;
	uint64_t total_send_cycles, total_send_ns;
	unsigned int msg_cnt = 0;
	int ret = 0;

	k_sem_take(&data_sc_sem, K_FOREVER);

	printk("\r\nOpenAMP[remote] Linux sample client responder started\r\n");

	ret = rpmsg_create_ept(&sc_ept, rpdev, "rpmsg-client-sample", RPMSG_ADDR_ANY,
			       RPMSG_ADDR_ANY, rpmsg_recv_cs_callback, NULL);

	total_send_cycles = 0;
	while (msg_cnt < NUM_MESSAGES) {
		k_sem_take(&data_sc_sem, K_FOREVER);
		if (msg_cnt == 0) {
			start_time = timing_counter_get();
		}
		msg_cnt++;
		start_send_time = timing_counter_get();

		rpmsg_send(&sc_ept, sc_msg.data, sc_msg.len);

		end_send_time = timing_counter_get();
		total_send_cycles += timing_cycles_get(&start_send_time, &end_send_time);
	}

	total_send_ns = timing_cycles_to_ns(total_send_cycles);
	end_time = timing_counter_get();
	total_cycles = timing_cycles_get(&start_time, &end_time);
	total_ns = timing_cycles_to_ns(total_cycles);

	printk("OpenAMP Linux sample client responder ended\n");

	printk("\n------ RESULTS: rpmsg-client-sample ------\n");
	printk("Total messages received: %d\n", msg_cnt);
	printk("Total time: %lld ms\n", total_ns / 1000000);
	printk("Total send time: %lld ms\n", total_send_ns / 1000000);

	rpmsg_destroy_ept(&sc_ept);
}

void app_rpmsg_tty(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	unsigned char tx_buff[MESSAGE_SIZE];
	int ret = 0;
	int msg_cnt = 0;
	timing_t start_time, end_time;
	uint64_t total_cycles, total_ns;

	k_sem_take(&data_tty_sem, K_FOREVER);

	printk("\r\nOpenAMP[remote] Linux TTY responder started\r\n");

	tty_ept.priv = &tty_msg;
	ret = rpmsg_create_ept(&tty_ept, rpdev, "rpmsg-tty", RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
			       rpmsg_recv_tty_callback, NULL);

	while (tty_ept.addr != RPMSG_ADDR_ANY) {
		k_sem_take(&data_tty_sem, K_FOREVER);
		if (tty_msg.len) {
			memcpy(tx_buff, tty_msg.data, tty_msg.len);
			rpmsg_send(&tty_ept, tx_buff, sizeof(tx_buff));
			rpmsg_release_rx_buffer(&tty_ept, tty_msg.data);

			if (msg_cnt == 0) {
				start_time = timing_counter_get();
			}
		}
		tty_msg.len = 0;
		tty_msg.data = NULL;
		msg_cnt++;
	}

	end_time = timing_counter_get();
	total_cycles = timing_cycles_get(&start_time, &end_time);
	total_ns = timing_cycles_to_ns(total_cycles);

	rpmsg_destroy_ept(&tty_ept);
}

void app_rpmsg_netlink(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	unsigned char tx_buff[MESSAGE_SIZE];
	int ret = 0;
	int msg_cnt = 0;
	timing_t start_time, end_time;
	uint64_t total_cycles, total_ns;

	k_sem_take(&data_netlink_sem, K_FOREVER);

	printk("\r\nOpenAMP[remote] Linux Netlink responder started\r\n");

	netlink_ept.priv = &netlink_msg;
	ret = rpmsg_create_ept(&netlink_ept, rpdev, "rpmsg-netlink", RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
			       rpmsg_recv_netlink_callback, NULL);

	while (netlink_ept.addr != RPMSG_ADDR_ANY) {
		k_sem_take(&data_netlink_sem, K_FOREVER);
		if (netlink_msg.len) {
			memcpy(tx_buff, netlink_msg.data, netlink_msg.len);
			rpmsg_send(&netlink_ept, tx_buff, sizeof(tx_buff));
			rpmsg_release_rx_buffer(&netlink_ept, netlink_msg.data);

			if (msg_cnt == 0) {
				start_time = timing_counter_get();
			}
		}
		netlink_msg.len = 0;
		netlink_msg.data = NULL;
		msg_cnt++;
	}

	end_time = timing_counter_get();
	total_cycles = timing_cycles_get(&start_time, &end_time);
	total_ns = timing_cycles_to_ns(total_cycles);

	rpmsg_destroy_ept(&netlink_ept);
}

void rpmsg_mng_task(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	unsigned char *msg;
	unsigned int len;
	int ret = 0;

	// init timing
	timing_init();
	timing_start();

	printk("\r\nOpenAMP[remote] Linux responder demo started\r\n");

	/* Initialize platform */
	ret = platform_init();
	if (ret) {
		LOG_ERR("Failed to initialize platform\n");
		ret = -1;
		goto task_end;
	}

	// Create rpmsg vdev - Only one shared channel, different services are provided through
	// different endpoints
	rpdev = platform_create_rpmsg_vdev(0, VIRTIO_DEV_DEVICE, NULL, new_service_cb);
	if (!rpdev) {
		LOG_ERR("Failed to create rpmsg virtio device\n");
		ret = -1;
		goto task_end;
	}

	printk("RPMSG tx buffer size: %d\n", rpmsg_virtio_get_tx_buffer_size(rpdev));
	printk("RPMSG rx buffer size: %d\n", rpmsg_virtio_get_rx_buffer_size(rpdev));

	/* start the rpmsg clients */
	k_sem_give(&data_sc_sem);
	k_sem_give(&data_tty_sem);
	k_sem_give(&data_nocopy_sem);
	k_sem_give(&data_netlink_sem);

	while (1) {
		receive_message(&msg, &len);
	}

task_end:
	cleanup_system();

	printk("OpenAMP demo ended\n");
}

int main(void)
{
	printk("Starting application threads!\n");
	k_tid_t mng =
		k_thread_create(&thread_mng_data, thread_mng_stack, APP_MNG_TASK_STACK_SIZE,
				rpmsg_mng_task, NULL, NULL, NULL, K_PRIO_COOP(8), 0, K_NO_WAIT);
	k_tid_t rpmsg_sc = k_thread_create(&thread_rp__client_data, thread_rp__client_stack,
					   APP_SC_TASK_STACK_SIZE, app_rpmsg_client_sample, NULL,
					   NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);
	k_tid_t tty =
		k_thread_create(&thread_tty_data, thread_tty_stack, APP_TTY_TASK_STACK_SIZE,
				app_rpmsg_tty, NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);

	k_tid_t netlink =
		k_thread_create(&thread_netlink_data, thread_netlink_stack, APP_TTY_TASK_STACK_SIZE,
				app_rpmsg_netlink, NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);

	// k_tid_t nocopy = k_thread_create(&thread_nocopy_data, thread_nocopy_stack,
	// 				 APP_SC_TASK_STACK_SIZE, app_rpmsg_send_nocopy, NULL, NULL,
	// 				 NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);

	k_thread_name_set(mng, "manager");
	k_thread_name_set(rpmsg_sc, "rpmsg-client-sample");
	// k_thread_name_set(nocopy, "rpmsg-nocopy");
	k_thread_name_set(tty, "rpmsg-tty");
	k_thread_name_set(netlink, "rpmsg-netlink");
	return 0;
}
