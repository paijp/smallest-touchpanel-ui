/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	SCI1 on P26/P30, which is where the on-board E2 Lite is wired. See
	seriallog.h for why that is the interesting port and what reads it.
*/

#include	"iodefine.h"
#include	"seriallog.h"

/*
	envision_clock_init() leaves PCLKB at 60 MHz. With SEMR.ABCS set the bit
	rate is PCLKB / (16 * (BRR + 1)):

	  BRR 29 -> 125000 exactly
	  BRR 32 -> 113636, which is 1.5% under 115385

	Written as arithmetic rather than a table so a different PCLKB is one
	constant away, but checked at compile time, because a silently wrong
	divisor is a garbled log rather than an error.
*/
#define	SERIALLOG_PCLKB	60000000UL
#define	SERIALLOG_BRR	((SERIALLOG_PCLKB / (16UL * SERIALLOG_BAUD)) - 1UL)

#if	SERIALLOG_BRR > 255
#error	"SERIALLOG_BAUD is too low for a single-byte BRR at this PCLKB"
#endif

/* MPC pin function select for SCI1's TXD/RXD on this part. */
#define	SERIALLOG_PSEL	0x0a

/*
	Nothing happens until init has run. lcdtp_sendlogc() calls into here for
	every program, including the ones that never wanted a serial log, and
	writing to SCI1 while it is still in module stop would spin on a TDRE that
	never arrives - a hang, in a log call, which is the worst place for one.
*/
static	W	seriallog_ready = 0;


void	seriallog_init(void)
{
	SYSTEM.PRCR.WORD = 0xa50b;
	MSTP_SCI1 = 0;
	SYSTEM.PRCR.WORD = 0xa500;

	SCI1.SCR.BYTE = 0x00;
	SCI1.SMR.BYTE = 0x00;			/* 8N1, PCLK/1 */
	SCI1.BRR = (UB)SERIALLOG_BRR;
	SCI1.SEMR.BYTE = 0x10;			/* ABCS: 16 clocks per bit */

	MPC.PWPR.BYTE = 0x00;			/* clear PFSWE protection */
	MPC.PWPR.BYTE = 0x40;			/* PFS writable */
	MPC.P26PFS.BYTE = SERIALLOG_PSEL;	/* TXD1 */
	MPC.P30PFS.BYTE = SERIALLOG_PSEL;	/* RXD1 */
	MPC.PWPR.BYTE = 0x80;
	PORT2.PMR.BIT.B6 = 1;
	PORT3.PMR.BIT.B0 = 1;

	SCI1.SCR.BYTE = 0x30;			/* TE and RE */

	seriallog_ready = 1;
}


void	seriallog_putc(W c)
{
	if (!(seriallog_ready))
		return;
	while (SCI1.SSR.BIT.TDRE == 0)
		;
	SCI1.TDR = (UB)c;
}


W	seriallog_getc(void)
{
	if (!(seriallog_ready))
		return -1;

	/*
		A framing or overrun error latches and stops reception until it is
		cleared, so clearing it here is what keeps a log readable after the
		host has been restarted mid-byte.
	*/
	if ((SCI1.SSR.BYTE & 0x38)) {
		SCI1.SSR.BYTE = (UB)(SCI1.SSR.BYTE & ~0x38);
		return -1;
	}
	if (SCI1.SSR.BIT.RDRF == 0)
		return -1;
	return SCI1.RDR;
}
