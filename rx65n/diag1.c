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
	"ssr@tdr ", "ssr@end ", "sisr    ", "simr3   ",
	"gaveup  ", "points  ", "int p02 ", "ok count"
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


int	main(void)
{
	static	UB	line[24];
	W	x, y, touched, i;

	init_lcdtp();
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	gdra_stp(8, 20, 0x07e0, 0x0000, NULL, (UB*)"rx65n touch i2c diag");
	lcdtp_sendlogs("diag1 up\n");

	for (;;) {
		x = y = -1;
		touched = envision_touch_get_raw(&x, &y);

		for (i = 0; i < ENVISION_I2C_TRACE_N; i++) {
			gfil_rec(8, 28 + i * 14, 240, 42 + i * 14, 0x0000);
			gdra_stp(8, 40 + i * 14, 0xffff, 0x0000, NULL,
				 (UB*)label[i]);
			hex8(envision_i2c_trace[i], line);
			gdra_stp(88, 40 + i * 14, 0xffe0, 0x0000, NULL, line);

			lcdtp_sendlogs(label[i]);
			lcdtp_sendloguw(envision_i2c_trace[i]);
			lcdtp_sendlogc('\n');
		}

		/*
			Shown separately from the trace: a coordinate only means
			anything when the transaction actually succeeded, and
			printing a stale one next to a failure reads as though it
			had worked.
		*/
		gfil_rec(260, 28, LCD_W, 160, 0x0000);
		if ((touched)) {
			gdra_stp(260, 40, 0x07ff, 0x0000, NULL, (UB*)"touch");
			hex8((UW)x, line);
			gdra_stp(260, 54, 0x07ff, 0x0000, NULL, line);
			hex8((UW)y, line);
			gdra_stp(260, 68, 0x07ff, 0x0000, NULL, line);

			/* and where, so the mapping can be eyeballed */
			gfil_rec(x - 4, y - 4, x + 4, y + 4, 0xf800);
		} else
			gdra_stp(260, 40, 0xf800, 0x0000, NULL, (UB*)"no touch");

		dly_tsk(150);
	}

	return 0;
}
