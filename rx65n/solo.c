/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	Everything in one file: clock, serial, bit-banged I2C, touch read.

	Built with nothing but the startup files - no lcdtp.c, no envision_hw.c, no
	i2c.h, no debuglog.c, no seriallog.c, no newlib. Only iodefine.h for the
	register names, and generate/start.S and vects.c for the reset vector.

	It exists because of what the fault looks like. The program counter goes
	somewhere the source does not go: with the I2C steps marked one character
	each, a run printed

	    pIswP123456789 pIsw swwsw rrrrrrr P123456789 rrrrrr...

	- the first probe attempt complete, the second one starting, and then the
	shape of touch_read() followed by i2crecv() for ever, with neither of the
	two characters i2cprobe() prints on the way out. That is not a hang in a
	loop; it is control arriving somewhere it was never sent.

	And this port's own notes say the same thing from the other side: undefined
	instructions, PCs one byte into a valid instruction, and whether it happens
	at all depending on nothing but how the code is laid out - a build with the
	debug console compiled in but never switched on faults, the same program
	without it does not.

	Everything that could be ruled out by an experiment has been. Not the
	optimiser (-O0 fails too). Not interrupts (every ICU enable byte reads 00,
	the touch INT line is a polled input). Not a CPU exception, bus error, RAM
	error or NMI (all eight handlers instrumented, never fired). Not a reset
	(the MCU does not answer the boot-mode sync afterwards, so it is still in
	user code). Not the probe (it happens with SW1-1 off, the emulator
	physically disconnected). Not the pins' drive strength (-DI2C_DSCR=0). Not
	the clock, supply or flash-wait margins (-DENVISION_ICLK_60 halves ICLK and
	widens all three at once). Not the display (this fails with none brought
	up), though the display makes it far more frequent.

	What is left is the code itself and how it is built, and that is what this
	file is for. If it runs, the fault is in what was left out, and the
	difference is small enough to bisect. If it fails, the whole program is a
	few hundred lines that can be disassembled and read end to end - which is
	the next step either way, and impossible while the build pulls in four
	other objects and a libc.

	The backlight is the liveness signal: solid while a contact is reported,
	blinking otherwise, dark only if the MCU was reset or lost power.

	Build:  rx-elf-gcc -mcpu=rx64m -O2 -nostartfiles -Igenerate \
	            solo.c generate/vects.c generate/start.S \
	            -T generate/linker_script.ld -o solo.elf
*/

#include	"iodefine.h"


/* ---- types, so nothing is included for them ---- */

typedef signed char		B;
typedef signed short		H;
typedef signed long		W;
typedef unsigned char		UB;
typedef unsigned short		UH;
typedef unsigned long		UW;

#define	SOLO_NULL		((void*)0)


/* ---- what to do, and how fast ---- */

#define	PERIOD_MS		200
#define	BAUD			115200

/*
	The clock, spelled out. The board's main clock is 12 MHz - not the 16 MHz
	the programming tools are told - so the PLL multiplies by 20 to 240 MHz and
	the dividers give ICLK 120 MHz and PCLKB 60 MHz. PLLCR.STC counts in half
	steps as 2*multiplier-1, so x20 is 39, and PLIDIV 0 divides the input by 1.
*/
#define	PLL_STC			39
#define	PCLKB_HZ		60000000UL

/* SCI1, with SEMR.ABCS set: rate is PCLKB / (16 * (BRR + 1)). */
#define	BRR_VALUE		((PCLKB_HZ / (16UL * BAUD)) - 1UL)

/* MPC pin function select for SCI1's TXD/RXD. */
#define	PSEL_SCI1		0x0a

/* FT5x06, 7-bit address. */
#define	TOUCH_ADDR		0x38

/*
	Half a bit of the I2C clock, as a plain loop count. 300 at 120 MHz is a few
	microseconds, which is far slower than the bus needs and is deliberate:
	speed is not what is being investigated.
*/
#define	I2C_HALFBIT		300

/* Bounded wait for a device that holds the clock down. */
#define	I2C_STRETCH		20000

/* Drive capacity of the two I2C pins: 0 normal, 1 high. */
#ifndef	SOLO_DSCR
#define	SOLO_DSCR		0
#endif


