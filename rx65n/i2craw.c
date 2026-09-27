/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	The touch controller's bytes, exactly as they arrive, on the screen and on
	the serial log. Nothing else.

	This exists to bisect a freeze. Running the UI, the display stopped
	updating shortly after a touch and the event log went silent with it - and
	the same thing had happened before, through gdb, which rules out both the
	debugger and this port's serial log as the cause. So this strips the
	program down to the one thing that must work: put a transaction on the I2C
	bus every 200 ms, print what came back, and count the passes.

	What it deliberately does not do: no gettp(), so no press/pressing state
	machine and no coordinate clamping; no history; no full-screen clears; no
	drawing that depends on where a finger is. If this survives touching the
	panel, the freeze is in what was taken out. If it freezes too, it is in
	envision_touch_get_raw() or below, and the last line of the log says how
	far the transaction got.

	The pass counter is the point of the exercise as much as the bytes are.
	With nothing animated on screen a working program and a stopped one look
	identical, which is exactly the confusion this is here to clear up.

	envision_touch_get_raw() is used rather than envision_touch_get() because
	it skips the power-up gate and always puts a transaction on the bus, so
	the trace is filled even when the gate would never have opened.

	One line per pass:

	    2.400 n=12 ok=12 gaveup=0 step=14 pin=1 int=1 pts=1 nak=0/0 str=0 |
	          01 01 8a 00 ad 00 00 | x=394 y=173
*/

#include	"lcdtp.h"
#include	"envision_hw.h"
#include	"debuglog.h"
#include	"seriallog.h"


#define	PERIOD_MS	200

#define	PAD		6
#define	LINE_H		14
#define	COL_X		PAD
#define	VAL_X		(PAD + 120)

#define	C_BG		0x0000
#define	C_TEXT		0xffff
#define	C_DIM		0x8410
#define	C_OK		0x07e0
#define	C_BAD		0xf800
#define	C_RAW		0xffe0


static	const char	*trace_label[8] = {
	"nak write", "nak read", "stretch", "pin order",
	"gave up at", "points", "int p02", "ok count"
};


static	UB	*put_str(UB *p, const char *s)
{
	while ((*s))
		*p++ = (UB)*s++;
	*p = 0;
	return p;
}


static	UB	*put_dec(UB *p, W v)
{
	UB	tmp[12];
	W	n = 0;

	if (v < 0) {
		*p++ = '-';
		v = -v;
	}
	do {
		tmp[n++] = (UB)('0' + (v % 10));
		v /= 10;
	} while ((v) && n < (W)sizeof(tmp));
	while (n > 0)
		*p++ = tmp[--n];
	*p = 0;
	return p;
}


static	UB	*put_hex2(UB *p, UW v)
{
	static	const char	*bin2hex = "0123456789abcdef";

	*p++ = (UB)bin2hex[(v >> 4) & 0xf];
	*p++ = (UB)bin2hex[v & 0xf];
	*p = 0;
	return p;
}


static	void	draw_row(W row, W x, const UB *s, UW color, W width)
{
	gfil_rec(x, PAD + row * LINE_H, x + width,
		 PAD + row * LINE_H + LINE_H - 1, C_BG);
	gdra_stp(x, PAD + row * LINE_H, color, C_BG, LCDTP_FONT12, s);
}


