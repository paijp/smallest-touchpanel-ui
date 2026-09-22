/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	The RX Debug Virtual Console: a mailbox in the debug SFR space that the
	emulator drains while the target runs.

	This is not semihosting and it is not a breakpoint. The target writes a
	byte to a register and carries on; the debug probe picks it up over the
	same link it uses for everything else. Nothing halts.

	The register layout is Renesas' own, from the RX BSP's lowlvl.c
	(renesas/rx-driver-package, source/r_bsp/.../mcu/all/lowlvl.c), which
	describes the block at 0x84080 as:

		+0x00	tx_data		write a character here
		+0x10	rx_data		read a character from here
		+0x40	dbgstat		flow control

	with bit 8 of dbgstat meaning "transmit side busy, do not write yet" and
	bit 12 meaning "a received character is waiting". charput() there spins on
	bit 8 and then stores; this does the same with a bound on the spin.

	This works on the Envision Kit, and what follows is what was measured
	rather than what was expected - the two differed enough to be worth
	writing down.

	The target side costs nothing and is safe with no debugger present.
	diag5 read dbgstat once and then wrote characters blind, with no status
	check at all, for fifteen minutes: 2748 writes, no fault, still counting.
	dbgstat read back as 0x00000000, so the transmit-busy bit is clear when
	nobody is draining and the spin below falls straight through. Writes
	with no listener simply go nowhere.

	(An earlier version of this comment said the opposite - that touching
	the block faulted, both with and without a debugger. That was measured
	on a build whose .bss began at address 0, so the diagnostic's own output
	buffer was a null pointer and its screen went blank for reasons that had
	nothing to do with these registers. See patch-demo.py. Neither
	measurement said anything about this block.)

	The host side is two monitor commands to e2-server-gdb, which then
	listens on a TCP port and writes what arrives there:

		monitor set_simio_pipe,telnet
		monitor start_interface,TELNET,telnet,5432

	The part that is not obvious: the emulator only drains the mailbox while
	it has execution control. Attached to a target that was already running
	from a flash, with both commands accepted and the port open, nothing
	came out. After `monitor enable_execute_on_connect` - which resets the
	target - and a continue, the same program's output appeared immediately
	and kept coming.

	So this costs a reset to switch on, which the ring buffer in debuglog.h
	does not. What it buys is a stream that does not depend on the gdb
	session staying healthy: the server serves it from its own SimIO thread,
	and it was still delivering after `-exec-continue` had left the MI
	channel unresponsive, which is the failure that has cost the most time
	here. Between the two, debuglog.h is the history read out of a stopped
	target and this is the running commentary.
*/

#ifndef	RX65N_DBGCON_H
#define	RX65N_DBGCON_H

#include	"basic.h"

#define	DBGCON_BASE	0x00084080
#define	DBGCON_TX	(*(volatile UW*)(DBGCON_BASE + 0x00))
#define	DBGCON_RX	(*(volatile UW*)(DBGCON_BASE + 0x10))
#define	DBGCON_STAT	(*(volatile UW*)(DBGCON_BASE + 0x40))

#define	DBGCON_TXBUSY	0x00000100	/* BSP: TXFL0EN */
#define	DBGCON_RXREADY	0x00001000	/* BSP: RXFL0EN */

/*
	How long to wait for the emulator to take the previous byte. Long enough
	that a debugger which is there but busy is not mistaken for one that is
	absent, short enough that a whole line costs a blink rather than a hang.
*/
#define	DBGCON_SPIN	20000

/* Set once the console has failed to drain; nothing tries again after that. */
extern	W	dbgcon_dead;

void	dbgcon_putc(W c);

#endif
