/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	The touch controller, on the log only. No display at all.

	blink.c established that the core, the clock, one GPIO and the serial path
	are all sound: sixteen seconds of blinking with every line arriving. So the
	freeze is in what blink left out, and the way to find which part is to put
	them back one at a time. This is the first step - the I2C half without the
	display half:

	    envision_clock_init()     yes
	    seriallog_init()          yes
	    envision_touch_init()     yes
	    envision_lcd_init()       NO
	    init_lcdtp()              NO

	Nothing here writes to the framebuffer, nothing brings up the GLCDC, and
	nothing calls into lcdtp's drawing. If it runs and keeps running, the
	display side is where to look next; if it stops, the I2C side is.

	The backlight is the liveness signal, since there is no screen to write to.
	It is a plain GPIO that a frozen CPU cannot clear, so:

	  - blinking            running
	  - stopped, lit        the CPU stopped with the light on
	  - dark                the MCU was reset or lost power - a different
	                        fault from a program that stopped, and the one
	                        that fits a program counter wandering out of the
	                        code and eventually reaching the reset vector

	The interrupt state goes on the log as well. Nothing in this port enables
	an interrupt - the GLCDC's requests are switched off, ICU GROUPAL1 is
	cleared, and the touch controller's INT line on P02 is configured as a
	plain input that is polled, never as an IRQ pin. Printing every IER byte
	and the PSW I bit says so from the hardware rather than from reading the
	source, and would show it immediately if something enabled one after all.
*/

#include	"iodefine.h"
#include	"lcdtp.h"
#include	"envision_hw.h"
#include	"seriallog.h"


/* From the linker script, for reporting where .bss ends. */
extern	char	ebss[];

#define	PERIOD_MS	200

/* Blink the backlight every this many passes, so it is visibly alive. */
#define	BLINK_PASSES	2


static	void	log_dec(const char *label, UW v)
{
	lcdtp_sendlogs(label);
	lcdtp_sendlogdec((W)v);
}


/*
	Every interrupt-enable byte in the ICU, plus the I bit. All zero means no
	interrupt can be taken at all, whatever the touch controller's INT line is
	doing.
*/
static	void	log_interrupt_state(const char *when)
{
	W	i;
	UW	psw;

	lcdtp_sendlogs("int state ");
	lcdtp_sendlogs(when);
	lcdtp_sendlogs(": ier");
	for (i = 0; i < 32; i++) {
		lcdtp_sendlogs(" ");
		lcdtp_sendlogub(ICU.IER[i].BYTE);
	}

	__asm__ __volatile__ ("mvfc psw, %0" : "=r" (psw));
	lcdtp_sendlogs(" psw=");
	lcdtp_sendloguw(psw);
	lcdtp_sendlogs(" ipl_i=");
	lcdtp_sendlogdec((W)((psw >> 16) & 1));	/* PSW.I */
	lcdtp_sendlogs(" p02=");
	lcdtp_sendlogdec((W)PORT0.PIDR.BIT.B2);
	lcdtp_sendlogs("\r\n");
}


int	main(void)
{
	UW	pass = 0;
	W	x, y, touched, i;

	envision_clock_init();
	seriallog_init();

	/* The backlight, without the display controller behind it. */
	PORT6.PODR.BIT.B3 = 1;
	PORT6.PDR.BIT.B3 = 1;
	PORT6.PODR.BIT.B6 = 1;
	PORT6.PDR.BIT.B6 = 1;

	lcdtp_sendlogs("\r\n--- i2clog: touch controller, no display, every ");
	lcdtp_sendlogdec(PERIOD_MS);
	lcdtp_sendlogs("ms ---\r\n");

	/*
		The one way control can leave the I2C code. i2cwait(), in the inner
		loop of every transaction, calls lcdtp_polltask() if it is non-NULL -
		and every wait in i2c.h is a bounded for loop, so an indirect call
		through this pointer is the only thing in there that can transfer
		control somewhere unplanned.
		
		It lives in .bss and is meant to be NULL until someone sets it. Print
		it before trusting that: if the startup code's .bss clearing did not
		happen, it holds whatever survived the reset - RAM is not cleared by
		one - and the I2C then calls it. That would be intermittent exactly as
		observed, because whether the leftover word is zero depends on what ran
		before.
		
		Then set it to NULL regardless, so the rest of the run cannot go that
		way whatever it held.
	*/
	lcdtp_sendlogs("polltask before = ");
	lcdtp_sendloguw((UW)lcdtp_polltask);
	lcdtp_sendlogs("  bss ");
	lcdtp_sendloguw((UW)&lcdtp_polltask);
	lcdtp_sendlogs("..");
	lcdtp_sendloguw((UW)ebss);
	lcdtp_sendlogs("\r\n");

	lcdtp_polltask = NULL;

	log_interrupt_state("before touch init");
	envision_touch_init();
	log_interrupt_state("after touch init");

	for (;;) {
		x = -1;
		y = -1;
		touched = envision_touch_get_raw(&x, &y);
		pass++;

		PORT6.PODR.BIT.B6 = ((pass / BLINK_PASSES) & 1)? 1 : 0;

		lcdtp_sendlogdec((W)(pass * PERIOD_MS / 1000));
		lcdtp_sendlogs(".");
		lcdtp_sendlogdec((W)((pass * PERIOD_MS) % 1000));
		log_dec(" n=", pass);
		log_dec(" ok=", envision_i2c_trace[7]);
		log_dec(" gaveup=", envision_i2c_trace[4]);
		log_dec(" step=", envision_i2c_trace[15]);
		log_dec(" pin=", envision_i2c_trace[3]);
		log_dec(" int=", envision_i2c_trace[6]);
		log_dec(" nak=", envision_i2c_trace[0]);
		lcdtp_sendlogs("/");
		lcdtp_sendlogdec((W)envision_i2c_trace[1]);
		log_dec(" str=", envision_i2c_trace[2]);
		log_dec(" drops=", seriallog_dropped());
		lcdtp_sendlogs(" plt=");
		lcdtp_sendloguw((UW)lcdtp_polltask);

		lcdtp_sendlogs(" | ");
		for (i = 0; i < 7; i++) {
			lcdtp_sendlogub(envision_i2c_trace[8 + i]);
			lcdtp_sendlogs(" ");
		}

		lcdtp_sendlogs("| ");
		if ((touched)) {
			log_dec("x=", (UW)x);
			log_dec(" y=", (UW)y);
		} else {
			lcdtp_sendlogs("no contact");
		}

		/*
			The trace as raw words too. A field printed under the wrong
			label has already cost a round of theorising here; with the
			words on the line the reading can be redone afterwards.
		*/
		lcdtp_sendlogs(" | trace");
		for (i = 0; i < ENVISION_I2C_TRACE_N; i++) {
			lcdtp_sendlogs(" ");
			lcdtp_sendloguh(envision_i2c_trace[i]);
		}
		lcdtp_sendlogs("\r\n");

		envision_delay_ms(PERIOD_MS);
	}
}
