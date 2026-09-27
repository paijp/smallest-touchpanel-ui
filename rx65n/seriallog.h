/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	Log sink that writes out of the board's USB cable.

	The Envision Kit has no UART the host can reach, which is why debuglog.c
	exists at all: a ring buffer read by a debugger, because there seemed to
	be nowhere else for characters to go. There is somewhere else. The E2 Lite
	on the board is wired to the MCU's boot-mode SCI - SCI1, TXD1 on P26 and
	RXD1 on P30 - and its USB command that carries those bytes keeps working
	after boot mode has ended. So user code that drives SCI1 is read by
	whatever is holding the probe.

	Nothing on the board has to change and no adapter is added. It needs a
	host that talks to the E2 Lite directly rather than through the vendor
	tools; open-rfp-monitor does that:

	    rxflash.py write lcdtp.mot --log

	Compared with debuglog.c this is live rather than history, and it does not
	need the debugger at all - which matters because that path, through
	e2-server-gdb and RRM/DMM, is the one that would not hold still.

	The cost is that transmission is blocking: put_char() spins until the
	transmit register is free, so logging inside a tight loop paces that loop
	at the baud rate. That is a feature when reading a trace and a hazard
	inside anything timing-critical, so keep it out of interrupt handlers.

	SERIALLOG_BAUD picks the rate at build time. Only rates the probe can
	itself be set to are useful, since it divides a 3 MHz reference:

	  115200   BRR 32 -> 113636 bps, 1.5% under the probe's 115385. Inside the
	           4% either way that the probe was measured to tolerate, and it
	           is what open-rfp-monitor defaults to.
	  125000   BRR 29 -> exactly 125000, which is exactly 3e6/24. Nothing is
	           approximate on either side; use it if a run ever looks
	           marginal.

	Both assume PCLKB at 60 MHz, which is what envision_clock_init() leaves
	behind (12 MHz main clock, PLL x20 to 240 MHz, PCLKB /4).
*/

#ifndef	RX65N_SERIALLOG_H
#define	RX65N_SERIALLOG_H

#include	"basic.h"

#ifndef	SERIALLOG_BAUD
#define	SERIALLOG_BAUD	115200
#endif

/*
	Brings SCI1 up for transmit and receive. Call it after
	envision_clock_init(), because the bit rate is derived from PCLKB.
*/
void	seriallog_init(void);

/* Blocks until the character is handed to the transmitter. */
void	seriallog_putc(W c);

/*
	Returns the next received byte, or -1 when none has arrived. Receive
	errors are cleared and reported as -1; a log that is only ever written
	does not have to call this, and nothing here requires a host to be
	listening.
*/
W	seriallog_getc(void);

#endif
