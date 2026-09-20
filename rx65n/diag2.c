/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	Read seven bytes over I2C and put them on the screen. Nothing else.

	diag1 stops a few touches in, somewhere between the transaction
	returning and the caller's next statement, and it stops without a loop
	anywhere near it. The trouble with hunting that inside diag1 is that
	diag1 does a great deal per pass - twenty rows of text, and, whenever
	the trace changes, seven hundred bytes pushed through the log a byte at
	a time. Those seven hundred bytes go out exactly when a touch arrives,
	which is exactly when it stops, and that correlation had gone unexamined
	while the transaction was being suspected.

	So this subtracts instead of adding. It calls i2c.h directly rather than
	going through envision_hw.c's touch_read(), keeps no ring buffer, logs
	nothing, and draws one line. What is left is the transaction and the
	return out of it - the stretch the stopping was localised to.

	If this stops too, it is the I2C layer or the path back out of it. If it
	does not, it is something diag1 does around them, and the instrumentation
	is the first thing to look at because it is the newest.
*/

#include	"lcdtp.h"
#include	"envision_hw.h"
#include	"i2c.h"


#define	ADDR	0x38		/* FT5x06 */


/*
	The transaction, written out here rather than called, so that the whole
	of what this program does to the bus is on one screen: address with the
	write bit, the register number, a repeated start, the address again with
	the read bit, seven bytes, stop. The last byte is NAKed so the device
	lets go of the line.
*/
static	W	read7(UB *buf)
{
	W	i;

	i2cstart();
	if (i2csend(ADDR << 1)) {
		i2cstop();
		return 0;
	}
	if (i2csend(2)) {
		i2cstop();
		return 0;
	}

	i2cstart();
	if (i2csend((ADDR << 1) | 1)) {
		i2cstop();
		return 0;
	}

	for (i = 0; i < 7; i++)
		buf[i] = (UB)i2crecv((i == 6)? 1 : 0);

	i2cstop();
	return 1;
}


static	void	hex2(UW v, UB *p)
{
	static	const	char	*bin2hex = "0123456789abcdef";

	p[0] = (UB)bin2hex[(v >> 4) & 0xf];
	p[1] = (UB)bin2hex[v & 0xf];
}


int	main(void)
{
	static	UB	line[32];
	static	UB	buf[7];
	static	UW	reads = 0, fails = 0;
	W	i;

	init_lcdtp();
	dly_tsk(500);
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	gdra_stp(8, 20, 0x07e0, 0x0000, NULL, (UB*)"rx65n i2c read, nothing else");

	/*
		Which pin is the clock is settled by asking the panel. i2c.h's
		state is per translation unit and envision_touch_init() settled
		its own copy, not this one, so ask again here.
	*/
	for (i = 0; i < 32; i++)
		line[i] = 0;
	hex2((UW)i2cprobe(ADDR), line);
	hex2((UW)i2c_swap, line + 3);
	line[2] = ' ';
	line[5] = 0;
	gdra_stp(8, 40, 0xffff, 0x0000, NULL, (UB*)"probe/swap");
	gdra_stp(120, 40, 0xffe0, 0x0000, NULL, line);

	for (;;) {
		if ((read7(buf)))
			reads++;
		else
			fails++;

		for (i = 0; i < 7; i++) {
			hex2(buf[i], line + i * 3);
			line[i * 3 + 2] = ' ';
		}
		line[20] = 0;
		gfil_rec(8, 46, 400, 74, 0x0000);
		gdra_stp(8, 60, 0xffe0, 0x0000, NULL, line);

		hex2(reads >> 8, line);
		hex2(reads, line + 2);
		line[4] = ' ';
		hex2(fails >> 8, line + 5);
		hex2(fails, line + 7);
		line[9] = 0;
		gfil_rec(8, 74, 400, 102, 0x0000);
		gdra_stp(8, 88, 0x07ff, 0x0000, NULL, (UB*)"ok/fail");
		gdra_stp(120, 88, 0x07ff, 0x0000, NULL, line);

		dly_tsk(60);
	}

	return 0;
}
