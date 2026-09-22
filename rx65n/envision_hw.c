/*
	MIT License

	Copyright (c) 2019 John Blaiklock
	Copyright (c) 2026 paijp

	Permission is hereby granted, free of charge, to any person obtaining a
	copy of this software and associated documentation files (the
	"Software"), to deal in the Software without restriction, including
	without limitation the rights to use, copy, modify, merge, publish,
	distribute, sublicense, and/or sell copies of the Software, and to
	permit persons to whom the Software is furnished to do so, subject to
	the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
	Board bring-up for the RX65N Envision Kit.

	The register sequences here come from miniwinwm/RenesasEnvisionGCC
	(EnvisionDemo1, src/EnvisionDemo1.c, src/lcd_driver.c, src/touch_driver.c).
	They are panel- and board-specific values that have been shown to work on
	this hardware, which is exactly the part that is not worth re-deriving
	from the hardware manual with no way to test the result.

	Three things are deliberately different from the original:

	1. The touch controller is reached by bit-banging I2C on P00/P01
	   (i2c.h), not through SCI6's simple-IIC mode. Both of the bugs this
	   port had were in that peripheral's flag semantics rather than in
	   the protocol - see i2c.h for what they were. Bit-banging also
	   removes this file's dependency on the upstream demo's interrupt
	   handlers, so the only thing it still needs from that project is
	   iodefine.h for the register names.

	2. Every wait is bounded. The original's waits are `while (!flag) {}`,
	   so a missing ACK - or a touch controller that has stopped answering -
	   hangs the program with no way out. Here each one gives up after a
	   timeout and the transaction reports failure, which the caller treats
	   as "no touch this time" and retries on the next pass.

	3. envision_touch_get() reports "no touch" for the phantom contact the
	   panel asserts at power-up, rather than reporting a touch at an
	   uninitialised coordinate.
*/

#include	"iodefine.h"
#include	"envision_hw.h"


#define	TOUCH_I2C_ADDRESS	0x38


/*
	Busy-wait. ICLK is 120MHz after envision_clock_init(), and the loop body
	is a NOP plus the loop overhead, so the count is nominal rather than
	calibrated: these delays gate a display reset and a touch controller
	reset, where being somewhat slow is free and being fast is not.
*/
void	__attribute__((optimize("O1")))	envision_delay_us(UW us)
{
	volatile UW	i;
	UW		n;

	n = us * 30;
	for (i = 0; i < n; i++)
		__asm__ __volatile__ ("nop");
}


void	envision_delay_ms(UW ms)
{
	while (ms-- > 0)
		envision_delay_us(1000);
}