int	main(void)
{
	UW	pass = 0;
	W	x, y, touched, i;
	UB	buf[96];
	UB	*p;

	envision_clock_init();
	seriallog_init();
	envision_lcd_init();
	envision_touch_init();
	init_lcdtp();

	lcdtp_sendlogs("\r\n--- i2craw: touch controller bytes, every ");
	lcdtp_sendlogdec(PERIOD_MS);
	lcdtp_sendlogs("ms ---\r\n");

	gfil_rec(0, 0, LCD_W - 1, LCD_H - 1, C_BG);
	gdra_stp(COL_X, PAD, C_TEXT, C_BG, LCDTP_FONT12,
		 (const UB*)"i2craw - raw touch bytes");
	for (i = 0; i < 8; i++)
		gdra_stp(COL_X, PAD + (2 + i) * LINE_H, C_DIM, C_BG,
			 LCDTP_FONT12, (const UB*)trace_label[i]);
	gdra_stp(COL_X, PAD + 11 * LINE_H, C_DIM, C_BG, LCDTP_FONT12,
		 (const UB*)"raw bytes");
	gdra_stp(COL_X, PAD + 12 * LINE_H, C_DIM, C_BG, LCDTP_FONT12,
		 (const UB*)"point");
	gdra_stp(COL_X, PAD + 13 * LINE_H, C_DIM, C_BG, LCDTP_FONT12,
		 (const UB*)"pass");

	for (;;) {
		x = -1;
		y = -1;
		touched = envision_touch_get_raw(&x, &y);
		pass++;

		/* --- the screen --- */

		for (i = 0; i < 8; i++) {
			put_dec(buf, (W)envision_i2c_trace[i]);
			draw_row(2 + i, VAL_X, buf,
				 (i == 0 || i == 1 || i == 2 || i == 4)?
					((envision_i2c_trace[i])? C_BAD : C_OK)
					: C_TEXT,
				 90);
		}

		p = buf;
		for (i = 0; i < 7; i++) {
			p = put_hex2(p, envision_i2c_trace[8 + i]);
			*p++ = ' ';
		}
		*p = 0;
		draw_row(11, VAL_X, buf, C_RAW, 170);

		p = put_str(buf, (touched)? "x=" : "(none) x=");
		p = put_dec(p, x);
		p = put_str(p, " y=");
		put_dec(p, y);
		draw_row(12, VAL_X, buf, (touched)? C_OK : C_DIM, 170);

		put_dec(buf, (W)pass);
		draw_row(13, VAL_X, buf, C_TEXT, 90);

		/* --- the log, one line per pass --- */

		p = put_dec(buf, (W)(pass * PERIOD_MS / 1000));
		*p++ = '.';
		{
			UW	frac = (pass * PERIOD_MS) % 1000;

			*p++ = (UB)('0' + frac / 100);
			*p++ = (UB)('0' + (frac / 10) % 10);
			*p++ = (UB)('0' + frac % 10);
		}
		*p = 0;
		lcdtp_sendlogs((const char*)buf);

		lcdtp_sendlogs(" n=");
		lcdtp_sendlogdec((W)pass);
		lcdtp_sendlogs(" ok=");
		lcdtp_sendlogdec((W)envision_i2c_trace[7]);
		lcdtp_sendlogs(" gaveup=");
		lcdtp_sendlogdec((W)envision_i2c_trace[4]);
		lcdtp_sendlogs(" step=");
		lcdtp_sendlogdec((W)envision_i2c_trace[15]);
		lcdtp_sendlogs(" pin=");
		lcdtp_sendlogdec((W)envision_i2c_trace[3]);
		lcdtp_sendlogs(" int=");
		lcdtp_sendlogdec((W)envision_i2c_trace[6]);
		lcdtp_sendlogs(" pts=");
		lcdtp_sendlogdec((W)envision_i2c_trace[5]);
		lcdtp_sendlogs(" nak=");
		lcdtp_sendlogdec((W)envision_i2c_trace[0]);
		lcdtp_sendlogs("/");
		lcdtp_sendlogdec((W)envision_i2c_trace[1]);
		lcdtp_sendlogs(" str=");
		lcdtp_sendlogdec((W)envision_i2c_trace[2]);

		lcdtp_sendlogs(" | ");
		for (i = 0; i < 7; i++) {
			lcdtp_sendlogub(envision_i2c_trace[8 + i]);
			lcdtp_sendlogs(" ");
		}

		lcdtp_sendlogs("| ");
		if ((touched)) {
			lcdtp_sendlogs("x=");
			lcdtp_sendlogdec(x);
			lcdtp_sendlogs(" y=");
			lcdtp_sendlogdec(y);
		} else {
			lcdtp_sendlogs("no contact");
		}
		lcdtp_sendlogs("\r\n");

		dly_tsk(PERIOD_MS);
	}
}