/* ---- serial ---- */

static	UW	tx_drops = 0;


static	void	sci1_init(void)
{
	SYSTEM.PRCR.WORD = 0xa50b;
	MSTP(SCI1) = 0;
	SYSTEM.PRCR.WORD = 0xa500;

	SCI1.SCR.BYTE = 0x00;
	SCI1.SMR.BYTE = 0x00;			/* 8N1, PCLK/1 */
	SCI1.BRR = (UB)BRR_VALUE;
	SCI1.SEMR.BYTE = 0x10;			/* ABCS: 16 clocks per bit */

	MPC.PWPR.BYTE = 0x00;
	MPC.PWPR.BYTE = 0x40;
	MPC.P26PFS.BYTE = PSEL_SCI1;		/* TXD1 */
	MPC.P30PFS.BYTE = PSEL_SCI1;		/* RXD1 */
	MPC.PWPR.BYTE = 0x80;
	PORT2.PMR.BIT.B6 = 1;
	PORT3.PMR.BIT.B0 = 1;

	SCI1.SCR.BYTE = 0x30;			/* TE and RE */
}


/*
	Bounded, so a stalled transmitter drops characters instead of stopping the
	program - the distinction between "stopped" and "the log stopped" cost a
	session once already.
*/
static	void	putc1(W c)
{
	UW	spin;

	for (spin = 0; SCI1.SSR.BIT.TDRE == 0; spin++) {
		if (spin >= 200000UL) {
			tx_drops++;
			return;
		}
	}
	SCI1.TDR = (UB)c;
}


static	void	puts1(const char *s)
{
	while ((*s))
		putc1(*s++);
}


static	void	putdec1(UW v)
{
	UB	buf[12];
	W	n = 0;

	if (v == 0) {
		putc1('0');
		return;
	}
	while ((v) && n < (W)sizeof(buf)) {
		buf[n++] = (UB)('0' + (v % 10));
		v /= 10;
	}
	while (n > 0)
		putc1(buf[--n]);
}


static	void	puthex1(UW v, W digits)
{
	static	const char	*bin2hex = "0123456789abcdef";
	W	i;

	for (i = digits - 1; i >= 0; i--)
		putc1(bin2hex[(v >> (i * 4)) & 0xf]);
}


/*
	One character per I2C step, emitted as it happens. A trace kept in RAM
	cannot be read back when the target stops, and this program's whole purpose
	is to watch where it goes.
*/
#ifdef	SOLO_MARK
#define	MARK(c)		putc1(c)
#else
#define	MARK(c)		do { } while (0)
#endif


/* ---- clock ---- */

static	void	clock_init(void)
{
	SYSTEM.PRCR.WORD = 0xa50b;

	/* ICLK will be 120 MHz, which needs two ROM wait states. Before the
	   clock goes up, not after. */
	SYSTEM.ROMWT.BYTE = 0x02;
	while (SYSTEM.ROMWT.BYTE != 0x02)
		;

	SYSTEM.MOFCR.BYTE = 0x20;		/* resonator, 8.1 to 16 MHz */
	SYSTEM.MOSCWTCR.BYTE = 0x53;
	SYSTEM.MOSCCR.BYTE = 0x00;		/* start the main clock */
	while (SYSTEM.OSCOVFSR.BIT.MOOVF == 0)
		;

	SYSTEM.PLLCR.WORD = (PLL_STC << 8);	/* PLIDIV /1, main clock, x20 */
	SYSTEM.PLLCR2.BYTE = 0x00;		/* run */
	while (SYSTEM.OSCOVFSR.BIT.PLOVF == 0)
		;

	SYSTEM.SCKCR.BIT.ICK = 1;		/* ICLK  120 MHz */
	SYSTEM.SCKCR.BIT.FCK = 2;		/* FCLK   60 MHz */
	SYSTEM.SCKCR.BIT.PCKA = 1;		/* PCLKA 120 MHz */
	SYSTEM.SCKCR.BIT.PCKB = 2;		/* PCLKB  60 MHz */
	SYSTEM.SCKCR.BIT.PCKC = 2;
	SYSTEM.SCKCR.BIT.PCKD = 2;
	SYSTEM.SCKCR.BIT.PSTOP0 = 1;
	SYSTEM.SCKCR.BIT.PSTOP1 = 1;

	SYSTEM.SCKCR3.BIT.CKSEL = 4;		/* switch to the PLL */

	SYSTEM.PRCR.WORD = 0xa500;
}


