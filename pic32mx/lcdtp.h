
/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2022-2024 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

#include	"basic.h"


	/* lcdtp.c */

#define	LCD_W	240
#define	LCD_H	320

extern	UW	lcdtp_flip;
#define	LCDTP_FLIP_X	1
#define	LCDTP_FLIP_Y	2

extern	void	(*lcdtp_polltask)();

void	dly_tsk(W ms);
void	gfil_rec(W l, W t, W r, W b, UW color16);
void	gdra_stp(W x, W y, UW color16, UW bgcolor16, W font, const UB *s);
W	gget_stw(W font, const UB *s);
UW	gettp();
void	init_lcdtp();

#define	TPLIB_CMD_MASK	0xff000000
#define	TPLIB_CMD_NULL	0
#define	TPLIB_CMD_PRESS	0x10000000		/* 10xxxyyy */
#define	TPLIB_CMD_PRESSING	0x11000000		/* 10xxxyyy */

#define	LCDTP_FONT12	0


