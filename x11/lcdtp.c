
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

static	Display	*xd0;
static	Window	xw0;
static	GC	gc0;

static	UW	screen[LCD_H][LCD_W];
static	XImage	*xi0;


static	const	UB	font12n[0x60][12] = {
		/* 0x20 */
	{0},
	{0, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0},
	{0, 0x36, 0x12, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x00, 0x12, 0x12, 0x7f, 0x22, 0x22, 0x7f, 0x24, 0x24, 0x24, 0},
	{0, 0x00, 0x08, 0x3f, 0x48, 0x48, 0x3e, 0x09, 0x09, 0x7e, 0x08, 0},
	{0, 0x00, 0x70, 0x51, 0x72, 0x04, 0x08, 0x10, 0x27, 0x45, 0x07, 0},
	{0, 0x00, 0x38, 0x44, 0x44, 0x28, 0x10, 0x28, 0x45, 0x42, 0x3d, 0},
	{0, 0x18, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x04, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x04, 0},
	{0, 0x10, 0x08, 0x08, 0x04, 0x04, 0x04, 0x04, 0x08, 0x08, 0x10, 0},
	{0, 0x00, 0x00, 0x08, 0x2a, 0x1c, 0x08, 0x1c, 0x2a, 0x08, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3e, 0x08, 0x08, 0x00, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x08, 0x10},
	{0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0},
	{0, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00, 0},
		/* 0x30 */
	{0, 0x3e, 0x41, 0x43, 0x45, 0x49, 0x51, 0x61, 0x41, 0x41, 0x3e, 0},
	{0, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1c, 0},
	{0, 0x3e, 0x41, 0x41, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x7f, 0},
	{0, 0x3e, 0x41, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x01, 0x41, 0x3e, 0},
	{0, 0x02, 0x06, 0x0a, 0x12, 0x22, 0x42, 0x7f, 0x02, 0x02, 0x02, 0},
	{0, 0x7f, 0x40, 0x40, 0x40, 0x7e, 0x41, 0x01, 0x01, 0x41, 0x3e, 0},
	{0, 0x3e, 0x41, 0x40, 0x40, 0x7e, 0x41, 0x41, 0x41, 0x41, 0x3e, 0},
	{0, 0x7f, 0x41, 0x41, 0x42, 0x02, 0x04, 0x04, 0x08, 0x08, 0x08, 0},
	{0, 0x3e, 0x41, 0x41, 0x41, 0x3e, 0x41, 0x41, 0x41, 0x41, 0x3e, 0},
	{0, 0x3e, 0x41, 0x41, 0x41, 0x41, 0x3f, 0x01, 0x01, 0x01, 0x3e, 0},
	{0, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x08, 0x10, 0},
	{0, 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x02, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x3e, 0x00, 0x00, 0},
	{0, 0x00, 0x20, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x20, 0},
	{0, 0x00, 0x1c, 0x22, 0x02, 0x02, 0x04, 0x08, 0x08, 0x00, 0x08, 0},
		/* 0x40 */
	{0, 0x00, 0x3e, 0x41, 0x4d, 0x55, 0x55, 0x55, 0x4a, 0x40, 0x3e, 0},
	{0, 0x08, 0x08, 0x14, 0x14, 0x22, 0x3e, 0x22, 0x41, 0x41, 0x41, 0},
	{0, 0x7e, 0x21, 0x21, 0x21, 0x3e, 0x21, 0x21, 0x21, 0x21, 0x7e, 0},
	{0, 0x3e, 0x41, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x41, 0x3e, 0},
	{0, 0x7c, 0x22, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x22, 0x7c, 0},
	{0, 0x7f, 0x20, 0x20, 0x20, 0x3e, 0x20, 0x20, 0x20, 0x20, 0x7f, 0},
	{0, 0x7f, 0x20, 0x20, 0x20, 0x3e, 0x20, 0x20, 0x20, 0x20, 0x20, 0},
	{0, 0x3e, 0x41, 0x40, 0x40, 0x4f, 0x41, 0x41, 0x41, 0x43, 0x3d, 0},
	{0, 0x41, 0x41, 0x41, 0x41, 0x7f, 0x41, 0x41, 0x41, 0x41, 0x41, 0},
	{0, 0x1c, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1c, 0},
	{0, 0x07, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x42, 0x3c, 0},
	{0, 0x42, 0x44, 0x48, 0x50, 0x60, 0x50, 0x48, 0x44, 0x42, 0x41, 0},
	{0, 0x70, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x21, 0x7f, 0},
	{0, 0x41, 0x63, 0x55, 0x49, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0},
	{0, 0x41, 0x61, 0x51, 0x49, 0x45, 0x43, 0x41, 0x41, 0x41, 0x41, 0},
	{0, 0x3e, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x3e, 0},
		/* 0x50 */
	{0, 0x7e, 0x41, 0x41, 0x41, 0x7e, 0x40, 0x40, 0x40, 0x40, 0x40, 0},
	{0, 0x3e, 0x41, 0x41, 0x41, 0x41, 0x41, 0x49, 0x45, 0x43, 0x3e, 0},
	{0, 0x7e, 0x41, 0x41, 0x41, 0x41, 0x7e, 0x48, 0x44, 0x42, 0x41, 0},
	{0, 0x3e, 0x41, 0x40, 0x40, 0x38, 0x06, 0x01, 0x01, 0x41, 0x3e, 0},
	{0, 0x7f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0},
	{0, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x3e, 0},
	{0, 0x41, 0x41, 0x22, 0x22, 0x22, 0x14, 0x14, 0x14, 0x08, 0x08, 0},
	{0, 0x41, 0x41, 0x49, 0x49, 0x55, 0x55, 0x55, 0x22, 0x22, 0x22, 0},
	{0, 0x41, 0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41, 0x41, 0x41, 0},
	{0, 0x41, 0x41, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0},
	{0, 0x7f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x10, 0x20, 0x40, 0x7f, 0},
	{0, 0x1e, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1e, 0},
	{0, 0x00, 0x00, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0},
	{0, 0x3c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x3c, 0},
	{0, 0x08, 0x14, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0},
		/* 0x60 */
	{0, 0x10, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x21, 0x0f, 0x11, 0x21, 0x1f, 0},
	{0, 0x60, 0x20, 0x20, 0x20, 0x3e, 0x21, 0x21, 0x21, 0x21, 0x7e, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x21, 0x20, 0x20, 0x21, 0x1e, 0},
	{0, 0x03, 0x01, 0x01, 0x01, 0x1f, 0x21, 0x21, 0x21, 0x21, 0x1f, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x21, 0x3f, 0x20, 0x21, 0x1e, 0},
	{0, 0x06, 0x09, 0x08, 0x08, 0x1e, 0x08, 0x08, 0x08, 0x08, 0x08, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x21, 0x21, 0x1f, 0x01, 0x21, 0x1e},
	{0, 0x20, 0x20, 0x20, 0x20, 0x3e, 0x21, 0x21, 0x21, 0x21, 0x21, 0},
	{0, 0x00, 0x00, 0x04, 0x00, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x1e, 0},
	{0, 0x00, 0x00, 0x02, 0x00, 0x06, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c},
	{0, 0x20, 0x20, 0x20, 0x20, 0x22, 0x24, 0x28, 0x34, 0x22, 0x21, 0},
	{0, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x02, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x76, 0x49, 0x49, 0x49, 0x49, 0x49, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x21, 0x21, 0x21, 0x21, 0x21, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x21, 0x21, 0x21, 0x21, 0x1e, 0},
		/* 0x70 */
	{0, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x21, 0x21, 0x21, 0x3e, 0x20, 0x20},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x21, 0x21, 0x21, 0x1f, 0x01, 0x01},
	{0, 0x00, 0x00, 0x00, 0x00, 0x2e, 0x31, 0x20, 0x20, 0x20, 0x20, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x20, 0x18, 0x06, 0x01, 0x3e, 0},
	{0, 0x00, 0x00, 0x08, 0x08, 0x1e, 0x08, 0x08, 0x08, 0x08, 0x06, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x21, 0x21, 0x21, 0x21, 0x21, 0x1f, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x41, 0x22, 0x22, 0x14, 0x14, 0x08, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x49, 0x49, 0x55, 0x55, 0x36, 0x22, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x21, 0x12, 0x0c, 0x0c, 0x12, 0x21, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x21, 0x22, 0x14, 0x14, 0x08, 0x10, 0x20},
	{0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x02, 0x04, 0x08, 0x10, 0x3f, 0},
	{0, 0x06, 0x08, 0x08, 0x08, 0x10, 0x08, 0x08, 0x08, 0x08, 0x06, 0},
	{0, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0},
	{0, 0x30, 0x08, 0x08, 0x08, 0x04, 0x08, 0x08, 0x08, 0x08, 0x30, 0},
	{0, 0x19, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x1c, 0x1c, 0x00, 0x00, 0x00, 0},
};