/*
	12MHz resonator, PLL x20 -> 240MHz, ICLK 120MHz, PCLKB 60MHz.
	Must run before anything else: the GLCDC dot clock is derived from it,
	and so is the delay loop everything else times itself with.
*/
void	envision_clock_init(void)
{
	/* unlock the clock control registers */
	SYSTEM.PRCR.WORD = 0xa50b;

	/* resonator, not an external clock */
	SYSTEM.MOFCR.BIT.MOSEL = 0;

	/* the HOCO and the sub-clock are unused, so stop and unpower them */
	SYSTEM.HOCOCR.BIT.HCSTP = 1;
	SYSTEM.HOCOPCR.BIT.HOCOPCNT = 1;
	SYSTEM.SOSCCR.BIT.SOSTP = 1;

	/* no USB clock */
	SYSTEM.SCKCR2.WORD = 0x0001;

	/* drive level for a 12MHz input, and its stabilisation wait */
	SYSTEM.MOFCR.BIT.MODRV2 = 2;
	SYSTEM.MOSCWTCR.BYTE = 0x53;
	SYSTEM.MOSCCR.BIT.MOSTP = 0;

	/*
		Wait for the oscillator to say it is running. MOSCWTCR gates the
		clock output until its counter expires, so the PLL below would
		not be locking onto nothing either way - but the documented
		order asks for this before the main clock is used as a source,
		and the cost of following it is one polled bit.
	*/
	while (SYSTEM.OSCOVFSR.BIT.MOOVF == 0)
		__asm__ __volatile__ ("nop");

	/*
		ROM wait states have to go up before the clock does - and the
		write has to have taken effect before the clock does, which is
		not the same thing and is the part that was missing.

		Above 100MHz the code flash needs two wait states. This write is
		not applied the instant it retires, so raising ICLK to 120MHz
		straight afterwards can happen while the flash is still being
		read with too few waits. What comes back then is not an error,
		it is wrong data - and wrong data fetched as code is a garbage
		instruction: an undefined instruction, or a 0x00, which is BRK,
		or a branch to nowhere.

		That is the shape of the fault this port has been chasing.
		Intermittent, never the same address twice, wild PCs in
		unimplemented space, worse the more the program does, and
		present in the diagnostic that was thought to be clean as well
		as the one that was not. Reading the register back until it
		reads 2 is what the Renesas BSP does and what was missing here.

		Whether it is *the* cause is not established - it is a
		documented requirement that was not met and a plausible fit for
		the symptom. The board decides that, not this comment.
	*/
	SYSTEM.ROMWT.BIT.ROMWT = 2;
	while (SYSTEM.ROMWT.BIT.ROMWT != 2)
		__asm__ __volatile__ ("nop");

	SYSTEM.PLLCR.BIT.PLIDIV = 0;		/* /1 */
	SYSTEM.PLLCR.BIT.STC = 39;		/* x20 */
	SYSTEM.PLLCR2.BIT.PLLEN = 0;		/* run */
	while (SYSTEM.OSCOVFSR.BIT.PLOVF == 0)
		__asm__ __volatile__ ("nop");

	SYSTEM.SCKCR.BIT.ICK = 1;		/* ICLK  120MHz */
	SYSTEM.SCKCR.BIT.FCK = 2;		/* FCLK   60MHz */
	SYSTEM.SCKCR.BIT.PCKA = 1;		/* PCLKA 120MHz */
	SYSTEM.SCKCR.BIT.PCKB = 2;		/* PCLKB  60MHz */
	SYSTEM.SCKCR.BIT.PCKC = 2;		/* PCLKC  60MHz */
	SYSTEM.SCKCR.BIT.PCKD = 2;		/* PCLKD  60MHz */
	SYSTEM.SCKCR.BIT.PSTOP0 = 1;		/* SDCLK unused */
	SYSTEM.SCKCR.BIT.PSTOP1 = 1;		/* BCLK  unused */

	SYSTEM.SCKCR3.BIT.CKSEL = 4;		/* switch to the PLL */
	SYSTEM.LOCOCR.BYTE = 1;			/* LOCO no longer needed */

	SYSTEM.PRCR.WORD = 0xa500;
}


	/* ---- GLCDC ---- */

