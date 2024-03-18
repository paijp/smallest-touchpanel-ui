
/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2022 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

#include	"tplib.h"
#include	"i2c.h"

#define	I2CADDR_EEPROM	0x54


int	main(int ac, char **av)
{
	static	W	val0 = 0;
	static	struct	tplib_parts_struct	parts[] = {
		{tplib_parts_fill, 0, 0, LCD_W, LCD_H, 0x0000, NULL, NULL, NULL, 0}, 
		{tplib_parts_log, 0, 0, LCD_W, LCD_H, 0, (char*)__func__, NULL, TPLIB_CONST2STR(SHA1SUM), 0}, 
		{tplib_parts_dec, 16, 16, 112, 24, 0, &val0, NULL, "eeprom:", 0}, 
		{tplib_parts_button, TPLIB_REL + 32, TPLIB_REL, 64, 32, 1, NULL, NULL, "+1", 0}, 
		{NULL}
	};
	
	init_lcdtp();
	tplib_setupflip(__FILE__);
	for (;;) {
		i2cstop();		/* init */
		i2cstart();
		i2csend(I2CADDR_EEPROM << 1);
		i2csend(0);
		i2csend(0);
		i2cstop();
		i2cstart();
		i2csend((I2CADDR_EEPROM << 1) + 1);
		val0 = i2crecv(1);
		i2cstop();
		tplib_proc(NULL, TPLIB_CMD_REDRAWPART);
		
		for (;;) {
			switch (tplib_proc(parts, gettp())) {
				default:
					continue;
				case	1:
					i2cstop();		/* init */
					i2cstart();
					i2csend(I2CADDR_EEPROM << 1);
					i2csend(0);
					i2csend(0);
					i2csend(val0 + 1);
					i2cstop();
					dly_tsk(100);		/* write wait */
					break;
			}
			break;
		}
	}
	
	return 0;
}


