/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

#include	"debuglog.h"
#include	"dbgcon.h"
#include	"seriallog.h"


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
	The only platform-specific piece of the lcdtp log interface - everything
	from lcdtp_sendlogs() up is shared with the other ports and builds on
	this one byte at a time.

	Two sinks, and they fail in opposite directions. The console streams live
	and unboundedly but keeps nothing, needs a reset to switch on, and drops
	everything written before the host opened the socket. The board's own
	serial port is live and unbounded too, needs no debugger, and is the only
	sink here whose host side is not the vendor's - so it is the one to reach
	for first. It stays quiet until seriallog_init() has run, which is what
	keeps this call safe in the programs that never set it up.

	There used to be a third: a 4 KB ring buffer in RAM, read back through
	gdb's RRM/DMM with the target stopped. That was the path that would not
	hold still, and once the serial port worked nothing read the buffer any
	more, so it has gone rather than cost 4 KB of .data and a write per
	character for nobody.

	The console was briefly taken out of here, on a measurement that said
	touching its registers faulted. That measurement was made on a build
	whose .bss started at address 0 and was about a null pointer, not about
	these registers; re-measured, a program can write the mailbox blind for
	fifteen minutes with no debugger attached and not miss a beat. See
	dbgcon.h.
*/
void	lcdtp_sendlogc(W c)
{
	dbgcon_putc(c);
	seriallog_putc(c);
}
