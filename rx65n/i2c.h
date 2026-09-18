/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	Bit-banged I2C on P00/P01, in the shape of the pic32mx port's i2c.h.

	It replaces SCI6's simple-IIC mode, which is where both of this port's
	touch bugs came from - neither of them in the protocol, both in the
	peripheral's flag semantics. The transmit interrupt in simple IIC is
	raised after the acknowledge bit, so SSR.TDRE, and even SSR.TEND, is
	set well before it and the ACK slot gets read before the device has
	driven it. And RDRF is already set when the address frame ends, so a
	read loop that waits on it starts one byte early and every byte after
	it is one position out. Neither is visible from outside the chip; both
	took a debugger to find.

	Bit-banging has none of that to interpret. The acknowledge is "read the
	pin on the ninth clock", there is no TDRE, TEND, RDRF or IICSTIF, no
	ICU, and no interrupt handlers - which also drops this port's last
	build-time dependency on the upstream demo's inthandler.c.

	Slower, and it does not matter here: the panel is polled every 60ms and
	a transaction is nine bytes, so even at 40kHz it costs about 2ms.

	Both lines are open drain against the board's pull-ups, so driving a
	line means pulling it low and releasing it means letting it float up.
	Because the input register still reads the pin, the bus can be read
	back - which is what makes the acknowledge and clock stretching
	visible at all.
*/

#ifndef	RX65N_I2C_H
#define	RX65N_I2C_H

#include	"iodefine.h"
#include	"basic.h"

/*
	P00 is SSCL6 and P01 is SSDA6: in simple IIC the clock is on the SCIn
	receive pin and the data on its transmit pin, and SCI6's are P00 and
	P01 here. The upstream demo sets both up identically so it does not
	settle the question; if a device that is certainly present never
	acknowledges, swapping these two blocks is the first thing to try.

	ODR0 holds two bits per pin, so P00's open-drain control is B0 and
	P01's is B2.
*/
#define	SCL_PODR	PORT0.PODR.BIT.B0
#define	SCL_PIDR	PORT0.PIDR.BIT.B0
#define	SCL_PDR		PORT0.PDR.BIT.B0
#define	SCL_PMR		PORT0.PMR.BIT.B0
#define	SCL_PCR		PORT0.PCR.BIT.B0
#define	SCL_ODR		PORT0.ODR0.BIT.B0
#define	SCL_DSCR	PORT0.DSCR.BIT.B0

#define	SDA_PODR	PORT0.PODR.BIT.B1
#define	SDA_PIDR	PORT0.PIDR.BIT.B1
#define	SDA_PDR		PORT0.PDR.BIT.B1
#define	SDA_PMR		PORT0.PMR.BIT.B1
#define	SDA_PCR		PORT0.PCR.BIT.B1
#define	SDA_ODR		PORT0.ODR0.BIT.B2
#define	SDA_DSCR	PORT0.DSCR.BIT.B1

/*
	Half a bit time. Nominal, not calibrated: at 120MHz ICLK this is on the
	order of 10us, so the bus runs somewhere near 50kHz. I2C has no minimum
	clock, and slow is the safe direction.
*/
#ifndef	I2C_HALFBIT
#define	I2C_HALFBIT	150
#endif

/*
	How long to let a device hold the clock down. Bounded, like every other
	wait in this port: a line stuck low is a fault to report, not a reason
	to stop.
*/
#ifndef	I2C_STRETCH
#define	I2C_STRETCH	20000
#endif

/* set by i2cscl_high() when a device held the clock past the limit */
static	W	i2c_stretch_timeouts = 0;


static	void	i2cwait(void)
{
	volatile W	i;

	for (i = 0; i < I2C_HALFBIT; i++)
		;
}


/*
	Release the clock and wait for it to actually rise. A device that needs
	more time holds it down, and going on regardless would clock a bit the
	device never saw.
*/
static	void	i2cscl_high(void)
{
	W	n;

	SCL_PODR = 1;
	for (n = I2C_STRETCH; n > 0; n--)
		if ((SCL_PIDR))
			return;
	i2c_stretch_timeouts++;
}


static	void	i2cinit(void)
{
	/* both lines: GPIO, output, open drain, no internal pull-up, released */
	SCL_PMR = 0;
	SCL_PCR = 0;
	SCL_ODR = 1;
	SCL_DSCR = 1;
	SCL_PODR = 1;
	SCL_PDR = 1;

	SDA_PMR = 0;
	SDA_PCR = 0;
	SDA_ODR = 1;
	SDA_DSCR = 1;
	SDA_PODR = 1;
	SDA_PDR = 1;

	i2cwait();
}


static	void	i2cstart(void)
{
	/* works as a repeated start too: put both lines up first */
	SDA_PODR = 1;
	i2cwait();
	i2cscl_high();
	i2cwait();

	SDA_PODR = 0;		/* data falls while the clock is high */
	i2cwait();
	SCL_PODR = 0;
	i2cwait();
}


static	void	i2cstop(void)
{
	SCL_PODR = 0;
	i2cwait();
	SDA_PODR = 0;
	i2cwait();
	i2cscl_high();
	i2cwait();

	SDA_PODR = 1;		/* data rises while the clock is high */
	i2cwait();
}


/* returns 0 when the device acknowledged, 1 when it did not */
static	W	i2csend(W data)
{
	W	i, nak;

	for (i = 0; i < 8; i++) {
		SDA_PODR = (data & (0x80 >> i))? 1 : 0;
		i2cwait();
		i2cscl_high();
		i2cwait();
		SCL_PODR = 0;
		i2cwait();
	}

	SDA_PODR = 1;		/* let the device answer */
	i2cwait();
	i2cscl_high();
	i2cwait();
	nak = (SDA_PIDR)? 1 : 0;
	SCL_PODR = 0;
	i2cwait();

	return nak;
}


/* nak = 1 on the last byte of a read, so the device stops driving */
static	W	i2crecv(W nak)
{
	W	i, ret;

	SDA_PODR = 1;
	ret = 0;
	for (i = 0; i < 8; i++) {
		i2cwait();
		i2cscl_high();
		i2cwait();
		ret <<= 1;
		if ((SDA_PIDR))
			ret |= 1;
		SCL_PODR = 0;
	}

	i2cwait();
	SDA_PODR = (nak)? 1 : 0;
	i2cwait();
	i2cscl_high();
	i2cwait();
	SCL_PODR = 0;
	i2cwait();
	SDA_PODR = 1;

	return ret;
}

#endif
