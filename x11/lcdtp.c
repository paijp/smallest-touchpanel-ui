
/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2022-2024 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

#include	<time.h>
#include	<sys/time.h>
#include	<stdlib.h>
#include	<string.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<X11/Xlib.h>
#include	<X11/Xutil.h>
#include	<X11/Xatom.h>

#include	"lcdtp.h"

UW	lcdtp_flip = 0;
void	(*lcdtp_polltask)() = NULL;

const	struct	lcdtp_font_struct	lcdtp_font12h = {{NULL}, 
	{8, 12, 0, 11, 0x00, 0x60, 7, 
	0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x84, 0x00, 0x20, 
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 
	0x20, 0x01, 0x10, 0x01, 0xb0, 0x91, 0x20, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x30, 
	0x00, 0x01, 0x21, 0x27, 0xf2, 0x22, 0x27, 0xf2, 
	0x42, 0x42, 0x40, 0x04, 0x80, 0x00, 0x01, 0x07, 
	0xe9, 0x09, 0x07, 0xc1, 0x21, 0x2f, 0xc1, 0x00, 
	0x09, 0x40, 0x00, 0x1c, 0x14, 0x5c, 0x81, 0x02, 
	0x04, 0x09, 0xd1, 0x41, 0xc0, 0x13, 0x00, 0x00, 
	0x1c, 0x22, 0x22, 0x14, 0x08, 0x14, 0x22, 0xa1, 
	0x1e, 0x80, 0x27, 0x00, 0x18, 0x08, 0x10, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 
	0x00, 0x08, 0x10, 0x10, 0x20, 0x20, 0x20, 0x20, 
	0x10, 0x10, 0x08, 0x00, 0xa4, 0x00, 0x40, 0x20, 
	0x20, 0x10, 0x10, 0x10, 0x10, 0x20, 0x20, 0x40, 
	0x01, 0x50, 0x00, 0x00, 0x00, 0x41, 0x50, 0xe0, 
	0x40, 0xe1, 0x50, 0x40, 0x00, 0x02, 0xb0, 0x00, 
	0x00, 0x00, 0x00, 0x80, 0x83, 0xe0, 0x80, 0x80, 
	0x00, 0x00, 0x05, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x02, 0x0b, 
	0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 
	0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c, 
	0x00, 0x2f, 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 
	0x08, 0x10, 0x20, 0x40, 0x00, 0x00, 0x60, 0x00, 
	0x7c, 0x82, 0x86, 0x8a, 0x92, 0xa2, 0xc2, 0x82, 
	0x82, 0x7c, 0x00, 0xc4, 0x00, 0x20, 0x60, 0x20, 
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70, 0x01, 
	0x90, 0x01, 0xf2, 0x0a, 0x08, 0x08, 0x10, 0x20, 
	0x40, 0x81, 0x03, 0xf8, 0x03, 0x30, 0x03, 0xe4, 
	0x10, 0x10, 0x10, 0xe0, 0x10, 0x10, 0x14, 0x13, 
	0xe0, 0x06, 0x80, 0x00, 0x40, 0xc1, 0x42, 0x44, 
	0x48, 0x4f, 0xe0, 0x40, 0x40, 0x40, 0x0d, 0x40, 
	0x1f, 0xd0, 0x10, 0x10, 0x1f, 0x90, 0x40, 0x40, 
	0x50, 0x4f, 0x80, 0x1b, 0x00, 0x1f, 0x20, 0xa0, 
	0x20, 0x3f, 0x20, 0xa0, 0xa0, 0xa0, 0x9f, 0x00, 
	0x37, 0x00, 0x7f, 0x41, 0x41, 0x42, 0x02, 0x04, 
	0x04, 0x08, 0x08, 0x08, 0x00, 0x70, 0x00, 0x7c, 
	0x82, 0x82, 0x82, 0x7c, 0x82, 0x82, 0x82, 0x82, 
	0x7c, 0x00, 0xe4, 0x00, 0xf9, 0x05, 0x05, 0x05, 
	0x04, 0xfc, 0x04, 0x04, 0x04, 0xf8, 0x01, 0xd0, 
	0x00, 0x00, 0x00, 0x00, 0xc0, 0xc0, 0x00, 0x00, 
	0xc0, 0xc0, 0x00, 0x03, 0xb0, 0x00, 0x00, 0x00, 
	0x01, 0x81, 0x80, 0x00, 0x01, 0x80, 0x81, 0x00, 
	0x07, 0x80, 0x00, 0x00, 0x40, 0x81, 0x02, 0x04, 
	0x02, 0x01, 0x00, 0x80, 0x40, 0x0f, 0x40, 0x00, 
	0x00, 0x00, 0x00, 0x0f, 0x80, 0x00, 0x0f, 0x80, 
	0x00, 0x00, 0x1f, 0x00, 0x00, 0x10, 0x08, 0x04, 
	0x02, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x3f, 
	0x00, 0x00, 0x1c, 0x22, 0x02, 0x02, 0x04, 0x08, 
	0x08, 0x00, 0x08, 0x00, 0x80, 0x00, 0x00, 0x7c, 
	0x82, 0x9a, 0xaa, 0xaa, 0xaa, 0x94, 0x80, 0x7c, 
	0x01, 0x04, 0x00, 0x20, 0x20, 0x50, 0x50, 0x88, 
	0xf8, 0x89, 0x05, 0x05, 0x04, 0x02, 0x10, 0x03, 
	0xf1, 0x09, 0x09, 0x09, 0xf1, 0x09, 0x09, 0x09, 
	0x0b, 0xf0, 0x04, 0x30, 0x03, 0xe4, 0x14, 0x04, 
	0x04, 0x04, 0x04, 0x04, 0x04, 0x13, 0xe0, 0x08, 
	0x80, 0x0f, 0x84, 0x44, 0x24, 0x24, 0x24, 0x24, 
	0x24, 0x24, 0x4f, 0x80, 0x11, 0x40, 0x1f, 0xc8, 
	0x08, 0x08, 0x0f, 0x88, 0x08, 0x08, 0x08, 0x1f, 
	0xc0, 0x23, 0x00, 0x3f, 0x90, 0x10, 0x10, 0x1f, 
	0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x47, 0x00, 
	0x3e, 0x41, 0x40, 0x40, 0x4f, 0x41, 0x41, 0x41, 
	0x43, 0x3d, 0x00, 0x90, 0x00, 0x82, 0x82, 0x82, 
	0x82, 0xfe, 0x82, 0x82, 0x82, 0x82, 0x82, 0x01, 
	0x24, 0x00, 0x70, 0x20, 0x20, 0x20, 0x20, 0x20, 
	0x20, 0x20, 0x20, 0x70, 0x02, 0x50, 0x00, 0x38, 
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x12, 0x11, 
	0xe0, 0x04, 0xb0, 0x04, 0x24, 0x44, 0x85, 0x06, 
	0x05, 0x04, 0x84, 0x44, 0x24, 0x10, 0x09, 0x80, 
	0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 
	0x04, 0x2f, 0xe0, 0x13, 0x40, 0x10, 0x58, 0xd5, 
	0x52, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x40, 
	0x27, 0x00, 0x20, 0xb0, 0xa8, 0xa4, 0xa2, 0xa1, 
	0xa0, 0xa0, 0xa0, 0xa0, 0x80, 0x4f, 0x00, 0x3e, 
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 
	0x3e, 0x00, 0xa0, 0x00, 0xfc, 0x82, 0x82, 0x82, 
	0xfc, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01, 0x44, 
	0x00, 0xf9, 0x05, 0x05, 0x05, 0x05, 0x05, 0x25, 
	0x15, 0x0c, 0xf8, 0x02, 0x90, 0x03, 0xf2, 0x0a, 
	0x0a, 0x0a, 0x0b, 0xf2, 0x42, 0x22, 0x12, 0x08, 
	0x05, 0x30, 0x03, 0xe4, 0x14, 0x04, 0x03, 0x80, 
	0x60, 0x10, 0x14, 0x13, 0xe0, 0x0a, 0x80, 0x0f, 
	0xe1, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
	0x01, 0x00, 0x15, 0x40, 0x10, 0x50, 0x50, 0x50, 
	0x50, 0x50, 0x50, 0x50, 0x50, 0x4f, 0x80, 0x2b, 
	0x00, 0x20, 0xa0, 0x91, 0x11, 0x11, 0x0a, 0x0a, 
	0x0a, 0x04, 0x04, 0x00, 0x57, 0x00, 0x41, 0x41, 
	0x49, 0x49, 0x55, 0x55, 0x55, 0x22, 0x22, 0x22, 
	0x00, 0xb0, 0x00, 0x82, 0x82, 0x44, 0x28, 0x10, 
	0x28, 0x44, 0x82, 0x82, 0x82, 0x01, 0x64, 0x01, 
	0x05, 0x04, 0x88, 0x50, 0x20, 0x20, 0x20, 0x20, 
	0x20, 0x20, 0x02, 0xd0, 0x03, 0xf8, 0x08, 0x10, 
	0x20, 0x40, 0x40, 0x81, 0x02, 0x03, 0xf8, 0x05, 
	0xb0, 0x01, 0xe1, 0x01, 0x01, 0x01, 0x01, 0x01, 
	0x01, 0x01, 0x01, 0xe0, 0x0b, 0x80, 0x00, 0x00, 
	0x08, 0x04, 0x02, 0x01, 0x00, 0x80, 0x40, 0x20, 
	0x00, 0x17, 0x40, 0x0f, 0x01, 0x01, 0x01, 0x01, 
	0x01, 0x01, 0x01, 0x01, 0x0f, 0x00, 0x2f, 0x00, 
	0x04, 0x0a, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x5f, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 
	0xc0, 0x00, 0x20, 0x10, 0x08, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0x84, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x78, 0x84, 0x3c, 0x44, 0x84, 
	0x7c, 0x03, 0x10, 0x03, 0x01, 0x01, 0x01, 0x01, 
	0xf1, 0x09, 0x09, 0x09, 0x0b, 0xf0, 0x06, 0x30, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0xe2, 0x12, 0x02, 
	0x02, 0x11, 0xe0, 0x0c, 0x80, 0x00, 0x60, 0x20, 
	0x20, 0x23, 0xe4, 0x24, 0x24, 0x24, 0x23, 0xe0, 
	0x19, 0x40, 0x00, 0x00, 0x00, 0x00, 0x07, 0x88, 
	0x4f, 0xc8, 0x08, 0x47, 0x80, 0x33, 0x00, 0x03, 
	0x04, 0x84, 0x04, 0x0f, 0x04, 0x04, 0x04, 0x04, 
	0x04, 0x00, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x1f, 0x21, 0x21, 0x1f, 0x01, 0x21, 0x1e, 0xd0, 
	0x00, 0x40, 0x40, 0x40, 0x40, 0x7c, 0x42, 0x42, 
	0x42, 0x42, 0x42, 0x01, 0xa4, 0x00, 0x00, 0x00, 
	0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x78, 
	0x03, 0x50, 0x00, 0x00, 0x00, 0x10, 0x00, 0x30, 
	0x10, 0x10, 0x10, 0x10, 0x90, 0x66, 0xb0, 0x02, 
	0x02, 0x02, 0x02, 0x02, 0x22, 0x42, 0x83, 0x42, 
	0x22, 0x10, 0x0d, 0x80, 0x01, 0x80, 0x80, 0x80, 
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x40, 0x1b, 
	0x40, 0x00, 0x00, 0x00, 0x00, 0x1d, 0x92, 0x52, 
	0x52, 0x52, 0x52, 0x40, 0x37, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x1f, 0x10, 0x90, 0x90, 0x90, 0x90, 
	0x80, 0x6f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 
	0x21, 0x21, 0x21, 0x21, 0x1e, 0x00, 0xe0, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x7c, 0x42, 0x42, 0x42, 
	0x7c, 0x40, 0x41, 0xc4, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x7c, 0x84, 0x84, 0x84, 0x7c, 0x04, 0x07, 
	0x90, 0x00, 0x00, 0x00, 0x00, 0x01, 0x71, 0x89, 
	0x01, 0x01, 0x01, 0x00, 0x07, 0x30, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0xf2, 0x01, 0x80, 0x60, 0x13, 
	0xe0, 0x0e, 0x80, 0x00, 0x00, 0x01, 0x01, 0x03, 
	0xc1, 0x01, 0x01, 0x01, 0x00, 0xc0, 0x1d, 0x40, 
	0x00, 0x00, 0x00, 0x00, 0x08, 0x48, 0x48, 0x48, 
	0x48, 0x47, 0xc0, 0x3b, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x20, 0x91, 0x11, 0x0a, 0x0a, 0x04, 0x00, 
	0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x49, 
	0x55, 0x55, 0x36, 0x22, 0x00, 0xf0, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x42, 0x24, 0x18, 0x18, 0x24, 
	0x42, 0x01, 0xe4, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x84, 0x88, 0x50, 0x50, 0x20, 0x40, 0x83, 0xd0, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0xf8, 0x10, 0x20, 
	0x40, 0x81, 0xf8, 0x07, 0xb0, 0x00, 0x60, 0x80, 
	0x80, 0x81, 0x00, 0x80, 0x80, 0x80, 0x80, 0x60, 
	0x0f, 0x80, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
	0x01, 0x01, 0x01, 0x01, 0x00, 0x1f, 0x40, 0x0c, 
	0x02, 0x02, 0x02, 0x01, 0x02, 0x02, 0x02, 0x02, 
	0x0c, 0x00, 0x3f, 0x00, 0x0c, 0x93, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x1c, 0x1c, 
	0x00, 0x00, 0x00, 0x00, 0x00}
};