static	void	delay_us(UW us)
{
	volatile UW	i;

	while (us--)
		for (i = 0; i < 24; i++)
			;
}


static	void	delay_ms(UW ms)
{
	while (ms--)
		delay_us(1000);
}


/* ---- bit-banged I2C on P00 and P01 ---- */

/*
	Which pin is the clock is settled by asking the panel, because the two are
	crossed over on this board relative to the obvious reading of it: with the
	wrong mapping nothing can acknowledge, since the address is being shifted
	out on the line the device listens to for a clock.
*/
static	W	i2c_swap = 0;
static	W	i2c_stretch_timeouts = 0;


static	void	scl_set(W v)
{
	if ((i2c_swap))
		PORT0.PODR.BIT.B1 = (UB)(v? 1 : 0);
	else
		PORT0.PODR.BIT.B0 = (UB)(v? 1 : 0);
}


static	W	scl_get(void)
{
	return ((i2c_swap)? PORT0.PIDR.BIT.B1 : PORT0.PIDR.BIT.B0)? 1 : 0;
}


static	void	sda_set(W v)
{
	if ((i2c_swap))
		PORT0.PODR.BIT.B0 = (UB)(v? 1 : 0);
	else
		PORT0.PODR.BIT.B1 = (UB)(v? 1 : 0);
}


static	W	sda_get(void)
{
	return ((i2c_swap)? PORT0.PIDR.BIT.B0 : PORT0.PIDR.BIT.B1)? 1 : 0;
}


static	void	i2cwait(void)
{
	volatile W	i;

	for (i = 0; i < I2C_HALFBIT; i++)
		;
}


static	void	i2cscl_high(void)
{
	W	n;

	scl_set(1);
	for (n = I2C_STRETCH; n > 0; n--)
		if ((scl_get()))
			return;
	i2c_stretch_timeouts++;
}


static	void	i2cinit(void)
{
	MARK('I');
	PORT0.PMR.BIT.B0 = 0;
	PORT0.PCR.BIT.B0 = 0;
	PORT0.ODR0.BIT.B0 = 1;			/* open drain */
	PORT0.DSCR.BIT.B0 = SOLO_DSCR;
	PORT0.PODR.BIT.B0 = 1;
	PORT0.PDR.BIT.B0 = 1;

	PORT0.PMR.BIT.B1 = 0;
	PORT0.PCR.BIT.B1 = 0;
	PORT0.ODR0.BIT.B2 = 1;			/* two bits per pin: B2 is P01 */
	PORT0.DSCR.BIT.B1 = SOLO_DSCR;
	PORT0.PODR.BIT.B1 = 1;
	PORT0.PDR.BIT.B1 = 1;

	i2cwait();
}


static	void	i2cstart(void)
{
	MARK('s');
	sda_set(1);				/* works as a repeated start too */
	i2cwait();
	i2cscl_high();
	i2cwait();

	sda_set(0);				/* data falls with the clock high */
	i2cwait();
	scl_set(0);
	i2cwait();
}


static	void	i2cstop(void)
{
	MARK('P');
	scl_set(0);
	i2cwait();
	sda_set(0);
	i2cwait();
	i2cscl_high();
	i2cwait();

	sda_set(1);				/* data rises with the clock high */
	i2cwait();
}


/* 0 when the device acknowledged, 1 when it did not */
static	W	i2csend(W data)
{
	W	i, nak;

	MARK('w');
	for (i = 0; i < 8; i++) {
		sda_set((data & (0x80 >> i))? 1 : 0);
		i2cwait();
		i2cscl_high();
		i2cwait();
		scl_set(0);
		i2cwait();
	}

	sda_set(1);				/* let the device answer */
	i2cwait();
	i2cscl_high();
	i2cwait();
	nak = (sda_get())? 1 : 0;
	scl_set(0);
	i2cwait();

	return nak;
}


