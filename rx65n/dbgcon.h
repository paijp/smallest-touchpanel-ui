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

	None of which works here yet. Read this before using it.

	On the Envision Kit, driven by e2-server-gdb with the options in
	container/gdbserver.sh, touching this block faults. Measured twice, both
	ways round. With no debugger attached, a program that reads dbgstat
	stops on that instruction: diag5 puts its title on the screen and never
	reaches the line after the read. Under a debugger, with the target reset
	and released, it ends in an exception storm - SIGTRAP at PC 0, with ISP
	walked all the way down to 0, which is what the empty handlers in
	inthandler.c do with a fault that repeats.

	The bound below is therefore not the protection it was written as. It
	bounds how long to *wait* for the mailbox, not whether to touch it, and
	touching it is what faults. dbgcon_putc() is not called from
	lcdtp_sendlogc() for that reason: a log sink that stops the program is
	worse than no log sink, and this one would be called from inside the
	code being debugged. Call it directly, from a program that is prepared
	to crash.

	What has not been ruled out is the server's own configuration.
	gdbserver.sh passes -uDebugMode= 0, and the name is at least suggestive;
	there may simply be a mode in which these registers are live. Until
	someone establishes that, this file is a record of the mechanism and not
	a working channel.

	The host side, for when it does work, is two monitor commands to e2-server-gdb, which then listens
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
