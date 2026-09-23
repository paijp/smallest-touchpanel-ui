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
	               4  SCL and SDA both low, then both released, each bit
	               5  SDA low, SCL low, SCL released, SDA released
	                  (a start then a stop, with no bits in between)

	No display, no console: the fault handler's breakpoint and diag12_n,
	the pass count, are the result.
*/

#include	"basic.h"
#include	"envision_hw.h"
#include	"i2c.h"


#ifndef	DIAG12_MODE
#define	DIAG12_MODE	0
#endif

/* -DDIAG12_ADDR=0x39 addresses nobody: the same traffic, never answered */
#ifndef	DIAG12_ADDR
#define	DIAG12_ADDR	0x38	/* FT5x06 */
#endif
#define	ADDR	DIAG12_ADDR

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

#ifdef	DIAG12_HOCO
	/*
		-DDIAG12_HOCO: the 16MHz on-chip oscillator, every divider 1, no
		PLL and no main oscillator. The instructions that go wrong are
		right in the flash, so the next suspect is the clock they are
		executed at. envision_delay_ms() is calibrated for 120MHz, so
		everything here runs about 7.5 times slower.
	*/
	SYSTEM.PRCR.WORD = 0xa50f;
	SYSTEM.HOCOCR.BIT.HCSTP = 0;
	while (!(SYSTEM.OSCOVFSR.BIT.HCOVF))
		;
	SYSTEM.SCKCR.LONG = 0;
	SYSTEM.SCKCR3.BIT.CKSEL = 1;
	SYSTEM.PRCR.WORD = 0xa500;
#elif	defined(DIAG12_PLLHOCO)
	/*
		-DDIAG12_PLLHOCO: the same 240MHz PLL and the same dividers as
		envision_clock_init(), but locked to the 16MHz on-chip oscillator
		(x15) instead of the 12MHz resonator. If HOCO alone is clean and
		this is too, the resonator is the difference; if this faults, it
		is the speed or the PLL.
	*/
	SYSTEM.PRCR.WORD = 0xa50f;
	SYSTEM.HOCOCR2.BYTE = 0;		/* 16MHz */
	SYSTEM.HOCOCR.BIT.HCSTP = 0;
	while (!(SYSTEM.OSCOVFSR.BIT.HCOVF))
		;
	SYSTEM.ROMWT.BIT.ROMWT = 2;
	while (SYSTEM.ROMWT.BIT.ROMWT != 2)
		;
	SYSTEM.PLLCR.BIT.PLIDIV = 0;
	SYSTEM.PLLCR.BIT.PLLSRCSEL = 1;		/* HOCO */
	SYSTEM.PLLCR.BIT.STC = 29;		/* x15 */
	SYSTEM.PLLCR2.BIT.PLLEN = 0;
	while (!(SYSTEM.OSCOVFSR.BIT.PLOVF))
		;
	SYSTEM.SCKCR.BIT.ICK = 1;
	SYSTEM.SCKCR.BIT.FCK = 2;
	SYSTEM.SCKCR.BIT.PCKA = 1;
	SYSTEM.SCKCR.BIT.PCKB = 2;
	SYSTEM.SCKCR.BIT.PCKC = 2;
	SYSTEM.SCKCR.BIT.PCKD = 2;
	SYSTEM.SCKCR.BIT.PSTOP0 = 1;
	SYSTEM.SCKCR.BIT.PSTOP1 = 1;
	SYSTEM.SCKCR3.BIT.CKSEL = 4;
	SYSTEM.PRCR.WORD = 0xa500;
#else
	envision_clock_init();
#endif
#ifdef	DIAG12_ROMCE
	/*
		-DDIAG12_ROMCE: the ROM cache on. Every fault is an instruction
		that is right in the flash and wrong where the processor fetched
		it, so fetching from the cache instead is the obvious thing to
		try.
	*/
	FLASH.ROMCIV.BIT.ROMCIV = 1;
	while ((FLASH.ROMCIV.BIT.ROMCIV))
		;
	FLASH.ROMCE.BIT.ROMCEN = 1;
#endif
	(void)i2cprobe(ADDR);
	i2c_swap = 1;		/* this board's, whether or not ADDR answered */

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
#elif	DIAG12_MODE == 4
			scl_set(0);
			sda_set(0);
#elif	DIAG12_MODE == 5
			sda_set(0);
			i2cwait();
			scl_set(0);
#endif
			i2cwait();
			i2cwait();
#if	DIAG12_MODE == 1
			scl_set(1);
#elif	DIAG12_MODE == 2
			sda_set(1);
#elif	DIAG12_MODE == 4
			scl_set(1);
			sda_set(1);
#elif	DIAG12_MODE == 5
			scl_set(1);
			i2cwait();
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
