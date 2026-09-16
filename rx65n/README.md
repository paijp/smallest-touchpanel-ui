# RX65N Envision Kit port

Port in progress. So far this holds the debug log sink — the one piece of
`lcdtp.c` that had no obvious answer on this board, and the piece the rest of
the porting work will lean on.

## Why a ring buffer and not a serial port

The Envision Kit has no UART broken out that the host can reach. Its E2 Lite
presents a single vendor-specific USB interface with two bulk endpoints and
nothing else — no CDC, so no `/dev/ttyACM*` to print to. The Pmod connector
does carry SCI9, but using it means adding a USB-serial adapter.

What the board does have is Renesas' **RRM/DMM** — real-time RAM monitoring —
which `e2-server-gdb` exposes with `-uAllowRRMDMM=1`. That lets the host read
target RAM *without halting the CPU*, which turns an ordinary ring buffer into
a live console over the USB cable that is already attached.

The alternative, writing to data flash and halting to dump it with
`rfp-cli -rv`, also works and needs no debugger — but it stops the program,
wears the flash, and is far too slow for anything chatty.

## Layout

```
debuglog.h   ring buffer layout
debuglog.c   lcdtp_sendlogc() — the only platform-specific log function
tools/readlog.py   host side: streams the buffer via gdb
```

`lcdtp_sendlogs`, `lcdtp_sendlogdec`, `lcdtp_sendlogun`, `lcdtp_sendlogub`,
`lcdtp_sendloguh` and `lcdtp_sendloguw` are **byte-identical** between the
pic32mx and x11 ports — they are pure formatting on top of `lcdtp_sendlogc` —
so this port does not reimplement them.

## Usage

```bash
e2-server-gdb -g E2LITE -t R5F565NE -p 61234 -d 61236 -uAllowRRMDMM=1 -n 0 &
python3 tools/readlog.py sample1.elf
```

The buffer's address is resolved from the `.elf` with `nm`, so nothing is
hardcoded and it can move freely between builds. (The RX ABI prefixes C
symbols with an underscore, so the symbol is `_debuglog`; the script accepts
either spelling.)

## How it stays consistent without a lock

One writer on the target, one reader on the host, and the reader never writes.

`wr` counts bytes ever written and never wraps. The writer stores the byte
*before* bumping `wr`, so a reader that samples mid-call sees the old count and
picks the byte up on its next pass rather than reading a slot that has not been
filled in yet.

Because `wr` is free-running, the reader can also tell when the writer lapped
it — `wr - seen > size` — and says so rather than quietly printing a garbled
window. An index that wrapped could not distinguish an overrun from no
progress at all.

The struct carries `magic` and `size` so the reader can confirm it is looking
at a real buffer (a stale `.elf`, or a target that never started, both show up
as garbage) without assuming the layout it was compiled against.

It lives in `.data`, not `.bss`: `magic` and `size` are correct before `main()`
runs, so a reader attaching to an already-running target never catches a
half-initialised header. Startup zeroes `.bss`, which would defeat that.

## Verified

- Compiles clean at `-O2` with `-Wall -Wextra` under GNU RX 14.2.0.202607
- Lands in `.data` as intended: 4112 bytes (16 header + 4096 buffer), `.bss` 0
- `DPGL` magic and size `0x00001000` present in the linked image
- Symbol resolution against real `nm` output
- Ring buffer wrap, overrun accounting and idle polling, against a simulation
  of both sides

Not yet run against hardware — the board was disconnected at the time. The
`e2-server-gdb` side is set up and reaches `can not connect to the emulator`,
which is the correct response with no board attached.
