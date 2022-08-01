
#include	"basic.h"


	/* lcdtp.c */

#define	LCD_W	240
#define	LCD_H	320

void	gfil_rec(W l, W t, W r, W b, UW color16);
void	gdra_stp(W x, W y, UW color16, UW bgcolor16, W font, const UB *s);
W	gget_stw(W font, const UB *s);
UW	gettp();
void	init_lcdtp();

#define	TPLIB_CMD_MASK	0xff000000
#define	TPLIB_CMD_NULL	0
#define	TPLIB_CMD_PRESS	0x10000000		/* 10xxxyyy */

#define	TPLIB_FONT12	0


