
/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2022-2023 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

#include	"tplib.h"

/*
	The rows step down by TPLIB_REL + 40 rather than the 48 this sample was
	written with. It was laid out for a 240x320 panel, where the last row -
	the volume slider, 48 high, at y 256 - ended at 304 and fitted. The
	Envision Kit's panel is 480x272, so that bottom row fell off the screen.

	Only the vertical step changed, and evenly: the rows now sit at 16, 56,
	96, 136, 176 and 216, and the slider ends at 264, eight pixels clear of
	the bottom. Widths, heights and every x are as they were, so the sample
	still shows what it was written to show.
*/


int	main(int ac, char **av)
{
	static	W	val0 = 0;
	static	W	val1 = 0;
	static	W	group = 0;
	static	W	groupa = 0;
	static	W	volume0 = 0;
	static	struct	tplib_parts_struct	parts[] = {
		{tplib_parts_fill, 0, 0, LCD_W, LCD_H, 0x0000, NULL, NULL, NULL, 0}, 
		{tplib_parts_log, 0, 0, LCD_W, LCD_H, 0, (char*)__func__, NULL, TPLIB_CONST2STR(SHA1SUM), 0}, 
		{tplib_parts_dec, 16, 16, 112, 24, 0, &val0, NULL, "val0:", 0}, 
		{tplib_parts_button, TPLIB_REL + 32, TPLIB_REL, 64, 32, 0, &val0, tplib_proc_tenkey, "set", 0}, 
		{tplib_parts_dec, 16, TPLIB_REL + 40, 112, 24, 0, &val1, NULL, "val1:", 0}, 
		{tplib_parts_button, TPLIB_REL + 32, TPLIB_REL, 64, 32, 0, &val1, tplib_proc_tenkey, "set", 0}, 
		{tplib_parts_buttonalt, 16, TPLIB_REL + 40, 64, 32, 0, NULL, NULL, "alt0", 0}, 
		{tplib_parts_buttonalt, TPLIB_REL + 80, TPLIB_REL, 64, 32, 1, NULL, NULL, "alt1", 0}, 
		{tplib_parts_buttongroup, 16, TPLIB_REL + 40, 64, 32, 0, &group, NULL, "group0", 0}, 
		{tplib_parts_buttongroup, TPLIB_REL + 8, TPLIB_REL, 64, 32, 1, &group, NULL, "group1", 0}, 
		{tplib_parts_buttongroup, TPLIB_REL + 8, TPLIB_REL, 64, 32, 2, &group, NULL, "group2", 0}, 
		{tplib_parts_buttongroup, 16, TPLIB_REL + 40, 64, 32, 0, &groupa, tplib_alwaysselect, "group0a", 0}, 
		{tplib_parts_buttongroup, TPLIB_REL + 8, TPLIB_REL, 64, 32, 1, &groupa, tplib_alwaysselect, "group1a", 0}, 
		{tplib_parts_buttongroup, TPLIB_REL + 8, TPLIB_REL, 64, 32, 2, &groupa, tplib_alwaysselect, "group2a", 0}, 
		{tplib_parts_dec, 16, TPLIB_REL + 40, 128, 24, 0, &volume0, NULL, "volume0:", 0}, 
		{tplib_parts_sliderv, TPLIB_REL + 16, TPLIB_REL, 32, 48, 0, &volume0, tplib_proc_redraw, NULL, 0}, 
		{NULL}
	};
	
	init_lcdtp();
	tplib_setupflip("smallest-touchpanel-ui");
	for (;;) {
		tplib_proc(parts, gettp());
	}
	
	return 0;
}


