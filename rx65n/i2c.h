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
	The clock and the data are on P00 and P01, but which is which is not
	settled by anything to hand. In simple IIC the clock rides on the SCIn
	receive pin and the data on its transmit pin, and SCI6's are these two;
	the upstream demo sets both to the same alternate function, so it does
	not say either. An earlier version of this file picked SCL=P00 and
	stated it as fact, and the panel answered nothing: every address frame
	went unacknowledged while the interrupt line showed touches arriving.

	So it is chosen at run time instead. i2cprobe() tries one way round,
	and if nothing answers, the other. Which one won is in the trace, so
	the board says which it is rather than being told.
*/
static	W	i2c_swap = 0;		/* 0: SCL=P00 SDA=P01, 1: the other way */

static	void	scl_set(W v)
{
	if ((i2c_swap))
		PORT0.PODR.BIT.B1 = (UB)v;
	else
		PORT0.PODR.BIT.B0 = (UB)v;
}

static	W	scl_get(void)
{
	return (i2c_swap)? PORT0.PIDR.BIT.B1 : PORT0.PIDR.BIT.B0;
}

static	void	sda_set(W v)
{
	if ((i2c_swap))
		PORT0.PODR.BIT.B0 = (UB)v;
	else
		PORT0.PODR.BIT.B1 = (UB)v;
}

static	W	sda_get(void)
{
	return (i2c_swap)? PORT0.PIDR.BIT.B0 : PORT0.PIDR.BIT.B1;
}

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

extern	void	(*lcdtp_polltask)();		/* lcdtp.c */


/*
	Half a bit, and the caller's poll task while we are waiting anyway.

	Safe, and worth doing. I2C is clocked by the master, so stretching any
	phase of it is legal - the bus has no minimum clock and a slave has
	nothing of its own to keep in step with. The one rule is the obvious
	one: whatever runs in here must not touch SDA or SCL. Moving SDA while
	SCL is high is a start or a stop condition, which would end the
	transaction underneath us.

	Worth doing because a transaction is about 2ms, and a serial port left
	unread for 2ms can lose bytes. That is the job lcdtp_polltask() exists
	for, and it should not be starved by a touch read.
*/
static	void	i2cwait(void)
{
	volatile W	i;

	if ((lcdtp_polltask))
		lcdtp_polltask();

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

	scl_set(1);
	for (n = I2C_STRETCH; n > 0; n--)
		if ((scl_get()))
			return;
	i2c_stretch_timeouts++;
}


/*
	Both pins get the same treatment - GPIO, output, open drain, no internal
	pull-up, released - so this does not need to know which is which. ODR0
	holds two bits per pin, hence B0 for P00 and B2 for P01.
*/
static	void	i2cinit(void)
{
	PORT0.PMR.BIT.B0 = 0;
	PORT0.PCR.BIT.B0 = 0;
	PORT0.ODR0.BIT.B0 = 1;
	PORT0.DSCR.BIT.B0 = 1;
	PORT0.PODR.BIT.B0 = 1;
	PORT0.PDR.BIT.B0 = 1;

	PORT0.PMR.BIT.B1 = 0;
	PORT0.PCR.BIT.B1 = 0;
	PORT0.ODR0.BIT.B2 = 1;
	PORT0.DSCR.BIT.B1 = 1;
	PORT0.PODR.BIT.B1 = 1;
	PORT0.PDR.BIT.B1 = 1;

	i2cwait();
}


static	void	i2cstart(void)
{
	/* works as a repeated start too: put both lines up first */
	sda_set(1);
	i2cwait();
	i2cscl_high();
	i2cwait();

	sda_set(0);		/* data falls while the clock is high */
	i2cwait();
	scl_set(0);
	i2cwait();
}


static	void	i2cstop(void)
{
	scl_set(0);
	i2cwait();
	sda_set(0);
	i2cwait();
	i2cscl_high();
	i2cwait();

	sda_set(1);		/* data rises while the clock is high */
	i2cwait();
}


/* returns 0 when the device acknowledged, 1 when it did not */
static	W	i2csend(W data)
{
	W	i, nak;

	for (i = 0; i < 8; i++) {
		sda_set((data & (0x80 >> i))? 1 : 0);
		i2cwait();
		i2cscl_high();
		i2cwait();
		scl_set(0);
		i2cwait();
	}

	sda_set(1);		/* let the device answer */
	i2cwait();
	i2cscl_high();
	i2cwait();
	nak = (sda_get())? 1 : 0;
	scl_set(0);
	i2cwait();

	return nak;
}


/* nak = 1 on the last byte of a read, so the device stops driving */
static	W	i2crecv(W nak)
{
	W	i, ret;

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
	Ask the device at addr whether it is there, both ways round, and keep
	whichever answered.

	An address frame and nothing else: start, the address with the write
	bit, look at the acknowledge, stop. A device that is present drives the
	ninth clock low; with the clock and the data crossed over, nothing can,
	because the address is being shifted out on the line the device is
	listening to for a clock.

	Returns 1 when something answered, and leaves i2c_swap set to the
	mapping that worked. Returns 0 when neither way round answered, which
	is a different fault - wrong address, held reset, no pull-ups - and not
	one this can decide between.
*/
static	W	i2cprobe(W addr)
{
	W	tries;

	for (tries = 0; tries < 2; tries++) {
		i2cinit();
		i2cstart();
		if (!i2csend(addr << 1)) {
			i2cstop();
			return 1;
		}
		i2cstop();
		i2c_swap = !i2c_swap;
	}
	return 0;
}

#endif