static	Display	*xd0;
static	Window	xw0;
static	GC	gc0;

static	UW	screen[LCD_H][LCD_W];
static	XImage	*xi0;


void	dly_tsk(W ms)
{
	if ((lcdtp_polltask))
		lcdtp_polltask();
	usleep(1000 * ms);
}


static	void	update_lcd()
{
	if ((lcdtp_polltask))
		lcdtp_polltask();
	XPutImage(xd0, xw0, gc0, xi0, 0, 0, 0, 0, LCD_W, LCD_H);
	XFlush(xd0);
}


static	UW	color16to32(UW color)
{
	UW	ret;
	
		/* 16: -------- -------- RRRRRGGG GGGBBBBB */
		/* 32: -------- RRRRRrrr GGGGGGgg BBBBBbbb */
	ret = (color << 8) & 0x00f80000;
	ret |= (color << 3) & 0x00070000;
	ret |= (color << 5) & 0x0000fc00;
	ret |= (color >> 1) & 0x00000300;
	ret |= (color << 3) & 0x000000f8;
	ret |= (color >> 2) & 0x00000007;
	
	return ret;
}


void	gfil_rec(W l, W t, W r, W b, UW color16)
{
	UW	color;
	UW	*p;
	W	x, y;
	
	color = color16to32(color16);
	
	if (l < 0)
		l = 0;
	if (r >= LCD_W)
		r = LCD_W - 1;
	if (t < 0)
		t = 0;
	if (b >= LCD_H)
		b = LCD_H - 1;
	
	for (y=t; y<b; y++) {
		p = screen[y] + l;
		for (x=l; x<r; x++)
			*(p++) = color;
	}
	update_lcd();
}


