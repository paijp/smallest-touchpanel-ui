/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	Print a number that is already known, and see whether it appears.

	diag4 drew two labels and two numbers. The labels appeared, the numbers
	did not, and that was treated as a side issue while the debug console was
	being chased. It was not a side issue: it means diag4 could not report
	its own result, and every conclusion drawn from what was on its screen
	rested on a display that was demonstrably dropping half of what it was
	asked to draw.

	So this draws 0x12345678 - a constant, no hardware read anywhere, and
	nothing here touches the debug SFRs - and asks why it would not appear.

	Three ways of turning a nibble into a character, because the difference
	between them is where the suspicion is:

	  ptr   a `static const char *` pointing at a literal, which is what
	        diag4 used. The pointer itself lives in .data and is set up by
	        the startup code copying .data out of ROM; if that copy is wrong
	        the pointer is garbage and every digit with it.
	  arr   a `static const char []`, which is the literal itself rather
	        than a pointer to it, and needs no pointer to be relocated.
	  ari   no table at all, just '0' + d and 'a' + d - 10.

	If ari draws and the other two do not, it is the table. If ptr alone
	fails, it is the pointer, and .data is the thing to look at. If all three
	draw, hex printing was never the problem and diag4 failed for another
	reason - in which case the fourth line below is the next suspect.

	That fourth line is the x coordinate. In diag4 the two draws that
	appeared were both at x=8 and the two that did not were both at x=120,
	which is a coincidence worth one line to rule out. The same string is
	drawn at both.

	The last line advances a letter, so that a frozen screen can be told
	from a screen that is merely wrong.
*/

#include	"lcdtp.h"


static	const	char	*hexptr = "0123456789abcdef";
static	const	char	hexarr[] = "0123456789abcdef";


static	UB	*putstr(UB *p, const char *s)
{
	while (*s)
		*(p++) = (UB)*(s++);
	return p;
}


/* mode: 0 = pointer table, 1 = array table, 2 = arithmetic */
static	UB	*putword(UB *p, UW v, W mode)
{
	W	i, d;

	for (i = 28; i >= 0; i -= 4) {
		d = (W)((v >> i) & 0xf);
		if (mode == 0)
			*(p++) = (UB)hexptr[d];
		else if (mode == 1)
			*(p++) = (UB)hexarr[d];
		else
			*(p++) = (UB)((d < 10)? ('0' + d) : ('a' + d - 10));
	}
	return p;
}


static	void	row(W x, W y, const char *tag, UW v, W mode)
{
	static	UB	line[32];
	UB	*p;

	p = putstr(line, tag);
	p = putword(p, v, mode);
	*p = 0;
	gdra_stp(x, y, 0xffe0, 0x0000, NULL, line);
}


int	main(void)
{
	static	UB	line[32];
	UB	*p;
	UW	n;

	init_lcdtp();
	dly_tsk(500);
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	gdra_stp(8, 20, 0x07e0, 0x0000, NULL, (UB*)"rx65n hex print test");

	/* The three tables, all printing the same known constant */
	row(8, 44, "ptr=", 0x12345678, 0);
	row(8, 68, "arr=", 0x12345678, 1);
	row(8, 92, "ari=", 0x12345678, 2);

	/* Same string, two x positions - diag4's failures were all at 120 */
	gdra_stp(8, 116, 0xffff, 0x0000, NULL, (UB*)"x8/x120:");
	row(120, 116, "", 0x12345678, 2);

	for (n = 0; ; n++) {
		p = putstr(line, "tick=");
		*(p++) = (UB)('a' + (n % 26));
		*p = 0;
		gfil_rec(8, 130, 400, 158, 0x0000);
		gdra_stp(8, 144, 0x07ff, 0x0000, NULL, line);
		dly_tsk(500);
	}

	return 0;
}
