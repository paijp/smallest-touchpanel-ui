/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	diag3's body with diag1's touch path. One difference, one answer.

	diag3 calls i2c.h directly and survives touching. diag1 goes through
	envision_hw.c and stops after about three touches. Those two differ in
	more than one way, so neither of them says where the fault is:

	  the I2C path      diag3 writes its own transaction; diag1 calls
	                    envision_touch_get_raw(), which calls touch_read()
	  what happens next envision_touch_get_raw() unpacks coordinates
	  the drawing       diag3 draws two rows; diag1 draws seventeen, clears
	                    a rectangle for each, and pushes several hundred
	                    bytes through the log whenever the trace changes

	touch_read() and diag3's read7() turn out to be the same transaction -
	same address frames, same repeated start, same NAK on the last byte -
	so the bus traffic is not the difference. That leaves the code around
	it and the volume of work per pass.

	This takes diag1's side of the first two and diag3's side of the third:
	envision_touch_get_raw() for the reading, two rows for the drawing, no
	logging at all. If it stops after a few touches, the fault is in
	envision_hw.c and the display has nothing to do with it. If it does not,
	the reading is fine and what remains to explain diag1 is the seventeen
	rows and the log.
*/

#include	"lcdtp.h"
#include	"envision_hw.h"


static	void	hex4(UW v, UB *p)
{
	static	const	char	*bin2hex = "0123456789abcdef";
	W	i;

	for (i = 0; i < 4; i++)
		p[i] = (UB)bin2hex[(v >> ((3 - i) * 4)) & 0xf];
}


static	UB	*putstr(UB *p, const char *s)
{
	while (*s)
		*(p++) = (UB)*(s++);
	return p;
}


int	main(void)
{
	static	UB	line[48];
	UB	*p;
	UW	polls = 0, touches = 0;
	W	x = 0, y = 0, lastx = 0, lasty = 0;

	init_lcdtp();
	envision_touch_init();
	dly_tsk(500);
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	gdra_stp(8, 20, 0x07e0, 0x0000, NULL,
		 (UB*)"envision touch path, minimal display");

	for (;;) {
		polls++;
		if ((envision_touch_get_raw(&x, &y))) {
			touches++;
			lastx = x;
			lasty = y;
		}

		/*
			One line, rewritten in place. Everything diag1 puts on
			the screen is diagnostic; what is needed here is only
			enough to see that the loop is still turning and that
			touches are being counted.
		*/
		p = putstr(line, "poll=");
		hex4(polls, p);
		p = putstr(p + 4, " touch=");
		hex4(touches, p);
		p = putstr(p + 4, " x=");
		hex4((UW)lastx, p);
		p = putstr(p + 4, " y=");
		hex4((UW)lasty, p);
		p += 4;
		*p = 0;

		gfil_rec(8, 46, 460, 74, 0x0000);
		gdra_stp(8, 60, 0xffe0, 0x0000, NULL, line);

		dly_tsk(60);
	}

	return 0;
}