static	const	UB	*bitbuf = NULL;
static	const	UB	*bitp = NULL;
static	W	bitpos = 0;


static	void	setbitbuf(const unsigned char *p, W pos)
{
	if ((p))
		bitbuf = p;
	bitpos = pos;
	bitp = bitbuf + (pos >> 3);
}


static	W	getbit(W size)
{
	W	ret;
	
	ret = 0;
	while (size-- > 0) {
		ret <<= 1;
		if (((*bitp << (bitpos & 7)) & 0x80))
			ret |= 1;
		if ((++bitpos & 7) == 0)
			bitp++;
	}
	return ret;
}


static	W	findcode(W code)
{
	W	pos, mask, size;
	W	bitsize;
	W	codebits;
	W	c;
	
	setbitbuf(NULL, 0);
	bitsize = getbit(8);		/* width */
	bitsize *= getbit(8);		/* height */
	setbitbuf(NULL, 8 * 4);
	size = getbit(16);
	codebits = getbit(8);
	bitsize += codebits;		/* code */
	
	c = getbit(codebits);			/* first code */
	if (code < c)
		return 0;		/* out of range */
	if (code == c)
		return 1;		/* found */
	
	setbitbuf(NULL, 8 * 4 + 16 + 8 + bitsize * (size - 1));
	c = getbit(codebits);			/* last code */
	if (code > c)
		return 0;		/* out of range */
	if (code == c)
		return 1;		/* found */
	
	pos = 0;
	mask = 0x10000;
	while ((mask)) {
		mask >>= 1;
		if ((pos | mask) >= size)
			continue;
		setbitbuf(NULL, 8 * 4 + 16 + 8 + bitsize * (pos | mask));
		c = getbit(codebits);	
		if (code == c)
			return 1;		/* found */
		if (code > c)
			pos |= mask;
	}
	return 0;		/* not found */
}


