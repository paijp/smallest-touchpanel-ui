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

	1. The I2C driver clears its three interrupt flags at the start of
	   every transaction. Nothing in the original does, and a flag left
	   set by a transaction abandoned part way through makes the next one
	   return immediately with stale data - "first touch fine, second touch
	   nothing, board frozen" on hardware. (A polled rewrite was tried
	   first and abandoned: it read the acknowledge bit too early and the
	   data one byte out, and finally returned one stale frame for ever.
	   The interrupt-driven form is what is known to work on this board.)

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
	Enough passes to cover any legitimate wait on a 400kHz bus - one byte is
	about 25us, so this is several hundred frames' worth - and short enough
	that a dead bus costs a fraction of a frame rather than the program.
*/
#define	I2C_TIMEOUT		200000


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
	Must run before anything else: the GLCDC dot clock and the SCI6 bit rate
	below are both derived from it.
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

	/* ROM wait states have to go up before the clock does */
	SYSTEM.ROMWT.BIT.ROMWT = 2;

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


	/* ---- FT5x06 touch controller, on SCI6 in simple-I2C mode ---- */

/*
	Interrupt-driven, exactly as the reference driver is, because the
	polled version was wrong on hardware in ways that took a debugger to
	see. The three handlers live in the startup code's inthandler.c and do
	the right things already: RXI6 reads RDR into received_byte (clearing
	RDRF on the way) and raises rx_complete_interrupt_flag; TXI6 raises
	tx_complete_interrupt_flag; group BL0 clears SIMR3.IICSTIF and raises
	start_complete_interrupt_flag. This file only enables them in the ICU
	and waits on the flags.

	Two things the reference driver did not do. The flags are cleared at
	the start of every transaction - left set by one abandoned part way
	through, they make the next return at once with stale data, which is
	the "first touch fine, second touch nothing" of the original. And every
	wait is bounded, so a device that stops answering costs a frame, not
	the program.
*/
#include	<stdbool.h>
#include	<stdint.h>

extern	volatile bool		rx_complete_interrupt_flag;
extern	volatile bool		tx_complete_interrupt_flag;
extern	volatile bool		start_complete_interrupt_flag;
extern	volatile uint8_t	received_byte;

volatile UW	envision_i2c_trace[ENVISION_I2C_TRACE_N] = {0};


static	W	wait_flag(volatile bool *flag, W site)
{
	UW	n;

	for (n = I2C_TIMEOUT; n > 0; n--)
		if ((*flag)) {
			*flag = false;
			return 1;
		}
	envision_i2c_trace[4] = site;
	return 0;
}


static	void	clear_flags(void)
{
	rx_complete_interrupt_flag = false;
	tx_complete_interrupt_flag = false;
	start_complete_interrupt_flag = false;
}


/*
	Start and stop conditions are generated by writing SIMR3; the group
	BL0 handler clears IICSTIF when the condition has been issued and
	raises the flag. Then the lines are handed to the transmitter (start)
	or released (stop).
*/
static	W	start_stop(W stop)
{
	SCI6.SIMR3.BYTE = (stop)? 0x53 : 0x51;

	if (wait_flag(&start_complete_interrupt_flag, (stop)? 2 : 1) == 0)
		return 0;

	if (!stop)
		envision_i2c_trace[3] = SCI6.SIMR3.BYTE;

	if ((stop)) {
		SCI6.SIMR3.BIT.IICSCLS = 3;
		SCI6.SIMR3.BIT.IICSDAS = 3;
	} else {
		SCI6.SIMR3.BIT.IICSCLS = 0;
		SCI6.SIMR3.BIT.IICSDAS = 0;
	}
	return 1;
}


static	W	write_address(W address, W isread)
{
	SCI6.TDR = (UB)((address << 1) | (isread? 1 : 0));
	envision_i2c_trace[0] = SCI6.SSR.BYTE;

	/* TXI in simple IIC mode is raised after the acknowledge bit */
	if (wait_flag(&tx_complete_interrupt_flag, 5) == 0) {
		envision_i2c_trace[1] = SCI6.SSR.BYTE;
		return 0;
	}
	envision_i2c_trace[1] = SCI6.SSR.BYTE;
	envision_i2c_trace[2] = SCI6.SISR.BYTE;

	/* IICACKR is the ACK bit as received: 0 means the device answered */
	return (SCI6.SISR.BIT.IICACKR == 0)? 1 : 0;
}


static	W	i2c_write(W address, const UB *data, W length)
{
	W	ret, i;

	clear_flags();
	SCI6.SCR.BIT.RIE = 0;

	if (start_stop(0) == 0)
		return 0;

	ret = write_address(address, 0);
	if ((ret))
		for (i = 0; i < length; i++) {
			SCI6.TDR = data[i];
			if (wait_flag(&tx_complete_interrupt_flag, 6) == 0) {
				ret = 0;
				break;
			}
		}

	if (start_stop(1) == 0)
		ret = 0;

	return ret;
}


