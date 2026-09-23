/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	diag3 turned into diag7 one property at a time.

	diag3 survives touching and survived five consecutive resets. diag7
	stops, usually in seconds. Everything tried so far has gone the other
	way - taking things out of the failing program - and twice that produced
	an answer that meant nothing, because removing the part also removed the
	work: taking out i2cprobe also took out i2cinit, so the bus was never
	set up and "the fault stopped" only said the program had stopped doing
	I2C. Adding to something that works cannot fail that way. Each step here
	changes one property from diag3's value to diag7's, and nothing else.

	Build with -DDIAG9_STEP=n:

	  1  diag3 as it stands: 200ms a pass, its own read7(), a log record
	     per pass, two rows drawn. This is the baseline and it is known
	     good - if it ever stops, nothing below is worth running until
	     that is understood.

	  2  the pass interval drops to 60ms. Nothing else changes.

	     This is first because it is the largest untested difference
	     between the two programs and the cheapest to undo. diag7 drives
	     the bus three times as often, and bus activity is the one thing
	     the fault has always needed.

	  3  the transaction comes from envision_hw.c - envision_touch_get_raw()
	     rather than read7(). The bus traffic is the same sequence either
	     way; what changes is which translation unit's copy of i2c.h's
	     statics is used, and that the trace array is written throughout.

	  4  the log record goes away, as in diag7.

	     Worth a step of its own rather than an afterthought. It is the
	     only thing in the baseline that touches memory heavily every pass,
	     and it is the thing least expected to matter, which is a reason to
	     check rather than a reason to skip.

	At step 4 this is diag7 in all but name. If it stops at step n and not
	at n-1, the property changed at n is the one to look at. If it never
	stops, diag7 differs in something not listed here, and that list is then
	short enough to read.

	The screen shows the step number, so a build cannot be mistaken for
	another once it is on the board.
*/

/*
	-DDIAG9_CONSOLE=1 also streams to the debug console, which is the only
	sink that can be read while the program runs. The ring buffer needs the
	target stopped, so it cannot answer "is it still going" without ending
	the run that question is about.

	It is a separate axis from the step number on purpose. The one piece of
	evidence against the console is a diag3 run that stopped after five
	records at about 110 bytes a second, and it was never settled whether
	that was the console's doing or the fault that stops everything else
	here. Keeping it independent makes step 1 with and against the console
	a single experiment that answers it.

	The rate is kept well under that number. A full record goes out only
	when the seven bytes change - which they do not, untouched - and
	otherwise one short line a second says the loop is turning. Untouched
	that is about six bytes a second; a finger makes it burst and stop
	again.
*/
#ifndef	DIAG9_CONSOLE
#define	DIAG9_CONSOLE	1
#endif

/*
	-DDIAG9_CONSOLE_OFF_AT_RUNTIME=1 compiles every line of the console in
	and then never switches it on.

	Step 1 with the console failed three runs out of three and without it
	passed, which looked like the console's doing - until one of the three
	stopped inside i2cprobe, before the console had been enabled at all.
	Whatever stopped it, it was not a console write. What the two builds
	also differ in is simply the code: how much there is and where it lands.
	This build has the console build's code and layout and none of its
	activity, so it says which of the two the difference was.
*/
#ifndef	DIAG9_CONSOLE_OFF_AT_RUNTIME
#define	DIAG9_CONSOLE_OFF_AT_RUNTIME	0
#endif

#include	"lcdtp.h"
#include	"envision_hw.h"
#include	"debuglog.h"
#include	"i2c.h"
#if	DIAG9_CONSOLE
#include	"dbgcon.h"
#endif


#ifndef	DIAG9_STEP
#define	DIAG9_STEP	1
#endif

#if	DIAG9_STEP >= 2
#define	PASS_MS		60
#else
#define	PASS_MS		200
#endif

#define	ADDR		0x38		/* FT5x06 */


static	const	char	*bin2hex = "0123456789abcdef";


static	void	hex2(UW v, UB *p)
{
	p[0] = (UB)bin2hex[(v >> 4) & 0xf];
	p[1] = (UB)bin2hex[v & 0xf];
}


#if	DIAG9_STEP < 4
static	void	loghex2(UW v)
{
	lcdtp_sendlogc(bin2hex[(v >> 4) & 0xf]);
	lcdtp_sendlogc(bin2hex[v & 0xf]);
}
#endif