void	gdra_stp(W x, W y, UW color16, UW bgcolor16, const struct lcdtp_font_struct *font, const UB *s)
{
	UW	color, bgcolor;
	UW	*p;
	
	if (s == NULL)
		return;
	
	if (font == NULL)
		font = &lcdtp_font12h;
	
	color = color16to32(color16);
	bgcolor = color16to32(bgcolor16);
	
	for (;;) {
		const	struct	lcdtp_font_struct	*f;
		W	c;
		
		if ((c = *(s++)) < 0x80) {
			if (c == 0)
				break;
		} else if ((c & 0xe0) == 0xc0)
			c = ((c & 0x1f) << 6) | (*(s++) & 0x3f);
		else if ((c & 0xf0) == 0xe0) {
			c = (c & 0xf) << 12;
			c |= (*(s++) & 0x3f) << 6;
			c |= *(s++) & 0x3f;
		} else if ((c & 0xf8) == 0xf0) {
			c = (c & 7) << 18;
			c |= (*(s++) & 0x3f) << 12;
			c |= (*(s++) & 0x3f) << 6;
			c |= *(s++) & 0x3f;
		} else if ((c & 0xfc) == 0xf8) {
			c = (c & 3) << 24;
			c |= (*(s++) & 0x3f) << 18;
			c |= (*(s++) & 0x3f) << 12;
			c |= (*(s++) & 0x3f) << 6;
			c |= *(s++) & 0x3f;
		} else if ((c & 0xfe) == 0xfc) {
			c = (c & 1) << 30;
			c |= (*(s++) & 0x3f) << 24;
			c |= (*(s++) & 0x3f) << 18;
			c |= (*(s++) & 0x3f) << 12;
			c |= (*(s++) & 0x3f) << 6;
			c |= *(s++) & 0x3f;
		} else
			continue;
		
		f = font;
		while ((f)) {
			W	fw, fh, offx, offy;
			
			setbitbuf(f->font, 0);
			fw = getbit(8);
			fh = getbit(8);
			offx = getbit(8);
			offy = getbit(8);
			if (findcode(c) == 0) {
				f = f->attr.fallback;
				continue;
			}
			do {
				W	x0, y0, x1, y1;
				
				x0 = x - offx;
				y0 = y - offy;
				if (x0 < 0)
					break;
				if (x0 + fw >= LCD_W)
					return;
				if (y0 < 0)
					break;
				if (y0 + fh >= LCD_H)
					break;
				
				for (y1=0; y1<fh; y1++) {
					p = screen[y0 + y1] + x0;
					for (x1=0; x1<fw; x1++)
						p[x1] = (getbit(1))? color : bgcolor;
				}
			} while (0);
			x += fw;
			break;
		}
	}
	update_lcd();
}


