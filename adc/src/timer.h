#ifndef __CVI_TIMER_H__
#define __CVI_TIMER_H__

#include <stdint.h>

struct cvi_timer_regs_t {
	uint32_t LOAD_COUNT;
	uint32_t CURRENT_VALUE;
	uint32_t CTRL;
	uint32_t INTR_CLR;
	uint32_t INTR_STA;
};
static struct cvi_timer_regs_t timer_offsets = {
	.LOAD_COUNT = 0x0,
	.CURRENT_VALUE = 0x4,
	.CTRL = 0x8,
	.INTR_CLR = 0xC,
	.INTR_STA = 0x10,
};

static struct cvi_timer_regs_t *timer_reg = &timer_offsets;

#define TIMER_LOAD_COUNT(reg_base)    *((uint32_t *)(reg_base + timer_reg->LOAD_COUNT))
#define TIMER_CURRENT_VALUE(reg_base) *((uint32_t *)(reg_base + timer_reg->CURRENT_VALUE))
#define TIMER_CTRL(reg_base)          *((uint32_t *)(reg_base + timer_reg->CTRL))
#define TIMER_INTR_CLR(reg_base)      *((uint32_t *)(reg_base + timer_reg->INTR_CLR))
#define TIMER_INTR_STA(reg_base)      *((uint32_t *)(reg_base + timer_reg->INTR_STA))
#define TIMER_INTERRUPT_NUMBER        55

#define TIMER_CLK *((uint32_t *)(0x300200C))

static inline void hal_timer4_enable_clk()
{
	TIMER_CLK |= (1U << 13);
}

static inline void hal_timer4_disable_clk()
{
	TIMER_CLK &= ~(1U << 13);
}
#endif // __CVI_TIMER_H__