static	W	i2c_read(W address, UB *data, W length)
{
	W	ret, i;

	clear_flags();
	SCI6.SCR.BIT.RIE = 0;

	if (start_stop(0) == 0)
		return 0;

	ret = write_address(address, 1);
	if ((ret)) {
		SCI6.SIMR2.BIT.IICACKT = 0;		/* ACK each byte ... */
		SCI6.SCR.BIT.RIE = 1;

		for (i = 0; i < length; i++) {
			if (i == length - 1)
				SCI6.SIMR2.BIT.IICACKT = 1;	/* ... but NACK the last */

			/* reception is driven by a dummy transmission */
			SCI6.TDR = 0xff;

			if (wait_flag(&rx_complete_interrupt_flag, 7) == 0) {
				ret = 0;
				break;
			}
			data[i] = received_byte;

			if (wait_flag(&tx_complete_interrupt_flag, 8) == 0) {
				ret = 0;
				break;
			}
		}
	}

	SCI6.SCR.BIT.RIE = 0;
	if (start_stop(1) == 0)
		ret = 0;

	return ret;
}


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
		The reference waits 100ms here. The FT5x06 is documented as needing
		on the order of 300ms after reset before it will talk sensibly,
		and the reference gets away with less only because it never reads
		the controller until the interrupt line has gone low once - which
		is hundreds of milliseconds later anyway. Wait it out here so the
		gate is a safety net rather than the thing holding everything up.
	*/
	envision_delay_ms(300);

	/* P00/P01 to SSCL6/SSDA6 */
	MPC.PWPR.BIT.B0WI = 0;
	MPC.PWPR.BIT.PFSWE = 1;
	MPC.P01PFS.BYTE = 0x0a;
	MPC.P00PFS.BYTE = 0x0a;
	MPC.PWPR.BIT.PFSWE = 0;
	MPC.PWPR.BIT.B0WI = 1;

	/* clock to SCI6 */
	SYSTEM.PRCR.WORD = 0xa502;
	SYSTEM.MSTPCRB.BIT.MSTPB25 = 0;
	SYSTEM.PRCR.WORD = 0xa500;

	SCI6.SCR.BYTE = 0;

	/* both pins: high drive, no pull-up, NMOS open drain, peripheral */
	PORT0.DSCR.BIT.B1 = 1;
	PORT0.PCR.BIT.B1 = 0;
	PORT0.PDR.BIT.B1 = 0;
	PORT0.ODR0.BIT.B2 = 1;
	PORT0.PMR.BIT.B1 = 1;

	PORT0.DSCR.BIT.B0 = 1;
	PORT0.PCR.BIT.B0 = 0;
	PORT0.PDR.BIT.B0 = 0;
	PORT0.ODR0.BIT.B0 = 1;
	PORT0.PMR.BIT.B0 = 1;

	/* bus idle: both lines released */
	SCI6.SIMR3.BIT.IICSCLS = 3;
	SCI6.SIMR3.BIT.IICSDAS = 3;

	SCI6.SMR.BYTE = 0;			/* PCLKB undivided */
	SCI6.SCMR.BIT.SDIR = 1;			/* MSB first */
	SCI6.SCMR.BIT.SINV = 0;
	SCI6.SCMR.BIT.SMIF = 0;
	SCI6.BRR = 4;				/* ~400kHz from PCLKB 60MHz */

	SCI6.SEMR.BIT.NFEN = 1;			/* noise filter on */
	SCI6.SEMR.BIT.BRME = 0;
	SCI6.SNFR.BIT.NFCS = 1;

	SCI6.SIMR1.BIT.IICM = 1;		/* simple I2C */
	SCI6.SIMR1.BIT.IICDL = 18;		/* SDA output delay */
	SCI6.SIMR2.BIT.IICACKT = 1;
	SCI6.SIMR2.BIT.IICCSC = 1;		/* clock stretching honoured */
	SCI6.SIMR2.BIT.IICINTM = 1;		/* TXI/RXI mean TDRE/RDRF */
	SCI6.SPMR.BYTE = 0;

	/* TIE, TE, RE, TEIE - as the reference driver has it */
	SCI6.SCR.BYTE = 0xb4;

	/* group BL0 interrupt 12: TEI6, used for start/stop completion */
	ICU.GENBL0.BIT.EN12 = 1;
	IR(ICU, GROUPBL0) = 0;
	IPR(ICU, GROUPBL0) = 2;
	IEN(ICU, GROUPBL0) = 1;

	IR(SCI6, RXI6) = 0;
	IPR(SCI6, RXI6) = 5;
	IEN(SCI6, RXI6) = 1;

	IR(SCI6, TXI6) = 0;
	IPR(SCI6, TXI6) = 5;
	IEN(SCI6, TXI6) = 1;
}


W	envision_touch_get_raw(W *x, W *y)
{
	UB	buf[7];
	UB	reg;

	envision_i2c_trace[4] = 0;
	envision_i2c_trace[6] = PORT0.PIDR.BIT.B2;

	reg = 2;
	if (i2c_write(TOUCH_I2C_ADDRESS, &reg, 1) == 0)
		return 0;
	if (i2c_read(TOUCH_I2C_ADDRESS, buf, sizeof(buf)) == 0)
		return 0;

	envision_i2c_trace[5] = buf[0];
	envision_i2c_trace[7]++;
	{
		W	i;

		for (i = 0; i < 7; i++)
			envision_i2c_trace[8 + i] = buf[i];
	}

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