void	envision_lcd_init(void)
{
	/* route the 16 data lines, the dot clock and the three TCON outputs */
	MPC.PWPR.BIT.B0WI = 0;
	MPC.PWPR.BIT.PFSWE = 1;

	MPC.PB5PFS.BYTE = 0x25;	PORTB.PMR.BIT.B5 = 1;	/* LCD_CLK */
	MPC.PB4PFS.BYTE = 0x25;	PORTB.PMR.BIT.B4 = 1;	/* LCD_TCON0 */
	MPC.PB2PFS.BYTE = 0x25;	PORTB.PMR.BIT.B2 = 1;	/* LCD_TCON2 */
	MPC.PB1PFS.BYTE = 0x25;	PORTB.PMR.BIT.B1 = 1;	/* LCD_TCON3 */

	MPC.PB0PFS.BYTE = 0x25;	PORTB.PMR.BIT.B0 = 1;	/* LCD_DATA0 */
	MPC.PA7PFS.BYTE = 0x25;	PORTA.PMR.BIT.B7 = 1;	/* LCD_DATA1 */
	MPC.PA6PFS.BYTE = 0x25;	PORTA.PMR.BIT.B6 = 1;	/* LCD_DATA2 */
	MPC.PA5PFS.BYTE = 0x25;	PORTA.PMR.BIT.B5 = 1;	/* LCD_DATA3 */
	MPC.PA4PFS.BYTE = 0x25;	PORTA.PMR.BIT.B4 = 1;	/* LCD_DATA4 */
	MPC.PA3PFS.BYTE = 0x25;	PORTA.PMR.BIT.B3 = 1;	/* LCD_DATA5 */
	MPC.PA2PFS.BYTE = 0x25;	PORTA.PMR.BIT.B2 = 1;	/* LCD_DATA6 */
	MPC.PA1PFS.BYTE = 0x25;	PORTA.PMR.BIT.B1 = 1;	/* LCD_DATA7 */
	MPC.PA0PFS.BYTE = 0x25;	PORTA.PMR.BIT.B0 = 1;	/* LCD_DATA8 */
	MPC.PE7PFS.BYTE = 0x25;	PORTE.PMR.BIT.B7 = 1;	/* LCD_DATA9 */
	MPC.PE6PFS.BYTE = 0x25;	PORTE.PMR.BIT.B6 = 1;	/* LCD_DATA10 */
	MPC.PE5PFS.BYTE = 0x25;	PORTE.PMR.BIT.B5 = 1;	/* LCD_DATA11 */
	MPC.PE4PFS.BYTE = 0x25;	PORTE.PMR.BIT.B4 = 1;	/* LCD_DATA12 */
	MPC.PE3PFS.BYTE = 0x25;	PORTE.PMR.BIT.B3 = 1;	/* LCD_DATA13 */
	MPC.PE2PFS.BYTE = 0x25;	PORTE.PMR.BIT.B2 = 1;	/* LCD_DATA14 */
	MPC.PE1PFS.BYTE = 0x25;	PORTE.PMR.BIT.B1 = 1;	/* LCD_DATA15 */

	MPC.PWPR.BIT.PFSWE = 0;
	MPC.PWPR.BIT.B0WI = 1;

	/* P63: panel reset, released */
	PORT6.PMR.BIT.B3 = 0;
	PORT6.PDR.BIT.B3 = 1;
	PORT6.ODR0.BIT.B6 = 0;
	PORT6.PODR.BIT.B3 = 1;

	/* P66: backlight, on */
	PORT6.PMR.BIT.B6 = 0;
	PORT6.PDR.BIT.B6 = 1;
	PORT6.ODR1.BIT.B4 = 0;
	PORT6.PODR.BIT.B6 = 1;

	/* clock to the GLCDC */
	SYSTEM.PRCR.WORD = 0xa502;
	MSTP(GLCDC) = 0;
	SYSTEM.PRCR.WORD = 0xa500;

	GLCDC.BGEN.BIT.SWRST = 1;

	GLCDC.PANELCLK.BIT.CLKSEL = 1;		/* dot clock from the PLL */
	GLCDC.PANELCLK.BIT.DCDR = 0x18;
	GLCDC.PANELCLK.BIT.PIXSEL = 0;
	GLCDC.PANELCLK.BIT.CLKEN = 1;

	/*
		BGMON.SWRST is sampled with PXCLK, so it only follows BGEN.SWRST
		once the panel clock is actually running - which makes this the
		clock-is-alive check as much as a reset-released check.
	*/
	while (GLCDC.BGMON.BIT.SWRST == 0)
		__asm__ __volatile__ ("nop");

	/* everything on the rising edge of LCD_CLK */
	GLCDC.CLKPHASE.BIT.LCDEDG = 0;
	GLCDC.CLKPHASE.BIT.TCON0EDG = 0;
	GLCDC.CLKPHASE.BIT.TCON2EDG = 0;
	GLCDC.CLKPHASE.BIT.TCON3EDG = 0;
	GLCDC.TCONTIM.BIT.OFFSET = 0;
	GLCDC.TCONTIM.BIT.HALF = 0;

	/* STHA: horizontal sync, inverted, 1 pixel wide, no delay */
	GLCDC.TCONSTHA2.BIT.SEL = 2;
	GLCDC.TCONSTHA2.BIT.INV = 1;
	GLCDC.TCONSTHA2.BIT.HSSEL = 0;
	GLCDC.TCONSTHA1.BIT.HS = 0;
	GLCDC.TCONSTHA1.BIT.HW = 1;

	/* STVA: vertical sync, inverted, 1 line */
	GLCDC.TCONSTVA2.BIT.SEL = 0;
	GLCDC.TCONSTVA2.BIT.INV = 1;
	GLCDC.TCONSTVA1.BIT.VS = 0;
	GLCDC.TCONSTVA1.BIT.VW = 1;

	/* STHB/STVB carry data enable */
	GLCDC.TCONSTHB2.BIT.SEL = 7;
	GLCDC.TCONDE.BIT.INV = 0;
	GLCDC.TCONSTHB2.BIT.HSSEL = 0;
	GLCDC.TCONSTHB1.BIT.HS = 0x29;
	GLCDC.TCONSTHB1.BIT.HW = ENVISION_LCD_W;
	GLCDC.TCONSTVB1.BIT.VS = 9;
	GLCDC.TCONSTVB1.BIT.VW = ENVISION_LCD_H;

	/* frame timing: 525 x 288 total for 480 x 272 active */
	GLCDC.BGPERI.BIT.FH = 0x20d;
	GLCDC.BGPERI.BIT.FV = 0x120;
	GLCDC.BGSYNC.BIT.HP = 3;
	GLCDC.BGSYNC.BIT.VP = 7;
	GLCDC.BGHSIZE.BIT.HP = 0x2c;
	GLCDC.BGHSIZE.BIT.HW = ENVISION_LCD_W;
	GLCDC.BGVSIZE.BIT.VP = 0x10;
	GLCDC.BGVSIZE.BIT.VW = ENVISION_LCD_H;
	GLCDC.BGCOLOR.BIT.R = 0;
	GLCDC.BGCOLOR.BIT.G = 0;
	GLCDC.BGCOLOR.BIT.B = 0;

	/*
		Graphics plane 2 reads the framebuffer straight out of expansion
		RAM: RGB565, 480 pixels per line, 272 lines, no gap between lines.
		DATANUM is 64-byte bursts per line minus one (480*2/64 - 1 = 14)
		and LNNUM is lines minus one, in units the controller counts in.
	*/
	GLCDC.GR2FLM6.BIT.FORMAT = 0;				/* RGB565 */
	GLCDC.GR2FLM2 = (UW)ENVISION_FRAMEBUFFER;
	GLCDC.GR2BASE.BIT.R = 0;
	GLCDC.GR2BASE.BIT.G = 0;
	GLCDC.GR2BASE.BIT.B = 0;
	GLCDC.GR2FLM5.BIT.DATANUM = 0xe;
	GLCDC.GR2FLM5.BIT.LNNUM = 0x10f;
	GLCDC.GR2FLM3.BIT.LNOFF = ENVISION_LCD_W * 2;

	GLCDC.GR2AB2.BIT.GRCVW = ENVISION_LCD_H;
	GLCDC.GR2AB2.BIT.GRCVS = 9;
	GLCDC.GR2AB3.BIT.GRCHW = ENVISION_LCD_W;
	GLCDC.GR2AB3.BIT.GRCHS = 0x29;
	GLCDC.GR2AB1.BIT.GRCDISPON = 0;
	GLCDC.GR2AB1.BIT.DISPSEL = 2;
	GLCDC.GR2AB7.BIT.CKON = 0;

	/* output: RGB565, no byte swapping, truncating dither, no division */
	GLCDC.OUTSET.BIT.ENDIANON = 0;
	GLCDC.OUTSET.BIT.SWAPON = 0;
	GLCDC.OUTSET.BIT.FORMAT = 2;
	GLCDC.OUTSET.BIT.FRQSEL = 0;
	GLCDC.PANELDTHA.BIT.FORM = 2;
	GLCDC.PANELDTHA.BIT.SEL = 0;

	/* brightness and contrast at their neutral points, no gamma */
	GLCDC.BRIGHT1.BIT.BRTG = 512;
	GLCDC.BRIGHT2.BIT.BRTB = 512;
	GLCDC.BRIGHT2.BIT.BRTR = 512;
	GLCDC.CONTRAST.BIT.CONTG = 128;
	GLCDC.CONTRAST.BIT.CONTB = 128;
	GLCDC.CONTRAST.BIT.CONTR = 128;
	GLCDC.GAMSW.BIT.GAMON = 0;

	/*
		Nothing here uses interrupts, so make sure none can arrive: the
		underflow requests in particular are latched, and an enabled one
		with no handler is a reset.
	*/
	GLCDC.DTCTEN.BIT.VPOSDTC = 0;
	GLCDC.DTCTEN.BIT.GR1UFDTC = 0;
	GLCDC.DTCTEN.BIT.GR2UFDTC = 0;

	GLCDC.INTEN.BIT.GR1UFINTEN = 0;
	EN(GLCDC, GR1UF) = 0;
	while (IS(GLCDC, GR1UF) != 0)
		__asm__ __volatile__ ("nop");

	GLCDC.INTEN.BIT.GR2UFINTEN = 0;
	EN(GLCDC, GR2UF) = 0;
	while (IS(GLCDC, GR2UF) != 0)
		__asm__ __volatile__ ("nop");

	IEN(ICU, GROUPAL1) = 0;
	IPR(ICU, GROUPAL1) = 0;

	/* start scanning out */
	GLCDC.GR2FLMRD.BIT.RENB = 1;
	GLCDC.BGEN.BIT.EN = 1;
	GLCDC.BGEN.BIT.VEN = 1;
	GLCDC.BGEN.BIT.SWRST = 1;
}


	/* ---- FT5x06 touch controller, bit-banged I2C ---- */

