
/*
	Smallest touchpanel UI https://github.com/paijp/smallest-touchpanel-ui

	Copyright (c) 2022-2023 paijp

	This software is released under the Apache 2.0 license.
	http://www.apache.org/licenses/
*/

#include	"xc.h"
#include	"lcdtp.h"

#pragma config  PMDL1WAY=OFF, IOL1WAY=OFF, FUSBIDIO=OFF, FVBUSONIO=OFF
#pragma config  FPLLIDIV=DIV_1, FPLLMUL=MUL_20, UPLLIDIV=DIV_1, UPLLEN=OFF
#pragma	config	FPLLODIV=DIV_2, FNOSC=FRCPLL, FSOSCEN=OFF, IESO=OFF
#pragma	config	POSCMOD=XT, OSCIOFNC=OFF, FPBDIV=DIV_1, FCKSM=CSECMD
#pragma	config	WDTPS=PS16384, WINDIS=OFF, FWDTEN=OFF, FWDTWINSZ=WINSZ_50
#pragma	config	DEBUG=OFF, JTAGEN=OFF, ICESEL=ICS_PGx2, PWP=OFF
#pragma	config	BWP=OFF, CP=OFF

/*map32mx
LSS	RPB3
LRS_T2P	RPB4
P9__SCL	RPA4
P2L_P2T	RPB5	SDO1

P19__TSS_SDA	RPB7
P7__P2B1	RPB15	UTX1
P8__B2P1	RPB13	URX1
P1__B2P2	RPB8	URX2
P10__P2B2	RPB9	UTX2

LCK_TCK	RPB14	SCK1

PGD	RPB10	PGED2	UTX2
PGC	PGEC2
fix	CLKI	CLKO	VCAP
VBUS	RB6
VUSB3V3	RB12

P2	RPA0
P3	RPA1
P4	RPB0
P5	RPB1
P6	RPB2
*/

/*cir-dip28 PIC32MX270F256B
22(442{r10k}2<<3V3>)2<MCLR>2{c104}2<<G>
22<P2>
22<P3>
22<P4>
22<P5>
22<P6>
22<LSS>2{d}2(2<RST_LED>)442{d}2<RYLED>
22<<G>
22<OSC1>
22<OSC2>
22<LRS_T2P>
22<P9_SCL>
22<<3V3>2{c106}2<<G>
22<P2L_P2T>
8
88<P19__TSS_SDA>8(448{d}8<RST_LED>)8{r10k}8<<3V3>
88<P1__B2P2>
88<P10__P2B2>
888<<G>
88{c106}8<<G>
88<PGD>
88<PGC>
88<<3V3>
88<B2P>8<P8__B2P1>
88<LCK_TCK>
88<P2B>8<P7__P2B1>
88<<G>
88<<3V3>8{c104}8<<G>
*/

/*cir-sip14 MSP2807
22<<3V3>
22<<G>
22<LSS>
22<RST_LED>
22<LRS_T2P>
22<P2L_P2T>
22<LCK_TCK>
22<RST_LED>
2
22<LCK_TCK>
22<P19__TSS_SDA>
22<P2L_P2T>
22<LRS_T2P>
2
*/

/*cir-sip5 ICSP
22<MCLR>
22<<3V3>
22<<G>
22<PGD>
22<PGC>
*/

/*cir-dip20 CN2x10
22<P1__B2P2>
22<P2>
22<P3>
22<P4>
22<P5>
22<P6>
22<P7__P2B1>
22<P8__B2P1>
22<P9_SCL>
22<P10__P2B2>
88<(BAT)>
88<P19__TSS_SDA>
88<AD2>
88<AD1>
88<<3V3>
88<(5V)>
88<(5V)>
88<<G>
88<<G>
88<DA1>
*/

/*cir-dip14 MCP3424	# i2c:0x69
2
2
2
2
2
22<<G>
22<P9_SCL>
88<P19__TSS_SDA>
88<<3V3>8{c104}8<<G>
88<<G>
88<<G>
88<AD1>
88<<G>
88<AD2>
*/

/*cir-dip14 MCP3424	# i2c:0x68
2
2
2
2
2
2
22<P9_SCL>
88<P19__TSS_SDA>
88<<3V3>8{c104}8<<G>
88<<G>
88<<G>
88<AD2>
88<AD2>
88<AD1>
*/

