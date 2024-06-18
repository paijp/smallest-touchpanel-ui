
#include	"tplib.h"
#include	"i2c.h"

#define	I2CADDR_DA0	0x60
#define	I2CADDR_AD0	0x68
#define	I2CADDR_AD1	0x69


/*jp.pa-i.cir/pcbgrid20
** 10,2qr

** 4,13t10,0,0
B8
A0
A1
B0
B1
(B2)
15
13
SC
B9

** 3,13t10,0,0
(DA)
G	2262<0>>22222222
G
5V
5V
3V3	63222222<0>>22222222
A0	12222222<0>>22222222
A1	112222222<0>>22222222
SD
BT


*/


int	main(int ac, char **av)
{
	
	init_lcdtp();
	tplib_setupflip(__FILE__);
	
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	
	i2cstop();
	
	i2cstart();
	i2csend(I2CADDR_AD0 * 2);
	i2csend(0x9f);		/* continuos ch=0 3.75sps gain=8 */
	i2cstop();
	
	for (;;) {
		W	h0, m0, l0;
		W	v, sts;
		W	i, x, y;
		UB	*sign;
		
		i2cstop();
		
		do {
			i2cstart();
			i2csend(I2CADDR_AD0 * 2 + 1);
			h0 = i2crecv(0);
			m0 = i2crecv(0);
			l0 = i2crecv(0);
			sts = i2crecv(1);
			i2cstop();
		} while ((sts & 0x80));
		
		v = (h0 << 16) | (m0 << 8) | l0;
		if ((v & 0x00800000))
			v -= 0x01000000;
		
		x = 192;
		y = 32;
		sign = " ";
		if (v < 0) {
			v = -v;
			sign = "-";
		}
		i = 0;
		do {
			static	const	UB*	dec2s[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
			
			gdra_stp(x -= 8, y, 0xffff, 0x0000, NULL, dec2s[v % 10]);
			v /= 10;
			i++;
		} while (v > 0);
		gdra_stp(x -= 8, y, 0xffff, 0x0000, NULL, sign);
		while (i++ < 10)
			gdra_stp(x -= 8, y, 0xffff, 0x0000, NULL, " ");
	}
	
	return 0;
}


