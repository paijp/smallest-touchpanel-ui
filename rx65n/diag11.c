/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	I2C, the display and the console, each switchable on its own, to find
	which combination the fault needs.

	  -DDIAG11_LCD=0|1   1: init_lcdtp() and a counter drawn every pass.
	                     0: clock only, the display never switched on.
	  -DDIAG11_CON=0|1   the debug console, written blind (no status read,
	                     100ms a character, as diag5 and diag10 do).
	  -DDIAG11_SKIP=n    the first n passes print nothing - the I2C read
	                     still happens and the time spent is the same - and
	                     printing starts at pass n.

	Every pass reads the touch controller's seven bytes (the transaction
	diag10 level 2 faults in) and, when printing, sends "nnnn bb\n": the
	pass number and the first byte read.

	The pass number is also kept in diag11_n, so a run that stops can be
	asked how far it got even when nothing was printed.

	The timing of a pass is the same whether it prints or not: a pass that
	does not print waits the 800ms the eight characters would have taken.
	Then SKIP separates "the console has been written" from "this much time
	has passed", which is the question.
*/

#include	"basic.h"
#include	"envision_hw.h"
#include	"lcdtp.h"
#include	"dbgcon.h"
#include	"i2c.h"


#ifndef	DIAG11_LCD
#define	DIAG11_LCD	0
#endif
#ifndef	DIAG11_CON
#define	DIAG11_CON	1
#endif
#ifndef	DIAG11_SKIP
#define	DIAG11_SKIP	0
#endif

#define	ADDR	0x38		/* FT5x06 */


static	const	char	*bin2hex = "0123456789abcdef";

volatile	UW	diag11_n = 0;


static	void	wait_ms(UW ms)
{
#if	DIAG11_LCD
	dly_tsk(ms);
#else
	envision_delay_ms(ms);
#endif
}


static	void	blindputc(W c)
{
#if	DIAG11_CON
	DBGCON_TX = (UW)(UB)c;
#else
	(void)c;
#endif
	wait_ms(100);
}


/* diag10's transaction, unchanged. */
static	W	read7(UB *buf)
{
	W	i;

	i2cstart();
	if (i2csend(ADDR << 1)) {
		i2cstop();
		return 0;
	}
	if (i2csend(2)) {
		i2cstop();
		return 0;
	}

	i2cstart();
	if (i2csend((ADDR << 1) | 1)) {
		i2cstop();
		return 0;
	}

	for (i = 0; i < 7; i++)
		buf[i] = (UB)i2crecv((i == 6)? 1 : 0);

	i2cstop();
	return 1;
}


int	main(void)
{
	static	UB	buf[7];
	UW	n;
#if	DIAG11_LCD
	static	UB	line[16];
	W	i;
#endif

#if	DIAG11_LCD
	init_lcdtp();
	dly_tsk(500);
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	gdra_stp(8, 20, 0x07e0, 0x0000, NULL, (UB*)"diag11");
#else
	envision_clock_init();
#endif

	blindputc('b');
	blindputc((W)bin2hex[DIAG11_LCD]);
	blindputc((W)bin2hex[DIAG11_CON]);
	blindputc('\n');

	(void)i2cprobe(ADDR);

	for (n = 0; ; n++) {
		diag11_n = n;
		(void)read7(buf);

		if (n >= DIAG11_SKIP) {
			blindputc((W)bin2hex[(n >> 12) & 0xf]);
			blindputc((W)bin2hex[(n >> 8) & 0xf]);
			blindputc((W)bin2hex[(n >> 4) & 0xf]);
			blindputc((W)bin2hex[n & 0xf]);
			blindputc(' ');
			blindputc((W)bin2hex[(buf[0] >> 4) & 0xf]);
			blindputc((W)bin2hex[buf[0] & 0xf]);
			blindputc('\n');
		} else
			wait_ms(800);

#if	DIAG11_LCD
		for (i = 0; i < 4; i++)
			line[i] = (UB)bin2hex[(n >> (12 - i * 4)) & 0xf];
		line[4] = 0;
		gfil_rec(8, 60, 400, 88, 0x0000);
		gdra_stp(8, 74, 0x07ff, 0x0000, NULL, line);
#endif

		wait_ms(200);
	}

	return 0;
}