W	gget_stw(const struct lcdtp_font_struct *font, const UB *s)
{
	W	ret;
	
	if (s == NULL)
		return 0;
	
	if (font == NULL)
		font = &lcdtp_font12h;
	
	ret = 0;
	for (;;) {
		const	struct	lcdtp_font_struct	*f;
		W	c;
		
		if ((c = *(s++)) < 0x80) {
			if (c == 0)
				break;
		} else if ((c & 0xe0) == 0xc0)
			c = ((c & 0x1f) << 6) | (*(s++) & 0x3f);
		else if ((c & 0xf0) == 0xe0) {
			c = (c & 0xf) << 12;
			c |= (*(s++) & 0x3f) << 6;
			c |= *(s++) & 0x3f;
		} else if ((c & 0xf8) == 0xf0) {
			c = (c & 7) << 18;
			c |= (*(s++) & 0x3f) << 12;
			c |= (*(s++) & 0x3f) << 6;
			c |= *(s++) & 0x3f;
		} else if ((c & 0xfc) == 0xf8) {
			c = (c & 3) << 24;
			c |= (*(s++) & 0x3f) << 18;
			c |= (*(s++) & 0x3f) << 12;
			c |= (*(s++) & 0x3f) << 6;
			c |= *(s++) & 0x3f;
		} else if ((c & 0xfe) == 0xfc) {
			c = (c & 1) << 30;
			c |= (*(s++) & 0x3f) << 24;
			c |= (*(s++) & 0x3f) << 18;
			c |= (*(s++) & 0x3f) << 12;
			c |= (*(s++) & 0x3f) << 6;
			c |= *(s++) & 0x3f;
		} else
			continue;
		
		f = font;
		while ((f)) {
			W	fw;
			
			setbitbuf(f->font, 0);
			fw = getbit(8);
			if (findcode(c) == 0) {
				f = f->attr.fallback;
				continue;
			}
			ret += fw;
			break;
		}
	}
	
	return ret;
}