/*cir-dip6 MCP4725	# i2c:0x60
22<DA1>
22<<G>
22<<3V3>2{c104}2<<G>
88<P19__TSS_SDA>
88<P9_SCL>
88<<G>
*/


#define	PORT_A0	PORTAbits.RA0
#define	PORT_A1	PORTAbits.RA1

#define	PORT_A3	PORTBbits.RB0
#define	PORT_A4	PORTBbits.RB1
#define	PORT_A5	PORTBbits.RB2

#define	LAT_LSS	LATBbits.LATB3
#define	LAT_LRS	LATBbits.LATB4
#define	PORT_T2P	PORTBbits.RB4
#define	TRIS_LRS_T2P	TRISBbits.TRISB4
#define	PORT_A8_SCL	PORTAbits.RA4
#define	LAT_P2L_P2T	LATBbits.LATB5

#define	LAT_TSS_SDA	LATBbits.LATB7
#define	PORT_SDA	PORTBbits.RB7
#define	PORT_A13_B2P	PORTBbits.RB8
#define	PORT_A9_P2B	PORTBbits.RB9

#define	PORT_A7	PORTBbits.RB13
#define	LAT_LCK_TCK	LATBbits.LATB14
#define	PORT_A6	PORTBbits.RB15


UW	lcdtp_flip = 0;
void	(*lcdtp_polltask)() = NULL;