static	void	update_lcd()
{
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


void	gdra_stp(W x, W y, UW color16, UW bgcolor16, W font, const UB *s)
{
	UW	color, bgcolor;
	UW	*p;
	W	c;
	
	if (s == NULL)
		return;
	if ((y -= 11) < 0)
		y = 0;
	if (y > LCD_H - 12)
		y = LCD_H - 12;
	if (x < 0)
		x = 0;
	
	color = color16to32(color16);
	bgcolor = color16to32(bgcolor16);
	
	while ((c = *(s++))) {
		int	x0, y0;
		
		if ((c -= 0x20) < 0)
			continue;
		if (c >= 0x60)
			continue;
		if (x + 8 > LCD_W)
			break;
		
		for (y0=0; y0<12; y0++) {
			unsigned char	v;
			
			v = font12n[c][y0];
			p = screen[y + y0] + x;
			for (x0=0; x0<8; x0++)
				p[x0] = (v & (0x80 >> x0))? color : bgcolor;
		}
		x += 8;
	}
	update_lcd();
}


W	gget_stw(W font, const UB *s)
{
	if (s == NULL)
		return 0;
	return strlen((char *)s) * 8;
}


UW	gettp()
{
	XEvent	ev0;
	
	for (;;) {
		if (!XCheckMaskEvent(xd0, ButtonPressMask, &ev0))
			return TPLIB_CMD_NULL;
		
		switch (ev0.type) {
			case	ButtonPress:
				return TPLIB_CMD_PRESS | (ev0.xbutton.x << 12) | ev0.xbutton.y;
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
	XSelectInput(xd0, xw0, ExposureMask|ButtonPressMask);
	XMapSubwindows(xd0, xw0);
	XMapWindow(xd0, xw0);
	update_lcd();
	usleep(500 * 1000);
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
			usleep(100 * 1000);
			continue;
		}
		x = y >> 12;
		y &= 0xfff;
		screen[y][x] = 0xffff;
		update_lcd();
	}
#else
	while (gettp() == 0)
		usleep(100 * 1000);
#endif
	
	return 0;
}
#endif

