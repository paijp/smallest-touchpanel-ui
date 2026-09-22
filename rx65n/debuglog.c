/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

#include	"debuglog.h"
#include	"dbgcon.h"


/*
	How many characters were dropped because the console did not drain in
	time. Readable with the debugger, and the difference between "the
	program stopped" and "the log stopped", which are not the same thing and
	were confused for each other here more than once.
*/
W	dbgcon_dropped = 0;

/* Opt-in; see dbgcon.h for why it is not on by default. */
W	dbgcon_enable = 0;


/*
	One character into the Debug Virtual Console, or none.

	A character that cannot be sent is dropped and counted; the next one
	tries again. The first version latched instead - one timeout and the
	console was off for the rest of the run - which was meant to keep the
	cost of running without a debugger to a single timeout. It also meant
	that a consumer which was merely slow for a moment silenced the log
	permanently, and it did: diag8 printed its banner and nothing else,
	which read as a program that had stopped and was a log that had given
	up. That cost a run and nearly cost a wrong conclusion.

	Dropping is the right failure here. The log is a diagnostic; a gap in it
	is information, and the counter says how big the gap was. Latching threw
	away everything after the first hiccup and said nothing about it.
*/
void	dbgcon_putc(W c)
{
	UW	spin;

	if (!dbgcon_enable)
		return;

	for (spin = 0; (DBGCON_STAT & DBGCON_TXBUSY); spin++) {
		if (spin >= DBGCON_SPIN) {
			dbgcon_dropped++;
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

	Both sinks, because they fail in opposite directions. The ring buffer
	keeps history but can only be read with the target stopped, and it holds
	minutes at best. The console streams live and unboundedly but keeps
	nothing, needs a reset to switch on, and drops everything written before
	the host opened the socket. Writing to both means the last few minutes
	are recoverable after a freeze and the run up to it was watchable as it
	happened.

	The console was briefly taken out of here, on a measurement that said
	touching its registers faulted. That measurement was made on a build
	whose .bss started at address 0 and was about a null pointer, not about
	these registers; re-measured, a program can write the mailbox blind for
	fifteen minutes with no debugger attached and not miss a beat. See
	dbgcon.h.
*/
void	lcdtp_sendlogc(W c)
{
	UW	w;

	w = debuglog.wr;
	debuglog.buf[w & (DEBUGLOG_SIZE - 1)] = (UB)c;
	debuglog.wr = w + 1;

	dbgcon_putc(c);
}