static	const	UB	font12n[0x60][12] = {
		/* 0x20 */
	{0},
	{0, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0},
	{0, 0x36, 0x12, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x00, 0x12, 0x12, 0x7f, 0x22, 0x22, 0x7f, 0x24, 0x24, 0x24, 0},
	{0, 0x00, 0x08, 0x3f, 0x48, 0x48, 0x3e, 0x09, 0x09, 0x7e, 0x08, 0},
	{0, 0x00, 0x70, 0x51, 0x72, 0x04, 0x08, 0x10, 0x27, 0x45, 0x07, 0},
	{0, 0x00, 0x38, 0x44, 0x44, 0x28, 0x10, 0x28, 0x45, 0x42, 0x3d, 0},
	{0, 0x18, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x04, 0x08, 0x08, 0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x04, 0},
	{0, 0x10, 0x08, 0x08, 0x04, 0x04, 0x04, 0x04, 0x08, 0x08, 0x10, 0},
	{0, 0x00, 0x00, 0x08, 0x2a, 0x1c, 0x08, 0x1c, 0x2a, 0x08, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3e, 0x08, 0x08, 0x00, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x08, 0x10},
	{0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0},
	{0, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00, 0},
		/* 0x30 */
	{0, 0x3e, 0x41, 0x43, 0x45, 0x49, 0x51, 0x61, 0x41, 0x41, 0x3e, 0},
	{0, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1c, 0},
	{0, 0x3e, 0x41, 0x41, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x7f, 0},
	{0, 0x3e, 0x41, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x01, 0x41, 0x3e, 0},
	{0, 0x02, 0x06, 0x0a, 0x12, 0x22, 0x42, 0x7f, 0x02, 0x02, 0x02, 0},
	{0, 0x7f, 0x40, 0x40, 0x40, 0x7e, 0x41, 0x01, 0x01, 0x41, 0x3e, 0},
	{0, 0x3e, 0x41, 0x40, 0x40, 0x7e, 0x41, 0x41, 0x41, 0x41, 0x3e, 0},
	{0, 0x7f, 0x41, 0x41, 0x42, 0x02, 0x04, 0x04, 0x08, 0x08, 0x08, 0},
	{0, 0x3e, 0x41, 0x41, 0x41, 0x3e, 0x41, 0x41, 0x41, 0x41, 0x3e, 0},
	{0, 0x3e, 0x41, 0x41, 0x41, 0x41, 0x3f, 0x01, 0x01, 0x01, 0x3e, 0},
	{0, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x08, 0x10, 0},
	{0, 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x02, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x3e, 0x00, 0x00, 0},
	{0, 0x00, 0x20, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x20, 0},
	{0, 0x00, 0x1c, 0x22, 0x02, 0x02, 0x04, 0x08, 0x08, 0x00, 0x08, 0},
		/* 0x40 */
	{0, 0x00, 0x3e, 0x41, 0x4d, 0x55, 0x55, 0x55, 0x4a, 0x40, 0x3e, 0},
	{0, 0x08, 0x08, 0x14, 0x14, 0x22, 0x3e, 0x22, 0x41, 0x41, 0x41, 0},
	{0, 0x7e, 0x21, 0x21, 0x21, 0x3e, 0x21, 0x21, 0x21, 0x21, 0x7e, 0},
	{0, 0x3e, 0x41, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x41, 0x3e, 0},
	{0, 0x7c, 0x22, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x22, 0x7c, 0},
	{0, 0x7f, 0x20, 0x20, 0x20, 0x3e, 0x20, 0x20, 0x20, 0x20, 0x7f, 0},
	{0, 0x7f, 0x20, 0x20, 0x20, 0x3e, 0x20, 0x20, 0x20, 0x20, 0x20, 0},
	{0, 0x3e, 0x41, 0x40, 0x40, 0x4f, 0x41, 0x41, 0x41, 0x43, 0x3d, 0},
	{0, 0x41, 0x41, 0x41, 0x41, 0x7f, 0x41, 0x41, 0x41, 0x41, 0x41, 0},
	{0, 0x1c, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1c, 0},
	{0, 0x07, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x42, 0x3c, 0},
	{0, 0x42, 0x44, 0x48, 0x50, 0x60, 0x50, 0x48, 0x44, 0x42, 0x41, 0},
	{0, 0x70, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x21, 0x7f, 0},
	{0, 0x41, 0x63, 0x55, 0x49, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0},
	{0, 0x41, 0x61, 0x51, 0x49, 0x45, 0x43, 0x41, 0x41, 0x41, 0x41, 0},
	{0, 0x3e, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x3e, 0},
		/* 0x50 */
	{0, 0x7e, 0x41, 0x41, 0x41, 0x7e, 0x40, 0x40, 0x40, 0x40, 0x40, 0},
	{0, 0x3e, 0x41, 0x41, 0x41, 0x41, 0x41, 0x49, 0x45, 0x43, 0x3e, 0},
	{0, 0x7e, 0x41, 0x41, 0x41, 0x41, 0x7e, 0x48, 0x44, 0x42, 0x41, 0},
	{0, 0x3e, 0x41, 0x40, 0x40, 0x38, 0x06, 0x01, 0x01, 0x41, 0x3e, 0},
	{0, 0x7f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0},
	{0, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x3e, 0},
	{0, 0x41, 0x41, 0x22, 0x22, 0x22, 0x14, 0x14, 0x14, 0x08, 0x08, 0},
	{0, 0x41, 0x41, 0x49, 0x49, 0x55, 0x55, 0x55, 0x22, 0x22, 0x22, 0},
	{0, 0x41, 0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41, 0x41, 0x41, 0},
	{0, 0x41, 0x41, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0},
	{0, 0x7f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x10, 0x20, 0x40, 0x7f, 0},
	{0, 0x1e, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1e, 0},
	{0, 0x00, 0x00, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0},
	{0, 0x3c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x3c, 0},
	{0, 0x08, 0x14, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0},
		/* 0x60 */
	{0, 0x10, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x21, 0x0f, 0x11, 0x21, 0x1f, 0},
	{0, 0x60, 0x20, 0x20, 0x20, 0x3e, 0x21, 0x21, 0x21, 0x21, 0x7e, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x21, 0x20, 0x20, 0x21, 0x1e, 0},
	{0, 0x03, 0x01, 0x01, 0x01, 0x1f, 0x21, 0x21, 0x21, 0x21, 0x1f, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x21, 0x3f, 0x20, 0x21, 0x1e, 0},
	{0, 0x06, 0x09, 0x08, 0x08, 0x1e, 0x08, 0x08, 0x08, 0x08, 0x08, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x21, 0x21, 0x1f, 0x01, 0x21, 0x1e},
	{0, 0x20, 0x20, 0x20, 0x20, 0x3e, 0x21, 0x21, 0x21, 0x21, 0x21, 0},
	{0, 0x00, 0x00, 0x04, 0x00, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x1e, 0},
	{0, 0x00, 0x00, 0x02, 0x00, 0x06, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c},
	{0, 0x20, 0x20, 0x20, 0x20, 0x22, 0x24, 0x28, 0x34, 0x22, 0x21, 0},
	{0, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x02, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x76, 0x49, 0x49, 0x49, 0x49, 0x49, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x21, 0x21, 0x21, 0x21, 0x21, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x21, 0x21, 0x21, 0x21, 0x1e, 0},
		/* 0x70 */
	{0, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x21, 0x21, 0x21, 0x3e, 0x20, 0x20},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x21, 0x21, 0x21, 0x1f, 0x01, 0x01},
	{0, 0x00, 0x00, 0x00, 0x00, 0x2e, 0x31, 0x20, 0x20, 0x20, 0x20, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x20, 0x18, 0x06, 0x01, 0x3e, 0},
	{0, 0x00, 0x00, 0x08, 0x08, 0x1e, 0x08, 0x08, 0x08, 0x08, 0x06, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x21, 0x21, 0x21, 0x21, 0x21, 0x1f, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x41, 0x22, 0x22, 0x14, 0x14, 0x08, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x49, 0x49, 0x55, 0x55, 0x36, 0x22, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x21, 0x12, 0x0c, 0x0c, 0x12, 0x21, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x21, 0x22, 0x14, 0x14, 0x08, 0x10, 0x20},
	{0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x02, 0x04, 0x08, 0x10, 0x3f, 0},
	{0, 0x06, 0x08, 0x08, 0x08, 0x10, 0x08, 0x08, 0x08, 0x08, 0x06, 0},
	{0, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0},
	{0, 0x30, 0x08, 0x08, 0x08, 0x04, 0x08, 0x08, 0x08, 0x08, 0x30, 0},
	{0, 0x19, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0},
	{0, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x1c, 0x1c, 0x00, 0x00, 0x00, 0}
};


