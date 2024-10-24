/*
 * Copyright (C) CSItek Co., Ltd. 2019-2020. All rights reserved.
 */

#ifndef __PIN_H__
#define __PIN_H__

#include <stdint.h>

#define PIN_MUX_Pos (0U)
#define PIN_MUX_Msk (0x7 << PIN_MUX_Pos)

#define PIN_MODE_Pos      (2U)
#define PIN_MODE_Msk      (3U << PIN_MODE_Pos)
#define PIN_MODE_NONE     (0U << PIN_MODE_Pos)
#define PIN_MODE_PULLUP   (1U << PIN_MODE_Pos)
#define PIN_MODE_PULLDOWN (2U << PIN_MODE_Pos)

#define PIN_DRIVE_Pos        (5U)
#define PIN_DRIVE_Msk        (7U << PIN_DRIVE_Pos)
#define PIN_DRIVE_STRENGTH_0 (1U << PIN_DRIVE_Pos)
#define PIN_DRIVE_STRENGTH_1 (2U << PIN_DRIVE_Pos)

#define PIN_SPEED_Pos  (11U)
#define PIN_SPEED_Msk  (1U << PIN_SPEED_Pos)
#define PIN_SPEED_FAST (0U << PIN_SPEED_Pos)
#define PIN_SPEED_SLOW (1U << PIN_SPEED_Pos)

#define PIN_REG(reg_base) *((uint32_t *)(reg_base))

static inline void pin_set_mux(unsigned long reg_base, uint32_t value)
{
	PIN_REG(reg_base) &= ~PIN_MUX_Msk;
	PIN_REG(reg_base) |= (PIN_MUX_Msk & value);
}

static inline uint32_t pin_get_mux(unsigned long reg_base)
{
	return PIN_REG(reg_base) & PIN_MUX_Msk;
}

static inline void pin_set_mode(unsigned long reg_base, uint32_t value)
{
	PIN_REG(reg_base) &= ~PIN_MODE_Msk;
	PIN_REG(reg_base) |= (PIN_MODE_Msk & value);
}

static inline uint32_t pin_get_mode(unsigned long reg_base)
{
	return (PIN_REG(reg_base) & PIN_MODE_Msk);
}

static inline void pin_set_speed(unsigned long reg_base, uint32_t value)
{
	PIN_REG(reg_base) &= ~PIN_SPEED_Msk;
	PIN_REG(reg_base) |= (PIN_SPEED_Msk & value);
}

static inline uint32_t pin_get_speed(unsigned long reg_base)
{
	return (PIN_REG(reg_base) & PIN_SPEED_Msk);
}

static inline void pin_set_drive(unsigned long reg_base, uint32_t value)
{
	PIN_REG(reg_base) &= ~PIN_DRIVE_Msk;
	PIN_REG(reg_base) |= (PIN_DRIVE_Msk & value);
}

static inline uint32_t pin_get_drive(unsigned long reg_base)
{
	return (PIN_REG(reg_base) & PIN_DRIVE_Msk);
}
#endif
