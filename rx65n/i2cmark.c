/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	The hook i2c.h calls with -DI2C_MARK, in a file of its own so any program
	can be built with the marks without editing it. i2clog.c has its own copy
	for historical reasons; link one or the other, not both.

	One character per step of every transaction, out of the serial port as it
	happens. The point is where a run stops, not how far the counters got: a
	trace kept in RAM cannot be read back on this board, and the pass counter
	only says which pass was the last, not where inside it control was lost.
	What the marks found is that control is lost immediately after a
	transaction completes - the seven data bytes, the stop condition and all
	nine steps of i2cstop all arrive - in straight-line code that cannot hang.

	And with -DI2C_MARK_PIN it also flips the backlight on every mark, which is
	what separates the two things a silent log can mean. If the program has
	stopped, nothing calls this any more and the light holds still. If instead
	the serial transmitter has died while the program runs on, the calls keep
	coming and the light keeps flickering, because a GPIO does not depend on
	SCI1 being alive. That question cannot be answered from the log - from
	outside, a stopped program and a dead transmitter look identical - and it
	decides whether the fault is in the program at all.

	P66 is the backlight, a plain GPIO on this board, and envision_lcd_init()
	has already made it an output by the time any of this runs; setting the
	direction here as well costs one write and makes the file work in a program
	that never brings the display up.
*/

#include	"iodefine.h"
#include	"basic.h"
#include	"seriallog.h"


void	i2c_mark(W c)
{
#ifdef	I2C_MARK_PIN
	PORT6.PDR.BIT.B6 = 1;
	PORT6.PODR.BIT.B6 = (UB)(PORT6.PODR.BIT.B6? 0 : 1);
#endif
	seriallog_putc(c);
}
