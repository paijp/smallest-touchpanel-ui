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

/*
	The same read without the power-up gate, for bring-up: it always puts a
	transaction on the bus, so the trace below is filled even when the gate
	would never have opened.
*/
W	envision_touch_get_raw(W *x, W *y);

/*
	Raw state from the last transaction. An I2C failure on a board with no
	serial port is otherwise completely silent, and this is small enough to
	leave in permanently:

	  [0] the write address frame's acknowledge: 0 means answered
	  [1] the read address frame's acknowledge
	  [2] times a device has held the clock past the stretch limit
	  [3] which pin turned out to be the clock, settled at init by asking
	      the panel: 0 nothing answered either way, 1 SCL on P00, 2 on P01
	  [4] where it gave up: 0 none, 1 write address, 2 register number,
	      3 read address
	  [5] the controller's touch-point count, buf[0]
	  [6] the touch interrupt line, P02
	  [7] transactions that completed without a timeout
	  [8..14] the seven bytes of the last successful read, as received
*/
#define	ENVISION_I2C_TRACE_N	16
extern	volatile UW	envision_i2c_trace[ENVISION_I2C_TRACE_N];

#endif
