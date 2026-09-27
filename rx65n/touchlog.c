/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2026 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

/*
	Touch panel response, on the LCD and on the serial log at the same time.

	Both halves say the same thing on purpose. The screen is what you watch
	while a finger is on the panel, because looking away to a terminal is
	exactly when the interesting reading goes past. The log is what survives:
	it has every event with a timestamp, so a coordinate that came out wrong,
	a press the panel split into two, or a gap where nothing was reported can
	be read back afterwards instead of remembered.

	It answers one question: does the panel report what you touched, and does
	it keep doing so? So it draws where the last contact was, keeps a short
	history of recent events, and counts what has happened since reset.

	On screen:

	  - a crosshair and a filled square at the last reported point
	  - the live coordinate, the event type and how long the contact has
	    lasted
	  - counters: presses, pressing reports, releases, I2C failures
	  - the last few events as a list, newest first

	On the log, one line per event:

	      12.484 press     x=241 y=137  n=17
	      12.516 pressing  x=242 y=139  n=18  held 32ms
	      12.702 release                n=18  held 218ms dx=8 dy=11

	Written to be read with open-rfp-monitor, which reads the board's own
	serial port through the E2 Lite that flashed it:

	    rxflash.py write touchlog.mot --log

	Build it instead of sample1.c.
*/

#include	"lcdtp.h"
#include	"envision_hw.h"
#include	"debuglog.h"
#include	"seriallog.h"


/* Layout. The readout sits on the left, the history under it. */
#define	PAD		6
#define	LINE_H		14
#define	COL_X		PAD
#define	HIST_N		8

#define	C_BG		0x0000		/* black */
#define	C_TEXT		0xffff		/* white */
#define	C_DIM		0x8410		/* grey */
#define	C_MARK		0xf800		/* red: the touch point */
#define	C_PRESS		0x07e0		/* green: a new contact */
#define	C_HOLD		0xffe0		/* yellow: the same contact, still down */
#define	C_FAIL		0xf81f		/* magenta: the bus did not answer */

#define	MARK_R		10		/* half-width of the square at the point */
#define	CROSS_R		26		/* arm length of the crosshair */


/*
	gettp() packs the type and the coordinate into one word: type in the top
	byte, x in bits 12..23, y in the low 12.
*/
#define	TP_X(v)		(((v) >> 12) & 0xfff)
#define	TP_Y(v)		((v) & 0xfff)


static	struct	event_struct {
	UW	ms;			/* when, since reset */
	W	type;			/* TPLIB_CMD_PRESS, PRESSING, or -1 for a failure */
	W	x, y;
	UW	held;			/* ms since the press that started this contact */
} history[HIST_N];

static	W	history_n = 0;		/* events ever recorded, so the list can say "…" */

static	UW	now_ms = 0;		/* the poll loop's clock, one tick per pass */

static	UW	count_press = 0;
static	UW	count_pressing = 0;
static	UW	count_release = 0;
static	UW	count_fail = 0;


/* --- small formatting helpers, in the port's style --- */

static	UB	*put_str(UB *p, const char *s)
{
	while ((*s))
		*p++ = (UB)*s++;
	*p = 0;
	return p;
}


static	UB	*put_dec(UB *p, W v, W width, W pad_zero)
{
	UB	tmp[12];
	W	n = 0, neg = 0;

	if (v < 0) {
		neg = 1;
		v = -v;
	}
	do {
		tmp[n++] = (UB)('0' + (v % 10));
		v /= 10;
	} while ((v) && n < (W)sizeof(tmp));
	if ((neg))
		tmp[n++] = '-';
	while (n < width)
		tmp[n++] = (UB)((pad_zero)? '0' : ' ');
	while (n > 0)
		*p++ = tmp[--n];
	*p = 0;
	return p;
}


/*
	Seconds with three decimals, from a millisecond count. The log is easier
	to read against a stopwatch this way than as a bare tick count, and the
	fractional part is where a split press shows up.
*/
static	UB	*put_time(UB *p, UW ms)
{
	p = put_dec(p, (W)(ms / 1000), 3, 0);
	*p++ = '.';
	p = put_dec(p, (W)(ms % 1000), 3, 1);
	*p = 0;
	return p;
}


/* --- the log line --- */