UW	gettp()
{
	static	W	ispress = 1;
	W	x, y, type;
	XEvent	ev0;
	
	usleep(61 * 6);
	for (;;) {
		if ((lcdtp_polltask))
			lcdtp_polltask();
		if (!XCheckMaskEvent(xd0, ButtonPressMask|ButtonReleaseMask|Button1MotionMask, &ev0))
			return TPLIB_CMD_NULL;
		
		type = TPLIB_CMD_PRESS;
		switch (ev0.type) {
			case	MotionNotify:
				if (ispress == 0)
					break;
				type = TPLIB_CMD_PRESSING;
				
			case	ButtonPress:
				ispress = 1;
				if ((lcdtp_flip & LCDTP_FLIP_X))
					x = LCD_W - 1 - ev0.xbutton.x;
				else
					x = ev0.xbutton.x;
				if ((lcdtp_flip & LCDTP_FLIP_Y))
					y = LCD_H - 1 - ev0.xbutton.y;
				else
					y = ev0.xbutton.y;
				return type | (x << 12) | y;
			case	ButtonRelease:
				ispress = 0;
				break;
			case	Expose:
				update_lcd();
				break;
		}
	}
}


void	init_lcdtp()
{
	if ((xd0 = XOpenDisplay(NULL)) == NULL) {
		fprintf(stderr, "XOpenDisplay failed.\n");
		exit(1);
	}
	xi0 = XCreateImage(xd0, DefaultVisual(xd0, 0), DisplayPlanes(xd0, 0), ZPixmap, 0, (void*)screen, LCD_W, LCD_H, 8, 0);
	xw0 = XCreateSimpleWindow(xd0, RootWindow(xd0, 0), 200, 40, LCD_W, LCD_H, 1, WhitePixel(xd0, 0), BlackPixel(xd0, 0));
	{
		XTextProperty	win_name;
		
		win_name.value = (unsigned char*)"lcdtp";
		win_name.encoding = XA_STRING;
		win_name.format = 8;
		win_name.nitems = strlen((char*)win_name.value);
		XSetWMName(xd0, xw0, &win_name);
	}
	gc0 = XCreateGC(xd0, xw0, 0, 0);
	XSelectInput(xd0, xw0, ExposureMask|ButtonPressMask|ButtonReleaseMask|Button1MotionMask);
	XMapSubwindows(xd0, xw0);
	XMapWindow(xd0, xw0);
	update_lcd();
	dly_tsk(500);
}


