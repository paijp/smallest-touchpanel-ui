/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	diag11 with the display and the console both off faults within a few
	passes, with or without the debugger attached, always as an undefined
	instruction at the head of the delay loop in i2cwait(). This takes the
	I2C transaction apart to see which part of it the fault needs.

	  -DDIAG12_MODE=0  i2cwait() only, as many times as a transaction calls
	                   it, no pin touched after i2cinit()
	               1  + SCL toggled (PODR), low and released each bit
	               2  + SDA toggled the same way, SCL left alone
	               3  the whole read7(), as diag11 does

	No display, no console: the fault handler's breakpoint and diag12_n,
	the pass count, are the result.
*/

#include	"basic.h"
#include	"envision_hw.h"
#include	"i2c.h"


#ifndef	DIAG12_MODE
#define	DIAG12_MODE	0
#endif

/* -DADDR=0x39 addresses nobody: the same traffic, never answered */
#ifndef	ADDR
#define	ADDR	0x38		/* FT5x06 */
#endif

/* about what read7() does: four address/register bytes and seven data */
#define	BITS	(11 * 9 + 8)

volatile	UW	diag12_n = 0;


#if	DIAG12_MODE == 3
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


int	main(void)
{
	static	UB	buf[7];
	UW	n;
	W	i;

	envision_clock_init();
	(void)i2cprobe(ADDR);

	for (n = 0; ; n++) {
		diag12_n = n;
#if	DIAG12_MODE == 3
		(void)read7(buf);
#else
		for (i = 0; i < BITS; i++) {
#if	DIAG12_MODE == 1
			scl_set(0);
#elif	DIAG12_MODE == 2
			sda_set(0);
#endif
			i2cwait();
			i2cwait();
#if	DIAG12_MODE == 1
			scl_set(1);
#elif	DIAG12_MODE == 2
			sda_set(1);
#endif
			i2cwait();
			i2cwait();
		}
#endif
		(void)buf;
		(void)i;
		envision_delay_ms(1000);
	}

	return 0;
}