#include	"i2c.h"

volatile UW	envision_i2c_trace[ENVISION_I2C_TRACE_N] = {0};


void	envision_touch_init(void)
{
	/* P05: user button, input. Not used here, but left in a known state. */
	PORT0.PMR.BIT.B5 = 0;
	PORT0.PDR.BIT.B5 = 0;
	PORT0.PCR.BIT.B5 = 0;

	/* P02: touch interrupt line, read as a plain input */
	PORT0.PMR.BIT.B2 = 0;
	PORT0.PDR.BIT.B2 = 0;
	PORT0.PCR.BIT.B2 = 0;

	/* P07: touch controller reset, pulsed low */
	PORT0.PMR.BIT.B7 = 0;
	PORT0.PDR.BIT.B7 = 1;
	PORT0.ODR1.BIT.B6 = 0;
	PORT0.PODR.BIT.B7 = 0;
	envision_delay_ms(2);
	PORT0.PODR.BIT.B7 = 1;
	/*
		The reference waits 100ms here. The FT5x06 is documented as
		needing on the order of 300ms after reset before it will talk
		sensibly, and the reference gets away with less only because it
		never reads the controller until the interrupt line has gone low
		once - which is hundreds of milliseconds later anyway. Wait it
		out here so that gate is a safety net rather than the thing
		holding everything up.
	*/
	envision_delay_ms(300);

	/*
		P00 and P01 stay ordinary port pins: nothing routes them to
		SCI6, and SCI6 itself is left in module stop. The pin function
		select has to be cleared explicitly in case something set it.
	*/
	MPC.PWPR.BIT.B0WI = 0;
	MPC.PWPR.BIT.PFSWE = 1;
	MPC.P00PFS.BYTE = 0x00;
	MPC.P01PFS.BYTE = 0x00;
	MPC.PWPR.BIT.PFSWE = 0;
	MPC.PWPR.BIT.B0WI = 1;

	/*
		Settle which pin is the clock by asking the panel rather than by
		asserting it. Picking wrong is silent from the outside: the
		interrupt line still reports touches, every transaction still
		runs to completion, and every address frame comes back
		unacknowledged - because the address is being shifted out on the
		line the device is watching for a clock.

		[3] records the answer, so the board states its own wiring:
		  0  nothing answered either way round
		  1  SCL on P00, SDA on P01
		  2  SCL on P01, SDA on P00
	*/
	if (!i2cprobe(TOUCH_I2C_ADDRESS))
		envision_i2c_trace[3] = 0;
	else
		envision_i2c_trace[3] = (i2c_swap)? 2 : 1;
}


