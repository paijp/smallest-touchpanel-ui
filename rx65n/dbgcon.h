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

	The bound is the whole difference, and it is not optional. Bit 8 is
	cleared by the emulator, so with no emulator attached - or with one
	attached but not told to service the console - it never clears and
	upstream's charput() spins forever. On a board whose only output is the
	screen that is indistinguishable from the freeze we are chasing, which is
	precisely the wrong thing to introduce into the program doing the
	chasing. So: give up after a while, and having given up once, stop
	trying. A program built with this in it runs the same with or without a
	debugger.

	The host side is two monitor commands to e2-server-gdb, which then listens
	on a TCP port and writes what arrives there:

		monitor set_simio_pipe,telnet
		monitor start_interface,TELNET,telnet,5432

	Both are present in the e2-server-gdb we have (checked in the binary's
	command table, alongside get_interface_port). Once started, the stream is
	served by the server's own SimIO thread, so reading it is a socket the
	host opens - not something driven through the gdb session, and not
	something that goes wrong when that session does.
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
