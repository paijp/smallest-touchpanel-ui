/*
	MIT License

	Copyright (c) 2019 John Blaiklock
	Copyright (c) 2026 paijp

	Board bring-up for the Renesas RX65N Envision Kit: system clock, the
	GLCDC display controller and the FT5x06 touch controller on SCI6's
	simple-I2C.

	Derived from miniwinwm/RenesasEnvisionGCC (EnvisionDemo1), which is MIT
	licensed, and kept in a file of its own so that the port proper -
	lcdtp.c, Apache 2.0 - stays free of code under a different licence.

	See envision_hw.c for what was changed and why.
*/

#ifndef	RX65N_ENVISION_HW_H
#define	RX65N_ENVISION_HW_H

#include	"basic.h"

/* 480x272 RGB565, scanned out of expansion RAM by the GLCDC. */
#define	ENVISION_LCD_W		480
#define	ENVISION_LCD_H		272
#define	ENVISION_FRAMEBUFFER	((volatile UH*)0x00800000)

void	envision_delay_us(UW us);
void	envision_delay_ms(UW ms);

void	envision_clock_init(void);
void	envision_lcd_init(void);
void	envision_touch_init(void);

/*
	Reads one touch point. Returns 1 and fills x/y when the panel reports a
	contact, 0 when it does not and 0 when the bus did not answer - a caller
	that only wants to know "is a finger down" cannot tell those apart, and
	does not need to.
*/
W	envision_touch_get(W *x, W *y);

#endif
