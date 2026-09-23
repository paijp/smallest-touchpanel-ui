/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	diag5, with diag9's work added back one piece at a time.

	diag9 was meant to be the known-good starting point and was not: its
	first step faulted every time it had the console compiled in. diag5
	does run - under the unattended chain, with the fault handlers in, it
	counted for a minute and never stopped - so this starts there instead
	and adds the things diag9 does that diag5 does not, in order:

	  -DDIAG10_ADD=0  diag5 as it is: count, and write the count to the
	                  debug console one character every 100ms
	               1  + i2cprobe() once, from this file
	               2  + read7() every pass - the I2C traffic - and the
	                  first byte it read goes out with the count
	               3  + the record goes into the ring buffer through
	                  lcdtp_sendlogc(), as diag9 does every pass
	               4  + diag9's second drawn row, the seven bytes

	The console output has the same shape and the same timing at every
	level - two hex digits of count, a space, two of data, a newline, at
	100ms a character - with "--" for the data where nothing is read. What
	arrives, and when, is then the same in every build, so the builds
	differ only in the thing added.

	What a failure at level n does not say by itself: this fault has come
	and gone with nothing but the code layout changing. A level that fails
	has to be repeated with its code compiled in and not run before it can
	be blamed for what the code does rather than for where it moved things.
*/

#include	"lcdtp.h"
#include	"dbgcon.h"
#include	"i2c.h"


#ifndef	DIAG10_ADD
#define	DIAG10_ADD	0
#endif

#define	ADDR	0x38		/* FT5x06 */


static	const	char	*bin2hex = "0123456789abcdef";


static	UB	*puthex2(UB *p, UW v)
{
	*(p++) = (UB)bin2hex[(v >> 4) & 0xf];
	*(p++) = (UB)bin2hex[v & 0xf];
	return p;
}


static	UB	*putstr(UB *p, const char *s)
{
	while (*s)
		*(p++) = (UB)*(s++);
	return p;
}


/* Blind, as in diag5: no status read, 100ms a character. */
static	void	blindputc(W c)
{
	DBGCON_TX = (UW)(UB)c;
	dly_tsk(100);
}


#if	DIAG10_ADD >= 2
/* diag9's transaction, unchanged. */
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
#endif


#if	DIAG10_ADD >= 3
static	void	loghex2(UW v)
{
	lcdtp_sendlogc(bin2hex[(v >> 4) & 0xf]);
	lcdtp_sendlogc(bin2hex[v & 0xf]);
}
#endif


int	main(void)
{
	static	UB	line[32];
	static	UB	buf[7];
	UB	*p;
	UW	n;
	W	ok = 0;
#if	DIAG10_ADD >= 3
	W	i;
#endif

	init_lcdtp();
	dly_tsk(500);
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	gdra_stp(8, 20, 0x07e0, 0x0000, NULL, (UB*)"diag10 add");
	p = puthex2(line, DIAG10_ADD);
	*p = 0;
	gdra_stp(140, 20, 0x07e0, 0x0000, NULL, line);

	/* The build names itself first, so no capture is ambiguous. */
	blindputc('a');
	blindputc((W)bin2hex[DIAG10_ADD & 0xf]);
	blindputc('\n');

#if	DIAG10_ADD >= 1
	(void)i2cprobe(ADDR);
#endif

	for (n = 0; ; n++) {
#if	DIAG10_ADD >= 2
		ok = read7(buf);
#endif

#if	DIAG10_ADD >= 3
		loghex2(n >> 8);
		loghex2(n);
		lcdtp_sendlogc(' ');
		for (i = 0; i < 7; i++)
			loghex2(buf[i]);
		lcdtp_sendlogc(' ');
		lcdtp_sendlogc((ok)? 'o' : 'x');
		lcdtp_sendlogc('\n');
#endif

		blindputc((W)bin2hex[(n >> 4) & 0xf]);
		blindputc((W)bin2hex[n & 0xf]);
		blindputc(' ');
#if	DIAG10_ADD >= 2
		blindputc((W)bin2hex[(buf[0] >> 4) & 0xf]);
		blindputc((W)bin2hex[buf[0] & 0xf]);
#else
		blindputc('-');
		blindputc('-');
#endif
		blindputc('\n');

		p = putstr(line, "sent=");
		p = puthex2(p, n >> 8);
		p = puthex2(p, n);
		*p = 0;
		gfil_rec(8, 60, 400, 88, 0x0000);
		gdra_stp(8, 74, 0x07ff, 0x0000, NULL, line);

#if	DIAG10_ADD >= 4
		for (i = 0; i < 7; i++) {
			puthex2(line + i * 3, buf[i]);
			line[i * 3 + 2] = ' ';
		}
		line[20] = 0;
		gfil_rec(8, 88, 400, 116, 0x0000);
		gdra_stp(8, 102, 0xffe0, 0x0000, NULL, line);
#endif

		(void)ok;
		dly_tsk(200);
	}

	return 0;
}
