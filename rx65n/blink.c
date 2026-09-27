/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	Is the CPU running at all? The smallest thing that can answer that.

	Clock setup, then blink the LCD backlight on P66 and count on the serial
	log. No GLCDC, no touch controller, no I2C, no framebuffer - nothing that
	the freeze hunt has been looking at.

	The backlight is the point. It is a plain GPIO that the program sets once
	and never clears, so a frozen CPU cannot turn it off: if it goes dark, the
	MCU was reset or lost power, which is a completely different fault from a
	program that stopped. Blinking it turns that one bit into a channel that
	needs no serial port, no debugger and nothing working except the core and
	one port pin - so it still answers when every other channel has gone quiet.

	Read it either way:

	  - backlight blinking, log counting      the board is fine, and whatever
	                                         stops is in what this leaves out
	  - backlight blinking, log silent        the CPU runs; the serial path or
	                                         the probe's side of it does not
	  - backlight steady on or steady off     the CPU stopped, and the log's
	                                         last line says where
	  - backlight dark from the start         it never got as far as the first
	                                         GPIO write

	Build it instead of sample1.c.
*/

#include	"iodefine.h"
#include	"lcdtp.h"
#include	"envision_hw.h"
#include	"seriallog.h"


#define	HALF_PERIOD_MS	500


int	main(void)
{
	UW	n = 0;

	envision_clock_init();
	seriallog_init();

	/*
		P66 drives the backlight and P63 the panel's reset, exactly as
		envision_lcd_init() sets them - but without bringing up the display
		controller, so nothing here touches expansion RAM or the GLCDC.
	*/
	PORT6.PODR.BIT.B3 = 1;		/* panel reset released */
	PORT6.PDR.BIT.B3 = 1;
	PORT6.PODR.BIT.B6 = 1;		/* backlight on */
	PORT6.PDR.BIT.B6 = 1;

	lcdtp_sendlogs("\r\n--- blink: backlight on P66, ");
	lcdtp_sendlogdec(HALF_PERIOD_MS);
	lcdtp_sendlogs("ms half period ---\r\n");

	for (;;) {
		PORT6.PODR.BIT.B6 = 1;
		lcdtp_sendlogs("on  ");
		lcdtp_sendlogdec((W)n);
		lcdtp_sendlogs("\r\n");
		envision_delay_ms(HALF_PERIOD_MS);

		PORT6.PODR.BIT.B6 = 0;
		lcdtp_sendlogs("off ");
		lcdtp_sendlogdec((W)n);
		lcdtp_sendlogs(" drops=");
		lcdtp_sendlogdec((W)seriallog_dropped());
		lcdtp_sendlogs("\r\n");
		envision_delay_ms(HALF_PERIOD_MS);

		n++;
	}
}
