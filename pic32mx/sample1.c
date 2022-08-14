
/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2022 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

#include	"tplib.h"


static	W	proc_tenkey(struct tplib_parts_struct *p, UW cmd)
{
	static	W	val;
	static	struct	tplib_parts_struct	parts[] = {
		{tplib_parts_fill, 0, 0, LCD_W, LCD_H, 0x0000, NULL, NULL, NULL, 0}, 
		{tplib_parts_dec, 16, 16, 112, 24, 0, &val, NULL, NULL, 0}, 
		{tplib_parts_button, TPLIB_REL + 32, TPLIB_REL, 64, 32, -1, NULL, NULL, "<-", 0}, 
		{tplib_parts_button, 16, TPLIB_REL + 48, 64, 48, 7, NULL, NULL, "7", 0}, 
		{tplib_parts_button, TPLIB_REL + 8, TPLIB_REL, 64, 48, 8, NULL, NULL, "8", 0}, 
		{tplib_parts_button, TPLIB_REL + 8, TPLIB_REL, 64, 48, 9, NULL, NULL, "9", 0}, 
		{tplib_parts_button, 16, TPLIB_REL + 64, 64, 48, 4, NULL, NULL, "4", 0}, 
		{tplib_parts_button, TPLIB_REL + 8, TPLIB_REL, 64, 48, 5, NULL, NULL, "5", 0}, 
		{tplib_parts_button, TPLIB_REL + 8, TPLIB_REL, 64, 48, 6, NULL, NULL, "6", 0}, 
		{tplib_parts_button, 16, TPLIB_REL + 64, 64, 48, 1, NULL, NULL, "1", 0}, 
		{tplib_parts_button, TPLIB_REL + 8, TPLIB_REL, 64, 48, 2, NULL, NULL, "2", 0}, 
		{tplib_parts_button, TPLIB_REL + 8, TPLIB_REL, 64, 48, 3, NULL, NULL, "3", 0}, 
		{tplib_parts_button, 16, TPLIB_REL + 64, 64, 48, 0, NULL, NULL, "0", 0}, 
		{tplib_parts_button, TPLIB_REL + 8, TPLIB_REL + 16, 64, 32, -2, NULL, NULL, "cancel", 0}, 
		{tplib_parts_button, TPLIB_REL + 8, TPLIB_REL, 64, 32, -3, NULL, NULL, "OK", 0}, 
		{NULL}
	};
	
	if (p == NULL)
		val = 0;
	else if ((p->ppar))
		val = *((W*)p->ppar);
	else
		val = p->par;
	
	tplib_proc(NULL, TPLIB_CMD_REDRAW);		/* draw myself */
	
	for (;;) {
		W	ret;
		
		switch (ret = tplib_proc(parts, gettp())) {
			case	TPLIB_CONTINUE:
				continue;
			default:
				if (val > 99999999)
					continue;
				val = val * 10 + ret;
				tplib_proc(NULL, TPLIB_CMD_REDRAWPART);		/* draw myself */
				continue;
			case	-1:		/* <- */
				val /= 10;
				tplib_proc(NULL, TPLIB_CMD_REDRAWPART);		/* draw myself */
				continue;
			case	-2:		/* cancel */
				tplib_proc(NULL, TPLIB_CMD_REDRAW);		/* draw parent screen */
				return TPLIB_CONTINUE;
			case	-3:		/* ok */
				tplib_proc(NULL, TPLIB_CMD_REDRAW);		/* draw parent screen */
				if (p == NULL)
					return val;
				if ((p->ppar))
					*((W*)p->ppar) = val;
				else
					p->par = val;
				return p->par;
		}
	}
}


int	main(int ac, char **av)
{
	static	W	val0 = 0;
	static	W	val1 = 0;
	static	W	group = 0;
	static	W	groupa = 0;
	static	struct	tplib_parts_struct	parts[] = {
		{tplib_parts_fill, 0, 0, LCD_W, LCD_H, 0x0000, NULL, NULL, NULL, 0}, 
		{tplib_parts_dec, 16, 16, 112, 24, 0, &val0, NULL, "val0:", 0}, 
		{tplib_parts_button, TPLIB_REL + 32, TPLIB_REL, 64, 32, 0, &val0, proc_tenkey, "set", 0}, 
		{tplib_parts_dec, 16, TPLIB_REL + 48, 112, 24, 0, &val1, NULL, "val1:", 0}, 
		{tplib_parts_button, TPLIB_REL + 32, TPLIB_REL, 64, 32, 0, &val1, proc_tenkey, "set", 0}, 
		{tplib_parts_buttonalt, 16, TPLIB_REL + 48, 64, 32, 0, NULL, NULL, "alt0", 0}, 
		{tplib_parts_buttonalt, TPLIB_REL + 80, TPLIB_REL, 64, 32, 1, NULL, NULL, "alt1", 0}, 
		{tplib_parts_buttongroup, 16, TPLIB_REL + 48, 64, 32, 0, &group, NULL, "group0", 0}, 
		{tplib_parts_buttongroup, TPLIB_REL + 8, TPLIB_REL, 64, 32, 1, &group, NULL, "group1", 0}, 
		{tplib_parts_buttongroup, TPLIB_REL + 8, TPLIB_REL, 64, 32, 2, &group, NULL, "group2", 0}, 
		{tplib_parts_buttongroup, 16, TPLIB_REL + 48, 64, 32, 0, &groupa, tplib_alwaysselect, "group0a", 0}, 
		{tplib_parts_buttongroup, TPLIB_REL + 8, TPLIB_REL, 64, 32, 1, &groupa, tplib_alwaysselect, "group1a", 0}, 
		{tplib_parts_buttongroup, TPLIB_REL + 8, TPLIB_REL, 64, 32, 2, &groupa, tplib_alwaysselect, "group2a", 0}, 
		{NULL}
	};
	
	init_lcdtp();
	tplib_setupflip("smallest-touchpanel-ui");
	for (;;) {
		tplib_proc(parts, gettp());
	}
	
	return 0;
}


