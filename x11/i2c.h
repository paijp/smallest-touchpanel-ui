
/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2022 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/


#include	<stdio.h>


static	void	i2cstart()
{
	fprintf(stderr, "i2cstart() ========\n");
}


static	void	i2cstop()
{
	fprintf(stderr, "i2cstop()\n");
}


static	W	i2csend(W data)
{
	W	ret;
	
	ret = 0;	/* ack */
	fprintf(stderr, "i2csend(%d) data(%02x)\n", ret, data);
	return ret;
}


static	W	i2crecv(W nak)
{
	static	UB	val = 0x40;
	
	val++;
	fprintf(stderr, "i2crecv(%02x) nak(%d)\n", val, nak);
	return val;
}



