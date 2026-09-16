#!/usr/bin/env python3
"""Stream the target's debug log off a running RX65N, via e2-server-gdb.

    e2-server-gdb -g E2LITE -t R5F565NE -p 61234 -d 61236 -uAllowRRMDMM=1 -n 0 &
    python3 readlog.py sample1.elf

Reads the ring buffer in debuglog.c over GDB's remote protocol and prints
whatever the firmware has written since the last poll. The target keeps
running throughout -- that is what -uAllowRRMDMM=1 buys, and it is the whole
reason for doing it this way rather than writing to data flash and halting to
dump it.

The buffer's address comes from the .elf, so nothing here hardcodes a layout;
rebuild and move it around freely.
"""

import argparse
import re
import subprocess
import sys
import time

MAGIC = 0x4C475044          # must match DEBUGLOG_MAGIC
HEADER = 16                 # magic, size, wr, pad


def symbol_address(elf, gdb, name):
    """Resolve a symbol without needing the target: nm on the .elf.

    The RX ABI prefixes C symbols with an underscore, so the symbol is
    _debuglog in the ELF even though it is debuglog in the source. Accept
    either, since the same script should work if it is ever pointed at a
    target whose ABI does not.
    """
    nm = gdb.replace("gdb", "nm")
    out = subprocess.run([nm, elf], capture_output=True, text=True).stdout
    wanted = (name, "_" + name)
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] in wanted:
            return int(parts[0], 16)
    sys.exit("%s not found in %s -- is debuglog.c linked in?" % (name, elf))


class Target:
    """A long-lived gdb driven through its own stdin.

    One gdb process for the whole session, not one per poll: connecting costs
    far more than a read, and reconnecting repeatedly would disturb a target
    we are trying to observe undisturbed.
    """

    def __init__(self, gdb, elf, port):
        self.p = subprocess.Popen(
            [gdb, "-q", "-nx", "--interpreter=mi2", elf],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, bufsize=1,
        )
        self.cmd("set non-stop on")
        self.cmd("set confirm off")
        self.cmd("target extended-remote localhost:%d" % port)

    def cmd(self, s, timeout=10.0):
        self.p.stdin.write(s + "\n")
        self.p.stdin.flush()
        out, deadline = [], time.time() + timeout
        while time.time() < deadline:
            line = self.p.stdout.readline()
            if not line:
                break
            out.append(line)
            if line.startswith("(gdb)"):
                break
        return "".join(out)

    def read(self, addr, count):
        """Read count bytes. Returns None if the target refused."""
        txt = self.cmd("x/%dxb 0x%x" % (count, addr))
        data = bytearray()
        for line in txt.splitlines():
            if ":" not in line:
                continue
            for tok in re.findall(r"0x([0-9a-fA-F]{1,2})\b", line.split(":", 1)[1]):
                data.append(int(tok, 16))
        return bytes(data[:count]) if len(data) >= count else None

    def word(self, addr):
        b = self.read(addr, 4)
        return None if b is None else int.from_bytes(b, "little")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("--gdb", default="/opt/e2gdb/rx-elf-gdb")
    ap.add_argument("--port", type=int, default=61234)
    ap.add_argument("--interval", type=float, default=0.2)
    args = ap.parse_args()

    base = symbol_address(args.elf, args.gdb, "debuglog")
    t = Target(args.gdb, args.elf, args.port)

    magic = t.word(base)
    if magic != MAGIC:
        sys.exit("bad magic 0x%08x at 0x%08x -- wrong .elf, or the target "
                 "has not started" % (magic or 0, base))
    size = t.word(base + 4)

    # Start from where the target is now rather than replaying the buffer: on
    # attach the history is whatever survived, and printing it as if it were
    # live is more confusing than useful.
    seen = t.word(base + 8)

    try:
        while True:
            wr = t.word(base + 8)
            if wr is None:
                time.sleep(args.interval)
                continue

            pending = wr - seen
            if pending > size:
                # The writer lapped us. Say so -- silently skipping would hide
                # exactly the burst that is usually worth seeing.
                sys.stderr.write("\n[readlog: dropped %d bytes]\n"
                                 % (pending - size))
                seen = wr - size
                pending = size

            if pending:
                start = seen % size
                if start + pending <= size:
                    chunk = t.read(base + HEADER + start, pending)
                else:
                    first = size - start
                    a = t.read(base + HEADER + start, first)
                    b = t.read(base + HEADER, pending - first)
                    chunk = None if a is None or b is None else a + b
                if chunk is not None:
                    sys.stdout.write(chunk.decode("latin-1"))
                    sys.stdout.flush()
                    seen += pending

            time.sleep(args.interval)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