/* nak = 1 on the last byte, so the device stops driving */
static	W	i2crecv(W nak)
{
	W	i, ret;

	MARK('r');
	sda_set(1);
	ret = 0;
	for (i = 0; i < 8; i++) {
		i2cwait();
		i2cscl_high();
		i2cwait();
		ret <<= 1;
		if ((sda_get()))
			ret |= 1;
		scl_set(0);
	}

	i2cwait();
	sda_set((nak)? 1 : 0);
	i2cwait();
	i2cscl_high();
	i2cwait();
	scl_set(0);
	i2cwait();
	sda_set(1);

	return ret;
}


/*
	Try both pin mappings and keep the one that answers. Returns 1 when
	something did.
*/
static	W	i2cprobe(void)
{
	W	tries;

	for (tries = 0; tries < 2; tries++) {
		MARK('p');
		i2cinit();
		i2cstart();
		if (!i2csend(TOUCH_ADDR << 1)) {
			i2cstop();
			MARK('+');
			return 1;
		}
		i2cstop();
		i2c_swap = !i2c_swap;
	}
	MARK('-');
	return 0;
}


/* Seven bytes from register 2. Returns 1 on a complete transaction. */
static	W	touch_read(UB *buf)
{
	W	i;

	i2cstart();
	if (i2csend(TOUCH_ADDR << 1)) {
		i2cstop();
		return 0;
	}
	if (i2csend(2)) {
		i2cstop();
		return 0;
	}

	i2cstart();				/* repeated start */
	if (i2csend((TOUCH_ADDR << 1) | 1)) {
		i2cstop();
		return 0;
	}

	for (i = 0; i < 7; i++)
		buf[i] = (UB)i2crecv((i == 6)? 1 : 0);

	i2cstop();
	return 1;
}


/* ---- the program ---- */

int	main(void)
{
	UB	buf[7];
	UW	pass = 0;
	UW	ok = 0;
	W	answered, got, i, x, y;

	clock_init();
	sci1_init();

	/* Backlight and panel reset, without the display controller behind
	   them: the light is the only liveness signal here. */
	PORT6.PODR.BIT.B3 = 1;
	PORT6.PDR.BIT.B3 = 1;
	PORT6.PODR.BIT.B6 = 1;
	PORT6.PDR.BIT.B6 = 1;

	puts1("\r\n--- solo: one file, no libraries, ");
	putdec1(PERIOD_MS);
	puts1("ms ---\r\n");
	puts1("pclkb ");
	putdec1(PCLKB_HZ / 1000000UL);
	puts1("MHz  brr ");
	putdec1(BRR_VALUE);
	puts1("  dscr ");
	putdec1(SOLO_DSCR);
	puts1("\r\n");

	puts1("probe: ");
	answered = i2cprobe();
	puts1(answered? " answered" : " nothing answered");
	puts1(", swap ");
	putdec1((UW)i2c_swap);
	puts1("\r\n");

	for (;;) {
		MARK('[');
		got = touch_read(buf);
		MARK(']');
		pass++;
		if ((got))
			ok++;

		x = -1;
		y = -1;
		if ((got) && buf[0] != 0) {
			x = ((W)(buf[1] & 0x0f) << 8) | buf[2];
			y = ((W)(buf[3] & 0x0f) << 8) | buf[4];
		}

		/* Solid while a contact is reported, blinking otherwise. */
		PORT6.PODR.BIT.B6 = (UB)((x >= 0)? 1 : ((pass & 2)? 1 : 0));

		putdec1(pass * PERIOD_MS / 1000);
		putc1('.');
		puthex1((pass * PERIOD_MS) % 1000, 3);
		puts1(" n=");
		putdec1(pass);
		puts1(" ok=");
		putdec1(ok);
		puts1(" str=");
		putdec1((UW)i2c_stretch_timeouts);
		puts1(" drops=");
		putdec1(tx_drops);
		puts1(" |");
		for (i = 0; i < 7; i++) {
			putc1(' ');
			puthex1(buf[i], 2);
		}
		puts1(" | ");
		if (x >= 0) {
			puts1("x=");
			putdec1((UW)x);
			puts1(" y=");
			putdec1((UW)y);
		} else {
			puts1((got)? "no contact" : "read failed");
		}
		puts1("\r\n");

		delay_ms(PERIOD_MS);
	}
}
