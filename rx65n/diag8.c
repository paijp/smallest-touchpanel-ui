/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	The same touch polling, with the display never switched on.

	Everything that has faulted so far has had GLCDC running behind it, and
	GLCDC is not a passive thing to have running. It is a bus master reading
	the whole framebuffer for every frame: 480 x 272 x 2 bytes, sixty times
	a second, about fifteen megabytes a second of traffic against the same
	memory the processor is fetching and storing through. That has never
	been on the list of suspects, because until the debug console worked
	there was no way to run anything without the screen and still see what
	it did.

	Now there is. This initialises the clock and the touch controller,
	polls it, and says what happened one line at a time down the console.
	envision_lcd_init() is never called, GR2FLM2 is never set, and nothing
	writes to the framebuffer.

	What the outcome means:

	  faults anyway  GLCDC is innocent and the fault is in the processor,
	                 the flash, or the I2C code - and this is now the
	                 smallest program that shows it, which makes everything
	                 after this cheaper.

	  runs clean     the display is part of it. That would fit the shape of
	                 the thing: intermittent, timing-dependent, present in
	                 every diagnostic regardless of what else it did, and
	                 in upstream's own demo as well.

	The rate is deliberately low. 110 bytes a second through the console
	stopped a board that four bytes a second ran fifteen minutes on, so this
	prints one short line every 500ms and nothing else.
*/

#include	"basic.h"
#include	"envision_hw.h"
#include	"lcdtp.h"
#include	"dbgcon.h"


static	const	char	*bin2hex = "0123456789abcdef";


static	void	puthex2(UW v)
{
	dbgcon_putc((W)bin2hex[(v >> 4) & 0xf]);
	dbgcon_putc((W)bin2hex[v & 0xf]);
}


static	void	puts_(const char *s)
{
	while (*s)
		dbgcon_putc((W)*(s++));
}


int	main(void)
{
	UW	polls, touches;
	W	x, y;

	envision_clock_init();
	envision_touch_init();

	dbgcon_enable = 1;
	puts_("diag8 up, no lcd\n");

	x = 0;
	y = 0;
	touches = 0;

	for (polls = 0; ; polls++) {
		if ((envision_touch_get_raw(&x, &y)))
			touches++;

		/*
			Sequence first, so a gap in the numbers says a pass was
			missed rather than leaving it to be guessed at.
		*/
		puthex2(polls >> 8);
		puthex2(polls);
		dbgcon_putc(' ');
		puthex2(touches);
		dbgcon_putc(' ');
		puthex2((UW)x >> 8);
		puthex2((UW)x);
		dbgcon_putc(' ');
		puthex2((UW)y >> 8);
		puthex2((UW)y);
		dbgcon_putc('\n');

		envision_delay_ms(500);
	}

	return 0;
}
