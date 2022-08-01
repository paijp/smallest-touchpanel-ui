
#include	"tplib.h"


W	tplib_parts_fill(struct tplib_parts_struct *p, UW cmd)
{
	switch (cmd & TPLIB_CMD_MASK) {
		default:
			return TPLIB_CONTINUE;
		case	TPLIB_CMD_REDRAW:
			break;
	}
	gfil_rec(p->left, p->top, p->left + p->width, p->top + p->height, p->par);
	return TPLIB_CONTINUE;
}


W	tplib_parts_text(struct tplib_parts_struct *p, UW cmd)
{
	W	ret;
	
	switch (cmd & TPLIB_CMD_MASK) {
		default:
			return TPLIB_CONTINUE;
		case	TPLIB_CMD_REDRAW:
			break;
	}
	
	if ((p->fn))
		if ((ret = p->fn(p, cmd)) != TPLIB_CONTINUE)
			return ret;
	
	gdra_stp(p->left, p->top + p->height, p->par, TPLIB_BGCOLOR, TPLIB_FONT12, (UB*)p->ppar);
	return TPLIB_CONTINUE;
}


W	tplib_parts_dec(struct tplib_parts_struct *p, UW cmd)
{
	static	UB	buf[2] = {0, 0};
	W	val, sign, x;
	
	switch (cmd & TPLIB_CMD_MASK) {
		default:
			return TPLIB_CONTINUE;
		case	TPLIB_CMD_REDRAW:
			break;
	}
	
	gdra_stp(p->left, p->top + p->height, TPLIB_CHARCOLOR, TPLIB_BGCOLOR, TPLIB_FONT12, (UB*)p->config);
	
	if ((p->fn))
		val = p->fn(p, cmd);
	else if ((p->ppar))
		val = *((W*)p->ppar);
	else
		val = p->par;
	
	sign = 0;
	if (val < 0) {
		sign = -1;
		val = -val;
	}
	
	x = p->left + p->width;
	do {
		buf[0] = '0' + (val % 10);
		val /= 10;
		gdra_stp(x -= 8, p->top + p->height, TPLIB_CHARCOLOR, TPLIB_BGCOLOR, TPLIB_FONT12, buf);
	} while ((val));
	if (sign < 0)
		gdra_stp(x -= 8, p->top + p->height, TPLIB_CHARCOLOR, TPLIB_BGCOLOR, TPLIB_FONT12, (UB*)"-");
	
	return TPLIB_CONTINUE;
}


W	tplib_parts_button(struct tplib_parts_struct *p, UW cmd)
{
	W	x;
	
	switch (cmd & TPLIB_CMD_MASK) {
		default:
			return TPLIB_CONTINUE;
		case	TPLIB_CMD_REDRAW:
			gfil_rec(p->left, p->top, p->left + p->width, p->top + p->height - 2, TPLIB_PARTSBGCOLOR);
			gfil_rec(p->left, p->top + p->height - 2, p->left + p->width, p->top + p->height, TPLIB_PARTSFGCOLOR);
			x = p->width - gget_stw(TPLIB_FONT12, (UB*)p->config);
			gdra_stp(p->left + x / 2, p->top + p->height - 4, TPLIB_PARTSCHARCOLOR, TPLIB_PARTSBGCOLOR, TPLIB_FONT12, (UB*)p->config);
			return TPLIB_CONTINUE;
		case	TPLIB_CMD_PRESS:
			break;
	}
	
	if ((p->fn))
		return p->fn(p, TPLIB_CMD_CHANGE);
	return p->par;
}


W	tplib_parts_buttonalt(struct tplib_parts_struct *p, UW cmd)
{
	W	x, v;
	
	if ((p->ppar))
		v = *((W*)p->ppar);
	else
		v = p->par;
	switch (cmd & TPLIB_CMD_MASK) {
		default:
			return TPLIB_CONTINUE;
		case	TPLIB_CMD_REDRAW:
			gfil_rec(p->left, p->top, p->left + p->width, p->top + p->height - 2, TPLIB_PARTSBGCOLOR);
			gfil_rec(p->left, p->top + p->height - 2, p->left + p->width, p->top + p->height, TPLIB_PARTSFGCOLOR);
			x = p->width - gget_stw(TPLIB_FONT12, (UB*)p->config);
			gdra_stp(p->left + x / 2, p->top + p->height - 4, TPLIB_PARTSCHARCOLOR, TPLIB_PARTSBGCOLOR, TPLIB_FONT12, (UB*)p->config);
			gfil_rec(p->left + 6, p->top + 3, p->left + p->width - 6, p->top + 7, (v)? TPLIB_PARTSONCOLOR : TPLIB_PARTSOFFCOLOR);
			return TPLIB_CONTINUE;
		case	TPLIB_CMD_PRESS:
			break;
	}
	
	v = (v == 0)? 1 : 0;
	if ((p->ppar))
		*((W*)p->ppar) = v;
	else
		p->par = v;
	gfil_rec(p->left + 6, p->top + 3, p->left + p->width - 6, p->top + 7, (v)? TPLIB_PARTSONCOLOR : TPLIB_PARTSOFFCOLOR);
	if ((p->fn))
		return p->fn(p, TPLIB_CMD_CHANGE);
	return p->par;
}


W	tplib_proc(struct tplib_parts_struct *list, UW cmd)
{
	static	W	redraw = 1;
	struct	tplib_parts_struct	*p;
	W	x, y;
	
	x = (cmd >> 12) & 0xfff;
	y = cmd & 0xfff;
	
	switch (cmd & TPLIB_CMD_MASK) {
		case	TPLIB_CMD_REDRAW:
			redraw = 1;
			return TPLIB_CONTINUE;
	}
	if (list == NULL)
		return TPLIB_CONTINUE;
	{
		W	l, t, w, h;
		
		l = t = w = h = 0;
		p = list;
		while ((p->parts)) {
			if ((p->left & TPLIB_REL))
				p->left = l + (p->left - TPLIB_REL);
			l = p->left;
			if ((p->top & TPLIB_REL))
				p->top = t + (p->top - TPLIB_REL);
			t = p->top;
			if ((p->width & TPLIB_REL))
				p->width = w + (p->width - TPLIB_REL);
			w = p->width;
			if ((p->height & TPLIB_REL))
				p->height = h + (p->height - TPLIB_REL);
			h = p->height;
			l += w;
			p++;
		}
	}
	if ((redraw)) {
		redraw = 0;
		p = list;
		while ((p->parts)) {
			p->parts(p, TPLIB_CMD_REDRAW);
			p++;
		}
	}
	switch (cmd & TPLIB_CMD_MASK) {
		case	TPLIB_CMD_PRESS:
			p = list;
			while ((p->parts)) {
				W	ret;
				
				if (x < p->left)
					;
				else if (x >= p->left + p->width)
					;
				else if (y < p->top)
					;
				else if (y >= p->top + p->height)
					;
				else if ((ret = p->parts(p, cmd)) != TPLIB_CONTINUE)
					return ret;
				p++;
			}
			break;
	}
	return TPLIB_CONTINUE;
}