static	void	log_line(const struct event_struct *e, W dx, W dy)
{
	UB	buf[24];

	put_time(buf, e->ms);
	lcdtp_sendlogs((const char*)buf);

	if (e->type == TPLIB_CMD_PRESS)
		lcdtp_sendlogs(" press    ");
	else if (e->type == TPLIB_CMD_PRESSING)
		lcdtp_sendlogs(" pressing ");
	else if (e->type == -1)
		lcdtp_sendlogs(" i2c fail ");
	else
		lcdtp_sendlogs(" release  ");

	if (e->type == TPLIB_CMD_PRESS || e->type == TPLIB_CMD_PRESSING) {
		lcdtp_sendlogs(" x=");
		lcdtp_sendlogdec(e->x);
		lcdtp_sendlogs(" y=");
		lcdtp_sendlogdec(e->y);
	} else {
		lcdtp_sendlogs("           ");
	}

	lcdtp_sendlogs("  n=");
	lcdtp_sendlogdec(history_n);

	if (e->type != TPLIB_CMD_PRESS && e->type != -1) {
		lcdtp_sendlogs(" held ");
		lcdtp_sendlogdec((W)e->held);
		lcdtp_sendlogs("ms");
	}
	if (dx >= 0) {
		lcdtp_sendlogs(" dx=");
		lcdtp_sendlogdec(dx);
		lcdtp_sendlogs(" dy=");
		lcdtp_sendlogdec(dy);
	}

	lcdtp_sendlogs("\r\n");
}


static	void	record(W type, W x, W y, UW held, W dx, W dy)
{
	struct	event_struct	*e;
	W	i;

	for (i = HIST_N - 1; i > 0; i--)
		history[i] = history[i - 1];
	e = &history[0];
	e->ms = now_ms;
	e->type = type;
	e->x = x;
	e->y = y;
	e->held = held;
	history_n++;

	log_line(e, dx, dy);
}


/* --- the screen --- */

static	void	draw_label(W row, const char *label, const UB *value, UW color)
{
	UB	buf[48];
	UB	*p;

	p = put_str(buf, label);
	put_str(p, (const char*)value);
	/*
		Clear to a fixed width first: a shorter value would otherwise leave
		the tail of a longer one behind, which reads as a stuck coordinate.
	*/
	gfil_rec(COL_X, PAD + row * LINE_H, COL_X + 200,
		 PAD + row * LINE_H + LINE_H - 1, C_BG);
	gdra_stp(COL_X, PAD + row * LINE_H, color, C_BG, LCDTP_FONT12, buf);
}


static	void	draw_mark(W x, W y, UW color)
{
	W	l, t, r, b;

	/* Crosshair first, so the square sits on top of its centre. */
	l = x - CROSS_R;
	r = x + CROSS_R;
	if (l < 0)
		l = 0;
	if (r >= LCD_W)
		r = LCD_W - 1;
	gfil_rec(l, y, r, y, color);

	t = y - CROSS_R;
	b = y + CROSS_R;
	if (t < 0)
		t = 0;
	if (b >= LCD_H)
		b = LCD_H - 1;
	gfil_rec(x, t, x, b, color);

	l = x - MARK_R;
	t = y - MARK_R;
	r = x + MARK_R;
	b = y + MARK_R;
	if (l < 0)
		l = 0;
	if (t < 0)
		t = 0;
	if (r >= LCD_W)
		r = LCD_W - 1;
	if (b >= LCD_H)
		b = LCD_H - 1;
	gfil_rec(l, t, r, b, color);
}


static	void	draw_history(void)
{
	UB	buf[48];
	UB	*p;
	W	i, row;

	row = 7;
	for (i = 0; i < HIST_N; i++) {
		const struct event_struct *e = &history[i];

		gfil_rec(COL_X, PAD + (row + i) * LINE_H, COL_X + 230,
			 PAD + (row + i) * LINE_H + LINE_H - 1, C_BG);
		if (i >= history_n)
			continue;

		p = put_time(buf, e->ms);
		p = put_str(p, " ");
		if (e->type == TPLIB_CMD_PRESS)
			p = put_str(p, "press   ");
		else if (e->type == TPLIB_CMD_PRESSING)
			p = put_str(p, "pressing");
		else if (e->type == -1)
			p = put_str(p, "i2c fail");
		else
			p = put_str(p, "release ");
		if (e->type == TPLIB_CMD_PRESS || e->type == TPLIB_CMD_PRESSING) {
			p = put_str(p, " ");
			p = put_dec(p, e->x, 3, 0);
			p = put_str(p, ",");
			p = put_dec(p, e->y, 3, 0);
		}
		gdra_stp(COL_X, PAD + (row + i) * LINE_H,
			 (i == 0)? C_TEXT : C_DIM, C_BG, LCDTP_FONT12, buf);
	}
}


