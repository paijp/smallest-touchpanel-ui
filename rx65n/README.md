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

`fetch-lcdtp.sh` pulls two things: this port, and the `generate/` directory
from [miniwinwm/RenesasEnvisionGCC](https://github.com/miniwinwm/RenesasEnvisionGCC)
- the interrupt vectors, reset code, linker script and Renesas' generated
`iodefine.h`. **The port does not build without them**, and not only for the
register names: the touch driver's I2C waits on flags raised by the three
interrupt handlers in that directory's `inthandler.c`.

They are fetched rather than vendored on purpose. `iodefine.h` in particular
is a Renesas-generated file that reaches us via a third party's MIT
licence, and fetching it at build time keeps a question this repository
cannot answer out of this repository.

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
# where the emulator is plugged in
rm -f /dev/shm/sem.CommuniDLL_USB_Semaphore*      # see "one session, one server"
e2-server-gdb -g E2LITE -t R5F565NE_DUAL -p 61234 -d 61236 \
    -uConnectionTimeout= 30 -uClockSrcHoco= 1 -uPTimerClock= 120000000 \
    -uAllowClockSourceInternal= 1 -uUseFine= 0 -uJTagClockFreq= 6.00 \
    -w 0 -z 0 -uRegisterSetting= 0 -uModePin= 0 \
    -uChangeStartupBank= 0 -uStartupBank= 0 -uDebugMode= 0 \
    -uExecuteProgram= 0 -uIdCode= FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \
    -uresetOnReload= 1 -n 0 -uWorkRamAddress= 8000 \
    -uverifyOnWritingMemory= 0 -uProgReWriteIRom= 0 -uProgReWriteDFlash= 0 \
    -uhookWorkRamAddr= 0x3fdd0 -uhookWorkRamSize= 0x230 \
    -uOSRestriction= 0 -l -uCore= 'SINGLE_CORE|enabled|1|main' \
    -uSyncMode= async -uFirstGDB= main -uAllowRRMDMM= 1 &

# anywhere that can reach port 61234 and has a GNU RX gdb
python3 tools/readlog.py lcdtp.elf --gdb rx-elf-gdb --host <that machine>
```

That is what e2 studio itself generates for this board, with three edits:
`-uAllowRRMDMM= 1`, the work RAM moved (below), and `_DUAL`, which is the
part on the Envision Kit. Everything about it that looks odd is
load-bearing.

### The space after `=`

Each value is a separate argv element. Written the ordinary way,
`-uUseFine=1` is an option with an empty value and is silently ignored - it
never reaches the emulator: two runs, with and without it, captured under
`LIBUSB_DEBUG=4`, are the identical 56-transfer conversation. Every attempt
of the form `-uOption=value` fails at

```
E20_set_clk() Failed
RxTargetDevice::startConnection() Rx_Init_E1_E20() Failed
```

which reads like a clock or interface problem and is neither; with the
values attached rather than passed, the server had almost no settings at
all. The interface in particular is a red herring: **JTAG at 6.00MHz** is
what this board wants, not FINE, for all that FINE is the obvious choice for
an E2 Lite on RX. Connected, it reports:

```
Firmware up to date at version '1.12.00.001'
        Target endian (MDE pin)    : little
  Emulator Board Revision       E2LITE  Rev.0
  User Vcc                      3.28711 V
Finished target connection
GDB: 61234
```

### One session, one server, and the semaphore it leaves behind

`libCommuni.so` takes a POSIX named semaphore for the emulator, keyed by
its USB serial:

```
/dev/shm/sem.CommuniDLL_USB_SemaphoreE2L: OBE020003
```

It is released on a clean exit. Kill a connected server instead and it
stays taken, and from then on every start reads the emulator's serial
(two `GET_DESCRIPTOR` string requests, and nothing else on the wire),
fails to take the semaphore, and reports "can not connect to the emulator"
without ever claiming the USB interface. Nothing on the device side clears
it - not a physical re-plug, not `usbip` detach/attach or unbind/bind, not
a forced re-enumeration - because it is a file on the host. `rfp-cli`
works throughout, which is what makes it look like anything but this.
Delete the file.

This cost most of a day, and the diagnosis went through a timeout theory
first: the first control transfer after `libusb_open` does take ~1.0-1.5s
down this path (an emulated VM, slirp, a container, an SSH tunnel, a Pi)
against libusb's 1000ms, and the one run that had connected had made it
with 4ms to spare. Real, worth knowing about, and not the cause: a run
whose first transfer completed in 0.85s failed identically.

Two consequences for how it is run. The server serves **one** gdb session
and then stops listening on its port; start a new server for each
`readlog.py`. And a server should be stopped with a clean gdb disconnect
where possible, or the semaphore removed before the next start - the
`rm -f` above is not optional after a `kill`.

It does **not** attach to the target as it finds it. An earlier version of
this file said it did - that a target left running by `rfp-cli -run` was
still running once the server had connected. That was wrong, and wrong in
the way that costs the most: RAM keeps its contents when the CPU stops, so
a frozen ring buffer reads exactly like a live one, and the numbers coming
back looked perfectly reasonable for days.

Connecting resets the target and leaves it there. The backlight goes out,
`.data` is copied from ROM again so the write pointer returns to zero, and
stopping the server does not release it. Only `rfp-cli -run`, with no
program operation - it prints "No operation" and returns - starts it again.

The reset also wipes the buffer before anything can be read out of it. So
this path currently yields nothing: not a live stream, and not a snapshot
either. Putting the buffer somewhere the startup code does not initialise
would at least make the snapshot work; that is not done yet.

`readlog.py` does not `continue` after connecting. That was written for the
attach-as-found model above, and needs revisiting with the rest of this.

### -uWorkRamAddress

e2 studio passes `1000` (hex, no prefix - the device description says
`workRamStart="4096"`): 1280 bytes of debugger work RAM at 0x1000. That is
inside `.data`, and `.data` is where the ring buffer lives, from 0x504 to
0x1514. `8000` puts it at 0x8000, well above the program (which ends below
0x2000; internal RAM runs to 0xA0000).

### Where gdb runs

The GDB server has to run where the emulator is plugged in. gdb does not,
and the small VM this was done in could not hold both: with the server up,
both the Renesas `rx-elf-gdb` and GNU RX's segfaulted at startup with
~75MB free. `readlog.py --host` lets gdb run on the build machine; it also
needs the matching `rx-elf-nm` beside it, and GNU RX's gdb wants
`libmpfr6`.

`readlog.py` reads memory with MI's `-data-read-memory-bytes` rather than
`x`. The first version parsed `x` output and silently got nothing back
through MI's quoting; a read failure now says so instead of printing a
zero magic.

### What the log said

The first thing this channel was used for was the touch driver, and it
answered in one screen. Eight identical lines per pass:

```
ssr@tdr  00000040     RDRF already set, right after the address byte
ssr@end  000000c0     TDRE and RDRF set, TEND clear, when the wait returned
sisr     00000015     IICACKR = 1: the controller NACKed its address
gaveup   00000000     no timeout anywhere
ok count 00000000     not one transaction completed
```

Two bugs, both in the polled I2C, both visible here and neither guessable:
the transmit wait returned before the acknowledge bit and read it too
early, and a receive flag left over from the address frame made every read
start one byte early. See the note at the top of `envision_hw.c`.

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

### On hardware

The display works, the touch controller answers, and coordinates come back.
The framebuffer was read back through the debugger with the target halted:
it is at 0x00800000, it holds what was drawn, and the last address that
answers is below 0x00840000 - so the 261,120 bytes it needs fit, with about
a kilobyte to spare. Above that, reads return a repeating `03000000
02000000` pattern, which is what unimplemented space gives back rather than
an error. That pattern was previously taken for "reads of a running target
are fabricated"; some of those reads were of addresses that do not exist.

Four bugs found by running it, none of them the one below:

- `.bss` was placed at address 0, so any object at its start had a null
  address and every null check in the codebase rejected it. Fixed in
  patch-demo.py; it is a defect in upstream's linker script.
- ROM wait states were set before raising ICLK past 100MHz but not read
  back, so the clock could go up before the write took effect.
- the exception handlers were empty, so every fault arrived as PC 0 with
  ISP 0 and nothing else. They now record which vector fired and stop.
- 54 relocatable vectors were `(fp)0`, so an unclaimed interrupt jumped to
  address 0 rather than to a handler.

### The fault that is still open

The program stops. Intermittently, usually within seconds, more readily
while the I2C is running, and in upstream's own demo as well as this port.

What it looks like with the handlers in place: a BRK or an undefined
instruction, at a PC that is either in unimplemented space (0x0040Fxxx,
0x007F8102) or one byte into a valid instruction. The interrupt stack and
the stacked PSW are intact and ordinary. Without the display initialised
the symptom changes shape: no exception at all, and
`envision_touch_get_raw()` does not return.

Ruled out, each by measurement rather than by argument: the drawing and its
volume; the I2C transaction itself (a diagnostic calling i2c.h directly
takes touches and keeps running); `lcdtp_polltask` (null, and its indirect
call site was never reached - hardware breakpoint, hit count zero); stack
overflow (124 bytes used of a kilobyte); peripheral interrupts (IER all
zero); user mode (start.S sets it deliberately); the debug console; the
debugger itself; and the clock configuration, which is identical to
upstream's.

Tools for the next attempt are in the rx65n repo: `logrun.sh` programs the
board with the target held, brings up gdb and the console, and releases it
with a listener already attached, so a capture starts at the program's
first byte. `BREAK` takes several locations, which is what a bisect needs,
because this gdb stops answering once the target is running.

## Licence

Apache 2.0, like the rest of the library, with one exception:
`envision_hw.c` and `envision_hw.h` are **MIT**, derived from EnvisionDemo1
in [miniwinwm/RenesasEnvisionGCC](https://github.com/miniwinwm/RenesasEnvisionGCC)
(Copyright (c) 2019 John Blaiklock), and reproduce that licence in full at
the top of each file. MIT permits this as long as the notice travels with
the code; it does not make the rest of the repository MIT.

The split is not cosmetic. Everything taken from that project - the system
clock setup, the ~130 GLCDC register writes, the touch controller's I2C -
is in those two files. `lcdtp.c` and `lcdtp.h` contain none of it: they
touch no register directly and reach the hardware only through the six
functions `envision_hw.h` declares. So the port proper is plain Apache 2.0
paijp code, and only the board bring-up carries the MIT notice.

Worth saying that the upstream is well worth reading, and not only because
its licence is generous: panel timings and pin assignments are values that
can only be confirmed on real hardware. It is also not bug-free - see
`patch-demo.py` in the build harness for the three defects found by running
it.
