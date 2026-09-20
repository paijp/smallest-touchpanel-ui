/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	Touch bring-up display.

	The board has no serial port the host can reach, so when the panel draws
	correctly and never reports a touch there is nothing to look at. This
	puts the I2C layer's raw state on the screen it has already proved it can
	draw on, which is the one debug channel that needs no extra hardware and
	no debugger.

	Build it instead of sample1.c. It also writes each line to the debug log,
	so if the RRM/DMM path is up the same information arrives on the host
	without anyone having to read the screen.
*/

#include	"lcdtp.h"
#include	"envision_hw.h"
#include	"debuglog.h"


static	const	char	*label[ENVISION_I2C_TRACE_N] = {
	"nak wr  ", "nak rd  ", "stretch ", "pinorder",
	"gaveup  ", "points  ", "int p02 ", "ok count",
	"raw[0]  ", "raw[1]  ", "raw[2]  ", "raw[3]  ",
	"raw[4]  ", "raw[5]  ", "raw[6]  ", "step    ",
	"loopstep", "        ", "        ", "        "
};


static	void	hex8(UW v, UB *p)
{
	static	const	char	*bin2hex = "0123456789abcdef";
	W	i;

	for (i = 7; i >= 0; i--) {
		p[i] = (UB)bin2hex[v & 0xf];
		v >>= 4;
	}
	p[8] = 0;
}


static	void	dec5(W v, UB *p)
{
	W	i;

	/* "-1" is the not-yet value; it has to survive as a visible dash */
	for (i = 4; i >= 0; i--) {
		p[i] = (UB)((v < 0)? '-' : '0' + (v % 10));
		v /= 10;
	}
	p[5] = 0;
}


int	main(void)
{
	static	UB	line[24];
	static	UW	prev[ENVISION_I2C_TRACE_N];
	static	W	touches = 0;
	static	W	lastx = -1, lasty = -1;
	W	x, y, touched, i, changed;

	init_lcdtp();
	/* let the controller finish coming up before anyone asks it anything */
	dly_tsk(500);
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	gdra_stp(8, 20, 0x07e0, 0x0000, NULL, (UB*)"rx65n touch diag 4 (bitbang)");
	lcdtp_sendlogs("diag1 up\n");

	for (;;) {
		envision_i2c_trace[16] = 1;
		x = y = -1;
		/*
			Through the gate this time, as the real driver reads: no
			transaction goes out until the interrupt line has been seen
			low once. The trace still shows the line (int p02) on every
			pass, so the log records when - whether - the gate opens.
		*/
		touched = envision_touch_get(&x, &y);
		envision_i2c_trace[16] = 2;

		/*
			Log only what changed. At eight lines a pass the ring
			buffer holds about four seconds, which is not enough to
			still contain a touch by the time anyone can read it;
			the steady state is identical pass after pass anyway.
		*/
		changed = 0;
		for (i = 0; i < ENVISION_I2C_TRACE_N; i++)
			if (i != 7 && envision_i2c_trace[i] != prev[i])
				changed = 1;	/* [7] is a counter; it always changes */

		envision_i2c_trace[16] = 3;
		for (i = 0; i < ENVISION_I2C_TRACE_N; i++) {
			/*
				The log takes every slot; the screen takes the
				ones that fit on it. gfil_rec and gdra_stp are
				given the framebuffer's coordinates directly,
				so a row past the bottom is a write past the
				end of it.
			*/
			if (42 + i * 14 <= LCD_H) {
				gfil_rec(8, 28 + i * 14, 240, 42 + i * 14,
					 0x0000);
				gdra_stp(8, 40 + i * 14, 0xffff, 0x0000, NULL,
					 (UB*)label[i]);
				hex8(envision_i2c_trace[i], line);
				gdra_stp(88, 40 + i * 14, 0xffe0, 0x0000,
					 NULL, line);
			}

			if ((changed)) {
				lcdtp_sendlogs(label[i]);
				lcdtp_sendloguw(envision_i2c_trace[i]);
				lcdtp_sendlogc('\n');
			}
			prev[i] = envision_i2c_trace[i];
		}

		/*
			A coordinate only means anything when the transaction
			actually succeeded, so it is recorded here and nowhere
			else - printed next to a failed read it would look as
			though the read had worked.
		*/
		envision_i2c_trace[16] = 4;
		if ((touched)) {
			touches++;
			lastx = x;
			lasty = y;
			lcdtp_sendlogs("TOUCH x=");
			lcdtp_sendlogdec(x);
			lcdtp_sendlogs(" y=");
			lcdtp_sendlogdec(y);
			lcdtp_sendlogc('\n');

			/*
				and where, so the mapping can be eyeballed -
				but only when the point is on the panel. The
				controller reports twelve bits per axis, so a
				misread gives coordinates in the thousands,
				and those go straight into a framebuffer
				address: not a wrong pixel but a write into
				whatever else lives there.

				Kept as a guard on its own merits. It was
				added believing it explained a program that
				stopped after a few touches, on the strength
				of RAM that read back as a repeating pattern;
				those reads turned out to have been taken of
				a running target and to mean nothing. The
				stopping is real and still unexplained.
			*/
			if (x >= 4 && y >= 4 && x < LCD_W - 4 && y < LCD_H - 4)
				gfil_rec(x - 4, y - 4, x + 4, y + 4, 0xf800);
		}

		/*
			Latched, not live: a coordinate that only shows while a
			finger is down cannot be read by the person whose finger
			it is.
		*/
		envision_i2c_trace[16] = 5;
		gfil_rec(260, 100, 460, 160, 0x0000);
		gdra_stp(260, 112, 0x07ff, 0x0000, NULL, (UB*)"touches");
		dec5(touches, line);
		gdra_stp(350, 112, 0x07ff, 0x0000, NULL, line);
		gdra_stp(260, 126, 0x07ff, 0x0000, NULL, (UB*)"last x");
		dec5(lastx, line);
		gdra_stp(350, 126, 0x07ff, 0x0000, NULL, line);
		gdra_stp(260, 140, 0x07ff, 0x0000, NULL, (UB*)"last y");
		dec5(lasty, line);
		gdra_stp(350, 140, 0x07ff, 0x0000, NULL, line);

		envision_i2c_trace[16] = 6;
		dly_tsk(60);
		envision_i2c_trace[16] = 7;
	}

	return 0;
}
