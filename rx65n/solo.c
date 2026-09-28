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

	And this port's own notes said the same thing from the other side:
	undefined instructions, and PCs one byte into a valid instruction.

	All of that was the symptom of one thing, found later and written up in
	envision_hw.c: SCKCR set one field at a time leaves ICK at 0, so ICLK ran
	at 240 MHz against a 120 MHz maximum and the code flash was read at twice
	the speed its wait states were set for. Everything this file's notes used
	to list as ruled out really was ruled out - the optimiser, interrupts, the
	exception handlers, the probe, the pins, the libraries, the display - and
	the one thing left was never on the list, because it was in the clock setup
	that every build shared, this one included.

	The backlight is the liveness signal: solid while a contact is reported,
	blinking otherwise, dark only if the MCU was reset or lost power.

	-DSOLO_RAMCODE puts every function in RAM and runs it from there.

	-DSOLO_RAMCODE was the experiment that seemed to decide the port's fault:
	this program stopped on its first pass out of flash and did 207 reads from
	RAM without one failure, which read as the instruction fetch being the
	problem.

	It was the right measurement and the wrong conclusion. This file had the
	same bug as the port: it set SCKCR one field at a time, which leaves ICK at
	0 and runs ICLK at 240 MHz against a 120 MHz maximum, so the flash could
	not be read at the speed it was being read at. RAM has no wait states,
	which is the whole of why the RAM build survived. Fixed here as it is in
	envision_hw.c, where the measurement that settled it is written up; the
	flash build has no reason to fail now, and -DSOLO_RAMCODE is kept because
	a single file that links nothing is still the cleanest place to test an
	idea about this board.

	It needs the linker script patched to add a .ramfunc section; see
	tools/patch-ramfunc.py. main() stays in ROM, copies the section, and only
	then calls anything in it.

	Build:  bash tools/patch-ramfunc.py generate/linker_script.ld
	        rx-elf-gcc -mcpu=rx64m -O2 -nostartfiles -Igenerate -DSOLO_RAMCODE \
	            solo.c generate/hwinit.c generate/inthandler.c \
	            generate/vects.c generate/start.S \
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


/*
	With -DSOLO_RAMCODE every function below carries this, and the linker puts
	them all in a section whose addresses are in RAM while its contents load
	from ROM. noinline keeps them from being folded back into main(), which
	stays in ROM because it is what does the copying.
*/
#ifdef	SOLO_RAMCODE
#define	RAMFUNC	__attribute__((section(".ramfunc"), noinline))
#else
#define	RAMFUNC
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
static	RAMFUNC	void	putc1(W c)
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


static	RAMFUNC	void	puts1(const char *s)
{
	while ((*s))
		putc1(*s++);
}


static	RAMFUNC	void	putdec1(UW v)
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


static	RAMFUNC	void	puthex1(UW v, W digits)
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

	/*
		One 32-bit write: ICK /2 = 120, FCK /4 = 60, PCKA /2 = 120,
		PCKB, PCKC and PCKD /4 = 60, BCK /2, PSTOP0 and PSTOP1 set.

		This file used to set those fields one at a time, which leaves ICK
		at 0 and ICLK at 240 MHz - and that is what this file's famous
		measurement was actually measuring. See the header comment.
	*/
	SYSTEM.SCKCR.LONG = 0x21C11222UL;

	SYSTEM.SCKCR3.BIT.CKSEL = 4;		/* switch to the PLL */

	SYSTEM.PRCR.WORD = 0xa500;
}


static	RAMFUNC	void	delay_us(UW us)
{
	volatile UW	i;

	while (us--)
		for (i = 0; i < 24; i++)
			;
}


static	RAMFUNC	void	delay_ms(UW ms)
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


static	RAMFUNC	void	scl_set(W v)
{
	if ((i2c_swap))
		PORT0.PODR.BIT.B1 = (UB)(v? 1 : 0);
	else
		PORT0.PODR.BIT.B0 = (UB)(v? 1 : 0);
}


static	RAMFUNC	W	scl_get(void)
{
	return ((i2c_swap)? PORT0.PIDR.BIT.B1 : PORT0.PIDR.BIT.B0)? 1 : 0;
}


static	RAMFUNC	void	sda_set(W v)
{
	if ((i2c_swap))
		PORT0.PODR.BIT.B0 = (UB)(v? 1 : 0);
	else
		PORT0.PODR.BIT.B1 = (UB)(v? 1 : 0);
}


static	RAMFUNC	W	sda_get(void)
{
	return ((i2c_swap)? PORT0.PIDR.BIT.B0 : PORT0.PIDR.BIT.B1)? 1 : 0;
}


static	RAMFUNC	void	i2cwait(void)
{
	volatile W	i;

	for (i = 0; i < I2C_HALFBIT; i++)
		;
}


static	RAMFUNC	void	i2cscl_high(void)
{
	W	n;

	scl_set(1);
	for (n = I2C_STRETCH; n > 0; n--)
		if ((scl_get()))
			return;
	i2c_stretch_timeouts++;
}


static	RAMFUNC	void	i2cinit(void)
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


static	RAMFUNC	void	i2cstart(void)
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


static	RAMFUNC	void	i2cstop(void)
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
static	RAMFUNC	W	i2csend(W data)
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
static	RAMFUNC	W	i2crecv(W nak)
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
static	RAMFUNC	W	i2cprobe(void)
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
static	RAMFUNC	W	touch_read(UB *buf)
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


#ifdef	SOLO_RAMCODE
/*
	From the linker script. Declared without the leading underscore because the
	compiler prepends one to every C identifier on RX, so `ramfunc` here is the
	script's `_ramfunc`.
*/
extern	char	ramfunc[];
extern	char	eramfunc[];
extern	char	mramfunc[];
#endif


/* ---- the program ---- */

int	main(void)
{
	UB	buf[7];
	UW	pass = 0;
	UW	ok = 0;
	W	answered, got, i, x, y;

	clock_init();
	sci1_init();

#ifdef	SOLO_RAMCODE
	/*
		Copy the code before calling any of it. clock_init() and sci1_init()
		above are deliberately left in ROM: they run before this, and main()
		itself has to stay in ROM to be able to do the copying at all.
	*/
	{
		UB	*d = (UB*)ramfunc;
		const UB *src = (const UB*)mramfunc;

		while (d < (UB*)eramfunc)
			*d++ = *src++;
	}
#endif

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
#ifdef	SOLO_RAMCODE
	puts1("  code in RAM at ");
	puthex1((UW)ramfunc, 8);
	puts1("..");
	puthex1((UW)eramfunc, 8);
#else
	puts1("  code in ROM");
#endif
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