#if 0
static	void	wait025us(void)
{
	long	l;
	
	for (l=1; l>0; l--)
		asm("nop");
}


static	void	wait05us(void)
{
	long	l;
	
	for (l=7; l>0; l--)
		asm("nop");
}
#endif


static	void	wait1us(void)
{
	long	l;
	
	for (l=15; l>0; l--)
		asm("nop");
}


#if 0
static	void	wait10us(void)
{
	long	l;
	
	for (l=150; l>0; l--)
		asm("nop");
}
#endif


static	void	wait100us(void)
{
	long	l;
	
	if ((lcdtp_polltask))
		lcdtp_polltask();
	
	for (l=1500; l>0; l--)
		asm("nop");
}


void	dly_tsk(W ms)
{
	ms *= 10;
	while (ms-- > 0)
		wait100us();
}


static	void	lcd_wait()
{
	while (SPI1STATbits.SRMT == 0)
		if ((lcdtp_polltask))
			lcdtp_polltask();
}


static	void	lcd_write(UB v)
{
	
	if ((lcdtp_polltask)) {
		static	W	count = 0;
		
		if (count-- <= 0) {
			count = 200;
			lcdtp_polltask();
		}
	}
	
	while ((SPI1STATbits.SPITBF))
		if ((lcdtp_polltask))
			lcdtp_polltask();
	SPI1BUF = v;
	return;
}


static	W	gettpinner(UW cmd)
{
	W	i, ret;
	
	if ((lcdtp_polltask))
		lcdtp_polltask();
	
	LAT_LSS = 1;
	
	RPB5R = 0;		/* port */
	SPI1CON = 0;
	LAT_LCK_TCK = 1;
	
	TRIS_LRS_T2P = 1;		/* input */
	LAT_TSS_SDA = 0;
	
	for (i=0; i<8; i++) {
		wait1us();
		LAT_P2L_P2T = (cmd & (0x80 >> i))? 1 : 0;
		wait1us();
		LAT_LCK_TCK = 0;
		wait1us();
		LAT_LCK_TCK = 1;
	}
	
	wait1us();
	wait1us();
	wait1us();
	wait1us();
	wait1us();
	LAT_P2L_P2T = 0;
	
	ret = 0;
	for (i=0; i<16; i++) {
		wait1us();
		LAT_LCK_TCK = 0;
		wait1us();
		LAT_LCK_TCK = 1;
		
		if ((PORT_T2P))
			ret |= (0x8000 >> i);
	}
	
	LAT_TSS_SDA = 1;
	TRIS_LRS_T2P = 0;		/* output */
	
	RPB5R = 3;		/* SDO1 */
	SPI1CON = 0;
	SPI1CONbits.ENHBUF = 1;
	SPI1CONbits.CKE = 1;
	SPI1CONbits.MSTEN = 1;
	SPI1CONbits.DISSDI = 1;
	SPI1CONbits.STXISEL = 3;
	SPI1STAT = 0;
	SPI1BRG = 0;
	SPI1CONbits.ON = 1;
	
	return ret;
}


