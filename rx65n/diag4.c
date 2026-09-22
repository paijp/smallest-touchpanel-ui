/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	Does the Debug Virtual Console carry anything at all? Nothing else.

	No I2C, no touch, no ring buffer. A counter and a newline, once a second,
	written straight at the transmit register.

	The flag is deliberately not read. dbgcon.h waits for the emulator to
	clear the transmit-busy bit before each byte, and if that bit never
	behaves as the BSP describes - wrong polarity, not implemented on this
	part, not driven unless the emulator was told to service the console -
	then dbgcon_putc() gives up on its first character and the whole channel
	looks dead, with no way to tell that from the console genuinely not being
	wired up. Two unknowns, one symptom.

	So this removes one of them. 100ms between characters is far longer than
	any plausible drain time, so if the flag is the only thing wrong, this
	works and dbgcon.h does not. If neither works, the flag was never the
	problem and the answer is somewhere else - the monitor commands, the RX
	path through the server, or the mailbox not being live during a normal
	run.

	The count also goes on the screen, so "nothing arrived" can be told from
	"the board is not running".

	Not a general-purpose writer: 100ms a character is ten bytes a second and
	only the first two unknowns are worth that. Once the channel is known to
	work, dbgcon.h is the one to use.
*/

#include	"lcdtp.h"
#include	"dbgcon.h"


static	void	hex2(UW v, UB *p)
{
	static	const	char	*bin2hex = "0123456789abcdef";

	p[0] = (UB)bin2hex[(v >> 4) & 0xf];
	p[1] = (UB)bin2hex[v & 0xf];
}


/* Blind. No status read anywhere - that is the entire point of this file. */
static	void	blindputc(W c)
{
	DBGCON_TX = (UW)(UB)c;
	dly_tsk(100);
}


int	main(void)
{
	static	UB	line[16];
	UW	n;

	init_lcdtp();
	dly_tsk(500);
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	gdra_stp(8, 20, 0x07e0, 0x0000, NULL, (UB*)"rx65n debug console, blind write");

	/*
		Show the status word once. If the console is not serviced at all
		this is what dbgcon.h would have been waiting on, and knowing its
		value is most of the answer either way.
	*/
	hex2(DBGCON_STAT >> 8, line);
	hex2(DBGCON_STAT, line + 2);
	line[4] = 0;
	gdra_stp(8, 40, 0xffff, 0x0000, NULL, (UB*)"dbgstat");
	gdra_stp(120, 40, 0xffe0, 0x0000, NULL, line);

	for (n = 0; ; n++) {
		hex2(n >> 8, line);
		hex2(n, line + 2);
		line[4] = 0;

		blindputc(line[0]);
		blindputc(line[1]);
		blindputc(line[2]);
		blindputc(line[3]);
		blindputc('\n');

		gfil_rec(8, 60, 400, 88, 0x0000);
		gdra_stp(8, 74, 0x07ff, 0x0000, NULL, (UB*)"sent");
		gdra_stp(120, 74, 0x07ff, 0x0000, NULL, line);

		dly_tsk(500);
	}

	return 0;
}
