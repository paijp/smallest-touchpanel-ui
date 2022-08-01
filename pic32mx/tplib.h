
#include	"lcdtp.h"


	/* tplib.c */

struct	tplib_parts_struct {
	W	(*parts)(struct tplib_parts_struct *p, UW cmd);
	W	left, top;
	W	width, height;
	W	par;
	void	*ppar;
	W	(*fn)(struct tplib_parts_struct *p, UW cmd);
	void	*config;
	UW	flag;
};
#define	TPLIB_REL	0x80000000
#define	TPLIB_CONTINUE	0x80000000

#define	TPLIB_CMD_CHANGE	0x20000000
#define	TPLIB_CMD_REDRAW	0x40000000
#define	TPLIB_CMD_REDRAWPART	0x41000000

W	tplib_parts_fill(struct tplib_parts_struct *p, UW cmd);
W	tplib_parts_text(struct tplib_parts_struct *p, UW cmd);
W	tplib_parts_dec(struct tplib_parts_struct *p, UW cmd);
W	tplib_parts_button(struct tplib_parts_struct *p, UW cmd);
W	tplib_parts_buttonalt(struct tplib_parts_struct *p, UW cmd);
W	tplib_proc(struct tplib_parts_struct *p, UW cmd);

#define	TPLIB_BGCOLOR	0x0000
#define	TPLIB_CHARCOLOR	0xffff
#define	TPLIB_PARTSBGCOLOR	0x8000
#define	TPLIB_PARTSFGCOLOR	0xf800
#define	TPLIB_PARTSCHARCOLOR	0xffff
#define	TPLIB_PARTSONCOLOR	0x07e0
#define	TPLIB_PARTSOFFCOLOR	0x0000