void	lcdtp_sendlogc(W c)
{
	fputc(c, stderr);
}


void	lcdtp_sendlogs(const char *s)
{
	W	c;
	
	while ((c = *(s++)))
		lcdtp_sendlogc(c);
}


void	lcdtp_sendlogdec(W v)
{
	if ((v < 0)) {
		v = -v;
		lcdtp_sendlogc('-');
	}
	if (v >= 10)
		lcdtp_sendlogdec(v / 10);
	lcdtp_sendlogc('0' + (v % 10));
}


void	lcdtp_sendlogun(UW v)
{
	static	const	char *bin2hex = "0123456789abcdef";
	
	lcdtp_sendlogc(bin2hex[v & 0xf]);
}


void	lcdtp_sendlogub(UW v)
{
	lcdtp_sendlogun(v >> 4);
	lcdtp_sendlogun(v);
}


void	lcdtp_sendloguh(UW v)
{
	lcdtp_sendlogun(v >> 12);
	lcdtp_sendlogun(v >> 8);
	lcdtp_sendlogun(v >> 4);
	lcdtp_sendlogun(v);
}


void	lcdtp_sendloguw(UW v)
{
	lcdtp_sendlogun(v >> 28);
	lcdtp_sendlogun(v >> 24);
	lcdtp_sendlogun(v >> 20);
	lcdtp_sendlogun(v >> 16);
	lcdtp_sendlogun(v >> 12);
	lcdtp_sendlogun(v >> 8);
	lcdtp_sendlogun(v >> 4);
	lcdtp_sendlogun(v);
}


#ifdef STANDALONE
int	main(int ac, char **av)
{
	W	x, y, c;
	
	init_lcdtp();
	
	for (y=0; y<8; y++)
		for (x=0; x<8; x++) {
			c = 0x5000 * (x & 3);
			c |= (x & 4)? 0x0540 : 0;
			c |= (y & 4)? 0x01a0 : 0;
			c |= 0xa * (y & 3);
			gfil_rec(x * 30, y * 40, x * 30 + 30, y * 40 + 40, c);
		}
	
	for (y=0; y<0x60; y++) {
		static	UB	buf[2] = {0, 0};
		
		buf[0] = y + 0x20;
		gdra_stp(0, y * 12 + 11, 0x07e0, 0x001f, 0, buf);
	}
	
#if 0
	for (;;) {
		if ((y = gettp()) == 0) {
			dly_tsk(100);
			continue;
		}
		x = y >> 12;
		y &= 0xfff;
		screen[y][x] = 0xffff;
		update_lcd();
	}
#else
	while (gettp() == 0)
		dly_tsk(100);
#endif
	
	return 0;
}
#endif