/*
	One transaction: point the controller at register 2 and read the seven
	bytes from there - the touch-point count, then the first point's X and
	Y, high byte and low. Every step's acknowledge is checked, because a
	read that continues past a device that is not answering returns bus
	idle (all ones) and looks like data.
*/
static	W	touch_read(UB *buf)
{
	W	i;

	envision_i2c_trace[4] = 0;
	envision_i2c_trace[15] = 1;

	i2cstart();
	envision_i2c_trace[15] = 2;
	if ((envision_i2c_trace[0] = i2csend(TOUCH_I2C_ADDRESS << 1))) {
		envision_i2c_trace[4] = 1;	/* address, write */
		i2cstop();
		return 0;
	}
	envision_i2c_trace[15] = 3;
	if (i2csend(2)) {
		envision_i2c_trace[4] = 2;	/* register number */
		i2cstop();
		return 0;
	}

	envision_i2c_trace[15] = 4;
	i2cstart();				/* repeated start */
	envision_i2c_trace[15] = 5;
	if ((envision_i2c_trace[1] = i2csend((TOUCH_I2C_ADDRESS << 1) | 1))) {
		envision_i2c_trace[4] = 3;	/* address, read */
		i2cstop();
		return 0;
	}

	for (i = 0; i < 7; i++) {
		envision_i2c_trace[15] = 6 + i;
		buf[i] = (UB)i2crecv((i == 6)? 1 : 0);
	}

	envision_i2c_trace[15] = 13;
	i2cstop();
	envision_i2c_trace[15] = 14;

	envision_i2c_trace[2] = (UW)i2c_stretch_timeouts;
	envision_i2c_trace[7]++;
	for (i = 0; i < 7; i++)
		envision_i2c_trace[8 + i] = buf[i];

	return 1;
}


W	envision_touch_get_raw(W *x, W *y)
{
	UB	buf[7];

	envision_i2c_trace[6] = PORT0.PIDR.BIT.B2;

	if (touch_read(buf) == 0)
		return 0;

	envision_i2c_trace[16] = 20;
	envision_i2c_trace[5] = buf[0];

	if (buf[0] == 0)
		return 0;

	*x = ((W)(buf[1] & 0x0f) << 8) | buf[2];
	*y = ((W)(buf[3] & 0x0f) << 8) | buf[4];

	return 1;
}


W	envision_touch_get(W *x, W *y)
{
	static	W	firsttouch = 0;

	/*
		The panel comes up asserting a contact that never happened. Its
		interrupt line is the one signal that is trustworthy at this
		point, so wait for that to go low once before believing any
		coordinate. After that the line is unreliable and the controller
		is polled instead.
	*/
	if (firsttouch == 0) {
		envision_i2c_trace[6] = PORT0.PIDR.BIT.B2;
		if (PORT0.PIDR.BIT.B2 != 0)
			return 0;
		firsttouch = 1;
		return 0;
	}

	return envision_touch_get_raw(x, y);
}
