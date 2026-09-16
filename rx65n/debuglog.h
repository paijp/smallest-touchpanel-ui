/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	Log sink for lcdtp_sendlogc() on RX65N: a ring buffer in RAM that the
	debugger reads while the target keeps running.

	The Envision Kit's E2 Lite has no virtual COM port (its USB descriptor is
	one vendor-specific interface with two bulk endpoints and nothing else),
	so there is no serial line to print to. What it does have is Renesas'
	RRM/DMM - real-time RAM monitoring - which e2-server-gdb exposes with
	-uAllowRRMDMM=1. With that, the host can read target RAM without halting
	the CPU, so a plain ring buffer becomes a live console.

	Nothing here is RX-specific except the memory it lives in; the same sink
	works on any target whose debugger can read RAM while running.

	Reading it: tools/readlog.py, which resolves the symbol from the .elf so
	no address is hardcoded anywhere.
*/

#ifndef	RX65N_DEBUGLOG_H
#define	RX65N_DEBUGLOG_H

#include	"basic.h"

/* Power of two: the writer masks with (size - 1) instead of dividing. */
#define	DEBUGLOG_SIZE	4096

/*
	Laid out so a reader that samples it in one pass can tell whether it
	sampled a torn state.

	`magic` lets the reader confirm it is looking at the buffer at all
	(a stale .elf, or a target that never booted, both show up as garbage).

	`wr` counts bytes ever written and never wraps, so the reader can
	compute how far behind it is and detect that the writer lapped it -
	an index that wrapped would make an overrun look like no progress.
*/
struct	debuglog_struct {
	UW	magic;			/* DEBUGLOG_MAGIC once initialised */
	UW	size;			/* == DEBUGLOG_SIZE, so the reader need not assume */
	UW	wr;			/* total bytes written, free-running */
	UW	pad;			/* keeps buf 16-byte aligned */
	UB	buf[DEBUGLOG_SIZE];
};

#define	DEBUGLOG_MAGIC	0x4C475044	/* "DPGL" */

extern	volatile struct debuglog_struct	debuglog;

#endif
