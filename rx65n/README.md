# RX65N Envision Kit port

Runs on the Renesas RX65N Envision Kit (R5F565NEDDFB, 480x272 LCD with an
FT5x06 capacitive touch panel), built with Renesas' GNU RX toolchain.

```
lcdtp.c   lcdtp.h    the port: drawing, touch, timing            Apache 2.0
basic.h              types                                       Apache 2.0
tplib.c   tplib.h    the GUI library, unchanged from the other ports
sample1.c            the same sample the x11 and pic32mx ports build
debuglog.c debuglog.h   debug log sink                            Apache 2.0
envision_hw.c .h     board bring-up: clock, GLCDC, touch I2C      MIT
tools/readlog.py     host side of the debug log
```

## Building and flashing

The build and flash harness lives in
[paijp/rx65n](https://github.com/paijp/rx65n), which fetches this port and
the RX65N startup files, compiles them and writes the result to the board
over an E2 Lite:

```bash
bash install-toolchain.sh gcc-14.2.0.202607-GNURX-ELF-linux.tar.gz
bash fetch-lcdtp.sh
make DEMO=lcdtp
```

## What the port had to do, and what it did not

Almost nothing, for the drawing half. The GLCDC scans a plain linear RGB565
framebuffer out of expansion RAM at `0x00800000`, so a pixel is a 16-bit
store at `base + y * 480 + x`. There is no window command, no chip select and
no bus transaction per pixel, which is the opposite of the pic32mx port's
ILI9341 over SPI. And because lcdtp already works in RGB565, the colour
arrives in the format the hardware wants; the x11 port's `color16to32` exists
only because X wants 32 bits per pixel.

So `gfil_rec` is a clamp and two loops, `gdra_stp` is the x11 glyph loop with
its one pixel-writing line changed, and `gget_stw`, the font table and the
bit decoder are carried over untouched. There is no `update_lcd()` at the end
of either: the controller is already scanning that memory, continuously.

`init_lcdtp` clears the framebuffer rather than assuming it starts blank -
expansion RAM holds whatever survived the reset and the GLCDC is already
displaying it.

## The parts that are the board's, not the library's

`envision_hw.c` holds the system clock setup, the ~130 GLCDC register writes
and the touch controller's I2C. Those register sequences come from
[miniwinwm/RenesasEnvisionGCC](https://github.com/miniwinwm/RenesasEnvisionGCC),
which is MIT licensed, so they are kept in a file of their own rather than
mixed into Apache 2.0 sources. They are panel timings and pin assignments -
values that are only correct for this board and could not be usefully checked
by rederiving them.

Three things there are deliberately not the original's:

**The I2C polls instead of waiting on interrupt handlers.** The reference
driver spins on flags set by three ISRs, which is the same hardware condition
reached the long way round: with `SIMR2.IICINTM = 1` the TXI and RXI requests
*are* `SSR.TDRE` and `SSR.RDRF`, and the group BL0 handler does nothing but
clear and report `SIMR3.IICSTIF`. Reading those flags directly drops the
dependency on a particular `inthandler.c`, and drops a bug with it: nothing
clears the flags between transactions, so one abandoned part way through
leaves the next reading stale data and sliding one step further out of step
each time. On hardware that looked like "first touch fine, second touch
nothing, board frozen".

The peripheral configuration is otherwise unchanged, `SCR = 0xb4` included.
The interrupt requests are still generated; they are simply never enabled in
the ICU.

**Every wait is bounded.** The original's are `while (!flag) {}`, so a missing
ACK hangs the program with nothing to look at. Here each gives up after a
timeout, the transaction fails, and `gettp` reports no touch and tries again
next pass.

**The power-up phantom touch reports nothing** rather than a touch at an
uninitialised coordinate. The panel asserts a contact before anyone has
touched it; its interrupt line is the one trustworthy signal at that point,
so the port waits for that once and polls the controller from then on.

## Press and pressing

The controller reports a coordinate, not an event, so `gettp` derives the
distinction the way the pic32mx port does: the first reading of a contact is
`TPLIB_CMD_PRESS`, every reading while it stays down is `TPLIB_CMD_PRESSING`,
and a reading with no contact ends it. A bus error counts as no contact,
which at worst splits a drag in two - keeping the old state across an error
would instead leave a button stuck down.

## Debug logging

The Envision Kit has no UART the host can reach. Its E2 Lite presents a
single vendor-specific USB interface with two bulk endpoints and nothing else
- no CDC, so no `/dev/ttyACM*` to print to. The Pmod connector does carry
SCI9, but using it means adding a USB-serial adapter.

What the board does have is Renesas' **RRM/DMM** - real-time RAM monitoring -
which `e2-server-gdb` exposes with `-uAllowRRMDMM=1`. That lets the host read
target RAM *without halting the CPU*, which turns an ordinary ring buffer
into a live console over the USB cable that is already attached.

The alternative, writing to data flash and halting to dump it with
`rfp-cli -rv`, also works and needs no debugger - but it stops the program,
wears the flash, and is far too slow for anything chatty.

```bash
e2-server-gdb -g E2LITE -t R5F565NE -p 61234 -d 61236 \
    -uAllowRRMDMM=1 -uWorkRamAddress=0x90000 -n 0 &
python3 tools/readlog.py lcdtp.elf
```

`-uWorkRamAddress` matters more than it looks. The device description in
e2 studio's `e2_devices.xml` puts this target's debugger work RAM at 0x1000
for 1280 bytes, which is inside `.data` - and `.data` is where the ring
buffer lives, from 0x504. Left at the default the debugger would quietly
overwrite the log it was being used to read. Internal RAM runs to 0xA0000
and the program ends below 0x2000, so anywhere high is safe.

### It does not currently connect on this board

Worth recording, because it cost a day: on this Envision Kit the debugger
never gets a link to the MCU. Every attempt ends at the same first step,

```
Firmware up to date at version '1.12.00.001'
E20_set_clk() Failed
RxTargetDevice::startConnection() Rx_Init_E1_E20() Failed
```

identically for `-uUseFine=1` and for `-uJTagClockFreq` at 16.5, 6.0 and
1.5 MHz, before and after a fresh USB attach. The emulator itself is fine -
it is found, its interface is claimed, its bulk transfers all complete and
it reports its own firmware version - so the failure is on the emulator's
far side, between it and the MCU.

That fits what `rfp-cli` already showed independently: `-if uart` programs
this board reliably, `-if fine` gets as far as connecting the emulator and
then fails as though the target were dead. The on-board E2 Lite reaches the
RX65N over the SCI boot path; the FINE/JTAG debug link is not coming up.
No debug link means no RRM/DMM, so this route is unavailable here until
that is sorted out.

Which is why diag1.c exists. The screen is the debug channel that does
work.

The buffer's address is resolved from the `.elf` with `nm`, so nothing is
hardcoded and it can move freely between builds. (The RX ABI prefixes C
symbols with an underscore, so the symbol is `_debuglog`; the script accepts
either spelling.)

`lcdtp_sendlogs`, `lcdtp_sendlogdec`, `lcdtp_sendlogun`, `lcdtp_sendlogub`,
`lcdtp_sendloguh` and `lcdtp_sendloguw` are pure formatting on top of
`lcdtp_sendlogc` and are byte-identical across the ports, so they live in
`lcdtp.c` here as they do elsewhere; only `lcdtp_sendlogc` is in
`debuglog.c`.

### How the ring buffer stays consistent without a lock

One writer on the target, one reader on the host, and the reader never
writes.

`wr` counts bytes ever written and never wraps. The writer stores the byte
*before* bumping `wr`, so a reader that samples mid-call sees the old count
and picks the byte up on its next pass rather than reading a slot that has
not been filled in yet.

Because `wr` is free-running, the reader can also tell when the writer lapped
it - `wr - seen > size` - and says so rather than quietly printing a garbled
window. An index that wrapped could not distinguish an overrun from no
progress at all.

The struct carries `magic` and `size` so the reader can confirm it is looking
at a real buffer (a stale `.elf`, or a target that never started, both show
up as garbage) without assuming the layout it was compiled against.

It lives in `.data`, not `.bss`: `magic` and `size` are correct before
`main()` runs, so a reader attaching to an already-running target never
catches a half-initialised header. Startup zeroes `.bss`, which would defeat
that.

## Verified

- `lcdtp.c` and `debuglog.c` compile with no warnings at `-O2 -Wall -Wextra`
- the whole program links with GNU RX 14.2.0.202607 for `-mcpu=rx64m`:
  text 11373, data 5844, bss 77, and a 51584-byte `.mot` to flash
- the framebuffer address is outside the linker's RAM region (which is
  0x0 + 640KB), so nothing the linker places can collide with it
- debug log lands in `.data` as intended: `_debuglog` at 0x504, reading
  `44 50 47 4c 00 10 00 00` - `DPGL` and size 0x1000
- ring buffer wrap, overrun accounting and idle polling, against a simulation
  of both sides

One build-side wrinkle: GCC 14 rejects incompatible pointer types outright,
and `tplib.h` declares `tplib_systemfont` as `struct tplib_font_struct *` -
an incomplete type that is defined nowhere and is really the
`struct lcdtp_font_struct *` that `gdra_stp` takes. The rx65n build passes
`-Wno-error=incompatible-pointer-types` rather than diverge its copy of
tplib from the other ports'. Fixing the declaration would be one line, in
one place, for all three.

`sample1.c` is laid out for the 240x320 screen the other ports have, so on a
480x272 panel its lower parts fall off the bottom. Everything clips safely;
it is a layout question, not a port question.

**Not yet run on hardware.** The board was disconnected while this was
written, so the display and touch paths have been checked by reading and by
building, not by looking at a screen. The `e2-server-gdb` side reaches
`can not connect to the emulator`, which is the correct response with no
board attached.