static	void	draw_counters(void)
{
	UB	buf[48];
	UB	*p;

	p = put_dec(buf, (W)count_press, 0, 0);
	p = put_str(p, " press  ");
	p = put_dec(p, (W)count_pressing, 0, 0);
	p = put_str(p, " hold");
	draw_label(3, "", buf, C_TEXT);

	p = put_dec(buf, (W)count_release, 0, 0);
	p = put_str(p, " release  ");
	p = put_dec(p, (W)count_fail, 0, 0);
	p = put_str(p, " i2c fail");
	draw_label(4, "", buf, (count_fail)? C_FAIL : C_DIM);
}


int	main(void)
{
	W	tp, type, x, y;
	W	last_x = -1, last_y = -1;
	W	press_x = 0, press_y = 0;
	UW	press_ms = 0;
	W	down = 0;
	UB	buf[48];
	UB	*p;

	envision_clock_init();
	seriallog_init();		/* after the clock: BRR comes from PCLKB */
	envision_lcd_init();
	envision_touch_init();
	init_lcdtp();

	lcdtp_sendlogs("\r\n--- touchlog: touch the panel ---\r\n");
	lcdtp_sendlogs("time    event     coordinate    counters\r\n");

	gfil_rec(0, 0, LCD_W - 1, LCD_H - 1, C_BG);
	gdra_stp(COL_X, PAD, C_TEXT, C_BG, LCDTP_FONT12,
		 (const UB*)"touchlog - touch the panel");
	draw_label(1, "waiting for a first contact", (const UB*)"", C_DIM);
	draw_counters();
	draw_history();

	for (;;) {
		tp = (W)gettp();
		type = tp & (W)TPLIB_CMD_MASK;

		if (type == TPLIB_CMD_PRESS || type == TPLIB_CMD_PRESSING) {
			x = TP_X(tp);
			y = TP_Y(tp);

			if (type == TPLIB_CMD_PRESS) {
				count_press++;
				press_x = x;
				press_y = y;
				press_ms = now_ms;
				down = 1;
				/*
					Clear the previous contact's marks before drawing
					this one, so a new press does not leave the old
					crosshair on screen looking like a second finger.
				*/
				gfil_rec(0, 0, LCD_W - 1, LCD_H - 1, C_BG);
				gdra_stp(COL_X, PAD, C_TEXT, C_BG, LCDTP_FONT12,
					 (const UB*)"touchlog - touch the panel");
				record(TPLIB_CMD_PRESS, x, y, 0, -1, -1);
			} else {
				count_pressing++;
				/*
					Every reading while a finger stays down would be
					hundreds of near-identical lines. Record one only
					when the point actually moved, which is what a drag
					looks like and what a jittering panel looks like too.
				*/
				if (x != last_x || y != last_y)
					record(TPLIB_CMD_PRESSING, x, y,
					       now_ms - press_ms, -1, -1);
			}

			if (x != last_x || y != last_y) {
				if (last_x >= 0)
					draw_mark(last_x, last_y, C_BG);
				draw_mark(x, y, (type == TPLIB_CMD_PRESS)? C_PRESS : C_MARK);
				last_x = x;
				last_y = y;
			}

			p = put_str(buf, "x=");
			p = put_dec(p, x, 3, 0);
			p = put_str(p, " y=");
			p = put_dec(p, y, 3, 0);
			p = put_str(p, (type == TPLIB_CMD_PRESS)? "  press" : "  held ");
			p = put_dec(p, (W)(now_ms - press_ms), 0, 0);
			put_str(p, "ms");
			draw_label(1, "", buf, (type == TPLIB_CMD_PRESS)? C_PRESS : C_HOLD);

		} else {
			/*
				No contact. envision_touch_get() reports a bus failure the
				same way, so a run of these with nothing touching the panel
				is normal and a rising i2c count is not - the trace in
				envision_i2c_trace says which.
			*/
			if ((down)) {
				W	dx, dy;

				count_release++;
				down = 0;
				dx = last_x - press_x;
				dy = last_y - press_y;
				if (dx < 0)
					dx = -dx;
				if (dy < 0)
					dy = -dy;
				record(0, 0, 0, now_ms - press_ms, dx, dy);

				if (last_x >= 0)
					draw_mark(last_x, last_y, C_DIM);
				draw_label(1, "released, last point held ",
					   (const UB*)"", C_DIM);
				p = put_dec(buf, (W)(now_ms - press_ms), 0, 0);
				put_str(p, "ms");
				draw_label(2, "  for ", buf, C_DIM);
			}
			if (envision_i2c_trace[4] != 0)
				count_fail = envision_i2c_trace[4];
		}

		draw_counters();
		draw_history();

		/*
			A fixed step is what makes the times comparable between events.
			gettp() itself takes a variable few milliseconds over I2C, so
			this clock drifts against real time; it is a consistent ruler,
			not a stopwatch.
		*/
		dly_tsk(16);
		now_ms += 16;
	}
}