static	W	gettpavg(UW cmd)
{
#define	BOXES	0x40
	static	W	count[BOXES + 1], sum[BOXES + 1];
	W	i, v;
	
	for (i=0; i<=BOXES; i++)
		count[i] = sum[i] = 0;
	
	for (;;) {
		v = gettpinner(cmd);
		i = (v >> 10) & (BOXES - 1);
		count[i]++;
		sum[i] += v;
		count[i + 1]++;
		sum[i + 1] += v;
		
		if (count[i] >= 8)
			return (sum[i] + 4) >> 3;
		if (count[i + 1] >= 8)
			return (sum[i + 1] + 4) >> 3;
	}
}


void	gfil_rec(W l, W t, W r, W b, UW color)
{
	W	i;
	
	if ((lcdtp_polltask))
		lcdtp_polltask();
	
	if (l < 0)
		l = 0;
	if (r > LCD_W)
		r = LCD_W;
	if (t < 0)
		t = 0;
	if (b >= LCD_H)
		b = LCD_H;
	if (l >= r)
		return;
	if (t >= b)
		return;
	
	r--;
	b--;
	LAT_LSS = 0;
	LAT_LRS = 0;
	lcd_write(0x2a);
	lcd_wait();
	
	LAT_LRS = 1;
	lcd_write(l >> 8);		/* left */
	lcd_write(l & 0xff);
	lcd_write(r >> 8);		/* right */
	lcd_write(r & 0xff);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0x2b);
	lcd_wait();
	
	LAT_LRS = 1;
	lcd_write(t >> 8);		/* top */
	lcd_write(t & 0xff);
	lcd_write(b >> 8);		/* bottom */
	lcd_write(b & 0xff);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0x2c);
	lcd_wait();
	
	LAT_LRS = 1;
	
	i = (r - l + 1) * (b - t + 1);
	while (i-- > 0) {
		lcd_write(color >> 8);
		lcd_write(color & 0xff);
	}
	lcd_wait();
	LAT_LSS = 1;
}


void	gdra_stp(W x, W y, UW color, UW bgcolor, W font, const UB *s)
{
	W	c, len, y0;
	
	if ((lcdtp_polltask))
		lcdtp_polltask();
	
	if (s == NULL)
		return;
	if ((y -= 11) < 0)
		y = 0;
	if (y > LCD_H - 12)
		y = LCD_H - 12;
	if (x < 0)
		x = 0;
	
	len = 0;
	while ((s[len])) {
		if (x + (len + 1) * 8 > LCD_W)
			break;
		len++;
	}
	LAT_LSS = 0;
	LAT_LRS = 0;
	lcd_write(0x2a);
	lcd_wait();
	
	LAT_LRS = 1;
	lcd_write(x >> 8);		/* left */
	lcd_write(x & 0xff);
	lcd_write((x + 8 * len - 1) >> 8);		/* right */
	lcd_write((x + 8 * len - 1) & 0xff);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0x2b);
	lcd_wait();
	
	LAT_LRS = 1;
	lcd_write(y >> 8);		/* top */
	lcd_write(y & 0xff);
	lcd_write((y + 11) >> 8);		/* bottom */
	lcd_write((y + 11) & 0xff);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0x2c);
	lcd_wait();
	
	LAT_LRS = 1;
	
	for (y0=0; y0<12; y0++) {
		W	x0, pos;
		
		for (pos=0; pos<len; pos++) {
			UB	v;
			
			c = s[pos];
			if ((c -= 0x20) < 0)
				c = 0;
			if (c >= 0x60)
				c = 0;
			
			v = font12n[c][y0];
			for (x0=0; x0<8; x0++)
				if ((v & (0x80 >> x0))) {
					lcd_write(color >> 8);
					lcd_write(color & 0xff);
				} else {
					lcd_write(bgcolor >> 8);
					lcd_write(bgcolor & 0xff);
				}
			}
		x += 8;
	}
	
	lcd_wait();
	LAT_LSS = 1;
}


W	gget_stw(W font, const UB *s)
{
	W	len;
	
	if ((lcdtp_polltask))
		lcdtp_polltask();
	
	len = 0;
	while ((*(s++)))
		len += 8;
	return len;
}


