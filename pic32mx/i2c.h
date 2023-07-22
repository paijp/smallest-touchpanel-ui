
/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2022-2023 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/


#include	<xc.h>


#ifndef	PORT_SDA
#define	PORT_SDA	PORTBbits.RB7
#endif
#ifndef	LAT_SDA
#define	LAT_SDA	LATBbits.LATB7
#endif
#ifndef	TRIS_SDA
#define	TRIS_SDA	TRISBbits.TRISB7
#endif
#ifndef	LAT_SCL
#define	LAT_SCL	LATAbits.LATA4
#endif
#ifndef	TRIS_SCL
#define	TRIS_SCL	TRISAbits.TRISA4
#endif


extern	void	(*lcdtp_polltask)();		/* lcdtp.c */


static	void	wait10us(void)
{
	long    l;
	
	if ((lcdtp_polltask))
		lcdtp_polltask();
	
	for (l=150; l>0; l--)
		asm("nop");
}


static	void	i2cstop()
{
	wait10us();
	LAT_SCL = 0;
	TRIS_SCL = 0;
	wait10us();
	LAT_SDA = 0;
	TRIS_SDA = 0;
	wait10us();
	LAT_SCL = 1;		/* cmos */
	wait10us();
	LAT_SDA = 1;
	TRIS_SDA = 0;		/* for touch panel */
	wait10us();
}


static	void	i2cstart()
{
	wait10us();
	TRIS_SDA = 1;		/* open-drain */	/* for restart */
	LAT_SDA = 0;
	wait10us();
	LAT_SCL = 1;
	wait10us();
	TRIS_SDA = 0;
	wait10us();
	LAT_SCL = 0;
	wait10us();
}


static	W	i2csend(W data)
{
	W	i, nak;
	
	for (i=0; i<8; i++) {
		TRIS_SDA = (data & (0x80 >> i))? 1 : 0;
		wait10us();
		LAT_SCL = 1;
		wait10us();
		LAT_SCL = 0;
		wait10us();
	}
	TRIS_SDA = 1;
	wait10us();
	LAT_SCL = 1;
	wait10us();
	nak = (PORT_SDA)? 1 : 0;
	LAT_SCL = 0;
	wait10us();
	TRIS_SDA = 0;
	wait10us();
	return nak;
}


static	W	i2crecv(W nak)
{
	W	i, ret;
	
	ret = 0;
	TRIS_SDA = 1;
	for (i=0; i<8; i++) {
		wait10us();
		LAT_SCL = 1;
		wait10us();
		ret <<= 1;
		if ((PORT_SDA))
			ret |= 1;
		LAT_SCL = 0;
	}
	wait10us();
	TRIS_SDA = (nak)? 1 : 0;
	wait10us();
	LAT_SCL = 1;
	wait10us();
	LAT_SCL = 0;
	wait10us();
	return ret;
}

