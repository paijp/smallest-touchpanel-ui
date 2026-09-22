/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

#include	"debuglog.h"
#include	"dbgcon.h"


/*
	Starts alive, and stays that way only as long as the console answers. In
	.data rather than .bss for the same reason the buffer is: a reader
	attaching to a running target should not have to care when it started.
*/
W	dbgcon_dead = 0;


/*
	One character into the Debug Virtual Console, or nothing at all.

	Two ways out without writing: the console has already been declared dead,
	or it does not drain within DBGCON_SPIN. The second declares it dead, so
	the cost of running without a debugger is one timeout for the whole run
	rather than one per character.
*/
void	dbgcon_putc(W c)
{
	UW	spin;

	if ((dbgcon_dead))
		return;

	for (spin = 0; (DBGCON_STAT & DBGCON_TXBUSY); spin++) {
		if (spin >= DBGCON_SPIN) {
			dbgcon_dead = 1;
			return;
		}
	}

	DBGCON_TX = (UW)(UB)c;
}


/*
	Not in .bss: the reader wants to attach to a target that is already
	running and find a usable buffer, and .bss is zeroed by the startup code
	at every reset. Giving magic and size initialisers puts the whole thing
	in .data, so the values are already correct before main() runs and a
	reader attaching mid-run never sees a half-set-up header.
*/
volatile struct debuglog_struct	debuglog = {
	DEBUGLOG_MAGIC,
	DEBUGLOG_SIZE,
	0,
	0,
	{0}
};


/*
	The only platform-specific piece of the lcdtp log interface - everything
	from lcdtp_sendlogs() up is shared with the other ports and builds on
	this one byte at a time.

	Order matters. The byte lands before wr advertises it, so a reader that
	catches this mid-call sees the old count and simply comes back for the
	byte next time, rather than reading a slot the writer has not filled in
	yet. There is no lock and none is needed: one writer on the target, one
	reader on the host, and the reader never writes.

	The ring buffer only. dbgcon_putc() was called from here as well, on the
	grounds that the two sinks fail in opposite directions and that the
	console would give up harmlessly when no debugger was listening. The
	second half of that was wrong, and wrong in the way that matters: the
	bound was on how long to *wait* for the mailbox, not on touching it at
	all, and touching it is what faults. Measured both ways - with no
	debugger the program stops dead at the first status read, and under one
	it ends in an exception storm with ISP walked down to zero and PC at
	zero. A log sink that stops the program is worse than no log sink, and
	this one is called from inside the code being debugged.

	So dbgcon.h is kept but not wired in: a program that wants the console
	calls dbgcon_putc() itself, having established that the mailbox is live
	on whatever configuration it runs under. See dbgcon.h for what is known.
*/
void	lcdtp_sendlogc(W c)
{
	UW	w;

	w = debuglog.wr;
	debuglog.buf[w & (DEBUGLOG_SIZE - 1)] = (UB)c;
	debuglog.wr = w + 1;
}