void	init_lcdtp(void)
{
	CNPUA = 0xffff;
	CNPUB = 0xffff;
	
	CNPDB = 0;
	CNPDB = 0;
	
	TRISA = 0x0000;		/* -------- ---O--OO */
	TRISB = 0x0000;		/* OOO---OO O-OOOOOO */
	
	ANSELA = 0;
	ANSELB = 0;
	
	PORTA = 0xffff;
	PORTB = 0xffff;
	
	SYSKEY = 0;
	SYSKEY = 0xaa996655;
	SYSKEY = 0x556699aa;
	SYSKEY = 0;
	
	gettpinner(0);		/* setup SPI1 */
	
	/* ref http://www.lcdwiki.com/res/Program/Common_SPI/2.8inch/SPI_ILI9341_MSP2807_V1.1/2.8inch_SPI_Module_ILI9341_MSP2807_V1.1.zip */
	
	LAT_LSS = 0;
	dly_tsk(1);
	LAT_LRS = 0;
	lcd_write(0xcb);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x39);
	lcd_write(0x2c);
	lcd_write(0x00);
	lcd_write(0x34);
	lcd_write(0x02);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0xcf);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x00);
	lcd_write(0xc1);
	lcd_write(0x30);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0xe8);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x85);
	lcd_write(0x00);
	lcd_write(0x78);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0xea);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x00);
	lcd_write(0x00);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0xed);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x64);
	lcd_write(0x03);
	lcd_write(0x12);
	lcd_write(0x81);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0xf7);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x20);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0xc0);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x23);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0xc1);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x10);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0xc5);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x3e);
	lcd_write(0x28);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0xc7);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x86);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0x36);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x48);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0x3a);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x55);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0xb1);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x00);
	lcd_write(0x18);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0xb6);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0x08);
	lcd_write(0x82);
	lcd_write(0x27);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0x11);
	lcd_wait();
	
	dly_tsk(200);
	
	lcd_write(0x29);
	lcd_write(0x2c);
	lcd_wait();
	LAT_LRS = 1;
	
	LAT_LSS = 1;
	
	dly_tsk(200);
	
	LAT_LSS = 0;
	LAT_LRS = 0;
	lcd_write(0x2a);
	lcd_wait();
	
	LAT_LRS = 1;
	lcd_write(0);		/* left */
	lcd_write(0);
	lcd_write(0);		/* right */
	lcd_write(240);
	lcd_wait();
	
	LAT_LRS = 0;
	lcd_write(0x2b);
	lcd_wait();
	LAT_LRS = 1;
	lcd_write(0);		/* top */
	lcd_write(0);
	lcd_write(1);		/* bottom */
	lcd_write(0);
	lcd_wait();
	LAT_LRS = 0;
	lcd_write(0x2c);
	lcd_wait();
	LAT_LRS = 1;
	
	gettp();
}


UW	gettp()
{
	static	W	pressed = 1;
	W	x, y;
	UW	z0, z1;
	
	z0 = gettpinner(0xb0);
	z1 = gettpinner(0xc0);
	if (z0 < (8 << 7)) {
		if (z0 < (4 << 7))
			pressed = 0;
		return TPLIB_CMD_NULL;
	}
	if (z1 > (0xf8 << 7)) {
		if (z1 > (0xfc << 7))
			pressed = 0;
		return TPLIB_CMD_NULL;
	}
	x = gettpinner(0xd0);
	y = gettpinner(0x90);
	z0 = gettpinner(0xb0);
	z1 = gettpinner(0xc0);
	if (z0 < (8 << 7)) {
		if (z0 < (4 << 7))
			pressed = 0;
		return TPLIB_CMD_NULL;
	}
	if (z1 > (0xf8 << 7)) {
		if (z1 > (0xfc << 7))
			pressed = 0;
		return TPLIB_CMD_NULL;
	}
	if ((pressed))
		return TPLIB_CMD_NULL;
	pressed = 1;
	
	x = (0x7800 - x) * LCD_W / 0x7000;
	y = (y - 0x800) * LCD_H / 0x7000;
	
	if (x < 0)
		x = 0;
	if (x >= LCD_W)
		x = LCD_W - 1;
	if (y < 0)
		y = 0;
	if (y >= LCD_H)
		y = LCD_H - 1;
	
	if ((lcdtp_flip & LCDTP_FLIP_X))
		x = LCD_W - 1 - x;
	if ((lcdtp_flip & LCDTP_FLIP_Y))
		y = LCD_H - 1 - y;
	
	return TPLIB_CMD_PRESS | (x << 12) | y;
}


