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
import os
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
    # Only the program name, not the whole path: a plain replace also hits
    # directory names, and the Renesas server lives in one called e2gdb.
    head, tail = os.path.split(gdb)
    nm = os.path.join(head, tail[::-1].replace("bdg", "mn", 1)[::-1])
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

    def __init__(self, gdb, elf, port, host="localhost"):
        self.p = subprocess.Popen(
            [gdb, "-q", "-nx", "--interpreter=mi2", elf],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, bufsize=1,
        )
        # gdb prints a prompt before it has been asked anything. cmd() reads
        # up to the next prompt, so without swallowing this one first every
        # reply would be paired with the *previous* command, and the memory
        # read would be handed the output of the connect instead of its own
        # - which is precisely how the first run of this read nothing and
        # reported it as a zero magic.
        self.wait_prompt()
        self.cmd("set non-stop on")
        self.cmd("set confirm off")
        self.cmd("target extended-remote %s:%d" % (host, port))
        # No resume here. With the option set in the README the server
        # attaches to the target as it is - running, if it was running - and
        # a `continue` against a running thread is an error, not a no-op.

    def wait_prompt(self, timeout=10.0):
        out, deadline = [], time.time() + timeout
        while time.time() < deadline:
            line = self.p.stdout.readline()
            if not line:
                break
            out.append(line)
            if line.startswith("(gdb)"):
                break
        return "".join(out)

    def cmd(self, s, timeout=10.0):
        self.p.stdin.write(s + "\n")
        self.p.stdin.flush()
        return self.wait_prompt(timeout)

    def read(self, addr, count):
        """Read count bytes. Returns None if the target refused.

        MI's own memory command rather than `x`: it answers with one
        contents="..." hex string, where `x` answers with CLI text that MI
        wraps, escapes and interleaves with warnings, and parsing that back
        out is exactly what went wrong the first time this ran for real.
        """
        txt = self.cmd("-data-read-memory-bytes 0x%x %d" % (addr, count))
        m = re.search(r'contents="([0-9a-fA-F]+)"', txt)
        if not m:
            return None
        data = bytes.fromhex(m.group(1))
        return data[:count] if len(data) >= count else None

    def word(self, addr):
        b = self.read(addr, 4)
        return None if b is None else int.from_bytes(b, "little")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("--gdb", default="/opt/e2gdb/rx-elf-gdb")
    ap.add_argument("--port", type=int, default=61234)
    # The GDB server has to run where the emulator is plugged in; gdb does
    # not, and that machine may be a small VM with no room for it.
    ap.add_argument("--host", default="localhost")
    ap.add_argument("--interval", type=float, default=0.2)
    args = ap.parse_args()

    base = symbol_address(args.elf, args.gdb, "debuglog")
    t = Target(args.gdb, args.elf, args.port, args.host)

    magic = t.word(base)
    if magic is None:
        sys.exit("could not read target memory at 0x%08x -- is the GDB "
                 "server up, and has it already served its one session?"
                 % base)
    if magic != MAGIC:
        sys.exit("bad magic 0x%08x at 0x%08x -- wrong .elf, or the target "
                 "has not started" % (magic, base))
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