#if	DIAG9_STEP < 3
/*
	diag3's own transaction, in diag3's own translation unit - which is the
	point of it being here rather than shared. i2c.h's statics are per
	file, so this uses the i2c_swap that the i2cprobe() call in main()
	settled, not envision_hw.c's.
*/
static	W	read7(UB *buf)
{
	W	i;

	i2cstart();
	if (i2csend(ADDR << 1)) {
		i2cstop();
		return 0;
	}
	if (i2csend(2)) {
		i2cstop();
		return 0;
	}

	i2cstart();
	if (i2csend((ADDR << 1) | 1)) {
		i2cstop();
		return 0;
	}

	for (i = 0; i < 7; i++)
		buf[i] = (UB)i2crecv((i == 6)? 1 : 0);

	i2cstop();
	return 1;
}
#endif


#if	DIAG9_CONSOLE
static	void	conhex2(UW v)
{
	dbgcon_putc((W)bin2hex[(v >> 4) & 0xf]);
	dbgcon_putc((W)bin2hex[v & 0xf]);
}


/*
	One line out of the console: the sequence number, the seven bytes, and
	whether the transaction answered. Only called when there is something
	to say, which is what keeps the rate down.
*/
static	void	conrecord(UW seq, const UB *buf, W ok)
{
	W	i;

	conhex2(seq >> 8);
	conhex2(seq);
	dbgcon_putc(' ');
	for (i = 0; i < 7; i++)
		conhex2(buf[i]);
	dbgcon_putc(' ');
	dbgcon_putc((ok)? 'o' : 'x');
	dbgcon_putc('\n');
}
#endif


int	main(void)
{
	static	UB	line[32];
	static	UB	buf[7];
	static	UW	seq = 0;
	W	i, ok;
#if	DIAG9_CONSOLE
	static	UB	prev[7];
	static	UW	lastbeat = 0;
	W	changed;
#endif
#if	DIAG9_STEP >= 3
	W	x = 0, y = 0;
#endif

	init_lcdtp();
	dly_tsk(500);
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	gdra_stp(8, 20, 0x07e0, 0x0000, NULL, (UB*)"diag9 step");
	hex2(DIAG9_STEP, line);
	line[2] = 0;
	gdra_stp(140, 20, 0x07e0, 0x0000, NULL, line);

#if	DIAG9_STEP < 3
	(void)i2cprobe(ADDR);
#endif
	lcdtp_sendlogs("diag9 up\n");

#if	DIAG9_CONSOLE
	dbgcon_enable = !DIAG9_CONSOLE_OFF_AT_RUNTIME;
	/*
		Name the build on the way out. A capture that does not say which
		step produced it is worth very little a day later, and this is
		the one line that is certain to be in every capture.
	*/
	dbgcon_putc('s');
	conhex2(DIAG9_STEP);
	dbgcon_putc('\n');
	for (i = 0; i < 7; i++)
		prev[i] = 0;
#endif

	for (;;) {
#if	DIAG9_STEP >= 3
		ok = envision_touch_get_raw(&x, &y);
		for (i = 0; i < 7; i++)
			buf[i] = (UB)envision_i2c_trace[8 + i];
#else
		ok = read7(buf);
#endif
		seq++;

#if	DIAG9_STEP < 4
		loghex2(seq >> 8);
		loghex2(seq);
		lcdtp_sendlogc(' ');
		for (i = 0; i < 7; i++)
			loghex2(buf[i]);
		lcdtp_sendlogc(' ');
		lcdtp_sendlogc((ok)? 'o' : 'x');
		lcdtp_sendlogc('\n');
#else
#if	!DIAG9_CONSOLE
		(void)ok;
#endif
#endif

#if	DIAG9_CONSOLE
		/*
			Say something when the data changes, and otherwise once a
			second so that silence means stopped rather than idle.
			The distinction is the whole reason this sink exists.
		*/
		changed = 0;
		for (i = 0; i < 7; i++)
			if (buf[i] != prev[i])
				changed = 1;

		if ((changed) || seq - lastbeat >= (1000 / PASS_MS)) {
			conrecord(seq, buf, ok);
			lastbeat = seq;
			for (i = 0; i < 7; i++)
				prev[i] = buf[i];
		}
#endif

		for (i = 0; i < 7; i++) {
			hex2(buf[i], line + i * 3);
			line[i * 3 + 2] = ' ';
		}
		line[20] = 0;
		gfil_rec(8, 46, 400, 74, 0x0000);
		gdra_stp(8, 60, 0xffe0, 0x0000, NULL, line);

		hex2(seq >> 8, line);
		hex2(seq, line + 2);
		line[4] = 0;
		gfil_rec(8, 74, 400, 102, 0x0000);
		gdra_stp(8, 88, 0x07ff, 0x0000, NULL, (UB*)"seq");
		gdra_stp(120, 88, 0x07ff, 0x0000, NULL, line);

		dly_tsk(PASS_MS);
	}

	return 0;
}
