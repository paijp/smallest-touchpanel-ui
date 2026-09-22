/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	diag4, with the display made trustworthy first.

	diag4 put two labels and two numbers on the screen. The labels appeared
	and the numbers did not, which leaves its whole result unreadable: the
	blind write is known not to fault, because the loop reached the label
	draw that follows it, but whether the loop kept going, and what the
	status register actually held, are both unknown. A test whose output
	cannot be read has not been run.

	Reading the drawing code did not explain it. The value draws differ from
	the label draws in nothing but the buffer handed over - same function,
	same coordinates, same colours - and the same hex2() put numbers on the
	screen in diag1. So this stops reasoning about it and measures it.

	Two changes, each answering one question by itself:

	Every line is one string with letters and digits in it - "stat=0001"
	rather than "stat=" drawn beside "0001". If the letters appear without
	the digits, it is the digits; if nothing appears where diag4's labels
	did, it is the buffer, because the letters are now in that buffer too.

	The loop count is also shown as a letter that advances a..z. Whether the
	loop is still running is the one thing diag4 most needed to report, and
	this reports it without depending on digits at all.

	The blind write is unchanged - it is still the thing under test.
*/

#include	"lcdtp.h"
#include	"dbgcon.h"


static	const	char	*bin2hex = "0123456789abcdef";


/* Append two hex digits, return where the next character goes. */
static	UB	*puthex2(UB *p, UW v)
{
	*(p++) = (UB)bin2hex[(v >> 4) & 0xf];
	*(p++) = (UB)bin2hex[v & 0xf];
	return p;
}


static	UB	*putstr(UB *p, const char *s)
{
	while (*s)
		*(p++) = (UB)*(s++);
	return p;
}


/* Blind. No status read - that is still the point. */
static	void	blindputc(W c)
{
	DBGCON_TX = (UW)(UB)c;
	dly_tsk(100);
}


int	main(void)
{
	static	UB	line[32];
	UB	*p;
	UW	n, stat;

	init_lcdtp();
	dly_tsk(500);
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	gdra_stp(8, 20, 0x07e0, 0x0000, NULL, (UB*)"rx65n debug console, blind write");

	/*
		Read the status word once, before anything has written, and show
		it as part of a sentence rather than on its own. This is the
		value dbgcon.h waits on, and it has not been readable yet:
		through the debugger it can only be read off a running target,
		which comes back fabricated.
	*/
	stat = DBGCON_STAT;
	p = putstr(line, "stat=");
	p = puthex2(p, stat >> 24);
	p = puthex2(p, stat >> 16);
	p = puthex2(p, stat >> 8);
	p = puthex2(p, stat);
	*p = 0;
	gdra_stp(8, 44, 0xffe0, 0x0000, NULL, line);

	for (n = 0; ; n++) {
		p = putstr(line, "sent=");
		p = puthex2(p, n >> 8);
		p = puthex2(p, n);

		/* The same count as a letter, for when the digits do not draw */
		p = putstr(p, " tick=");
		*(p++) = (UB)('a' + (n % 26));
		*p = 0;

		blindputc((W)bin2hex[(n >> 4) & 0xf]);
		blindputc((W)bin2hex[n & 0xf]);
		blindputc('\n');

		gfil_rec(8, 60, 400, 88, 0x0000);
		gdra_stp(8, 74, 0x07ff, 0x0000, NULL, line);

		dly_tsk(500);
	}

	return 0;
}
