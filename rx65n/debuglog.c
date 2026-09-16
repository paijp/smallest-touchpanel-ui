/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

#include	"debuglog.h"


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
*/
void	lcdtp_sendlogc(W c)
{
	UW	w;

	w = debuglog.wr;
	debuglog.buf[w & (DEBUGLOG_SIZE - 1)] = (UB)c;
	debuglog.wr = w + 1;
}
