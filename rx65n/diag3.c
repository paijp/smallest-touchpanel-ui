/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	diag2 with the log put back, and nothing else put back.

	diag1 does a great deal per pass and stops a few touches in. diag2 took
	all of it away but the transaction. This adds one thing: a line in the
	ring buffer per read. If diag2 runs and this does not, the log is what
	does it, and the log is the code that was added to find this bug.

	Two differences from how diag1 logged, both so that the history is still
	there to read afterwards:

	Slower. 200ms rather than 60ms, which is a third of the traffic and
	still fast enough to catch a finger.

	Shorter. One line of about twenty bytes, not twenty lines of thirty-five
	- so the 4KB buffer holds around three minutes rather than six seconds.
	The reading has to happen with the target stopped, because RAM read off
	a running one comes back as a repeating pattern that is not what is in
	memory, so what matters is that the history survives until then.

	Each line starts with a count, so a gap says a read was missed and the
	last line says where it got to.
*/

#include	"lcdtp.h"
#include	"envision_hw.h"
#include	"debuglog.h"
#include	"i2c.h"


#define	ADDR	0x38		/* FT5x06 */


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


static	const	char	*bin2hex = "0123456789abcdef";

static	void	hex2(UW v, UB *p)
{
	p[0] = (UB)bin2hex[(v >> 4) & 0xf];
	p[1] = (UB)bin2hex[v & 0xf];
}


/* straight into the log, so the line costs what it says and no more */
static	void	loghex2(UW v)
{
	lcdtp_sendlogc(bin2hex[(v >> 4) & 0xf]);
	lcdtp_sendlogc(bin2hex[v & 0xf]);
}


int	main(void)
{
	static	UB	line[32];
	static	UB	buf[7];
	static	UW	seq = 0;
	W	i, ok;

	init_lcdtp();
	dly_tsk(500);
	gfil_rec(0, 0, LCD_W, LCD_H, 0x0000);
	gdra_stp(8, 20, 0x07e0, 0x0000, NULL, (UB*)"rx65n i2c read + log, 200ms");

	/* i2c.h's state is per translation unit; settle this copy's own */
	hex2((UW)i2cprobe(ADDR), line);
	line[2] = ' ';
	hex2((UW)i2c_swap, line + 3);
	line[5] = 0;
	gdra_stp(8, 40, 0xffff, 0x0000, NULL, (UB*)"probe/swap");
	gdra_stp(120, 40, 0xffe0, 0x0000, NULL, line);
	lcdtp_sendlogs("diag3 up\n");

	for (;;) {
		ok = read7(buf);
		seq++;

		/* seq, the seven bytes, and whether it answered */
		loghex2(seq >> 8);
		loghex2(seq);
		lcdtp_sendlogc(' ');
		for (i = 0; i < 7; i++)
			loghex2(buf[i]);
		lcdtp_sendlogc(' ');
		lcdtp_sendlogc((ok)? 'o' : 'x');
		lcdtp_sendlogc('\n');

		for (i = 0; i < 7; i++) {
			hex2(buf[i], line + i * 3);
			line[i * 3 + 2] = ' ';
		}
		line[20] = 0;
		gfil_rec(8, 46, 400, 74, 0x0000);
		gdra_stp(8, 60, 0xffe0, 0x0000, NULL, line);

		hex2(seq >> 8, line);
		hex2(seq, line + 2);
		line[4] = 0;
		gfil_rec(8, 74, 400, 102, 0x0000);
		gdra_stp(8, 88, 0x07ff, 0x0000, NULL, (UB*)"seq");
		gdra_stp(120, 88, 0x07ff, 0x0000, NULL, line);

		dly_tsk(200);
	}

	return 0;
}
