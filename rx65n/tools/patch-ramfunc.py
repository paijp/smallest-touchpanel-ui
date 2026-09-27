#!/usr/bin/env python3
"""Add a .ramfunc section to the fetched linker script, for solo.c's
-DSOLO_RAMCODE build.

The section's addresses are in RAM and its contents load from ROM, alongside
.data; main() copies it across before calling anything in it. That is what lets
the same program be run once out of flash and once out of RAM, which is the one
remaining way to test whether the fault is in the instruction fetch itself - the
code is correct and every loop in it is bounded, and it still leaves the source's
control flow.

It goes in after .bss so the existing addresses do not move: .data stays at
0x504 and .bss where it was, which keeps a build with this patch comparable with
one without it.

Run it after fetching generate/, before make. Idempotent.
"""
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "generate/linker_script.ld"

# 0x2000 is clear of the stacks (0x100..0x500) and of .data and .bss, which
# together end below 0x1600 in every build of solo.c so far.
SECTION = """	.ramfunc 0x2000 : AT(_mdata + SIZEOF(.data))
	{
		_ramfunc = .;
		*(.ramfunc)
		*(.ramfunc.*)
		_eramfunc = .;
	} > RAM
	_mramfunc = LOADADDR(.ramfunc);
"""

ANCHOR = """		_ebss = .;
		_end = .;
	} > RAM
"""

text = open(PATH).read()
if ".ramfunc" in text:
    print("already patched")
    sys.exit(0)
if ANCHOR not in text:
    print("could not find the end of .bss to insert after; check the script")
    sys.exit(1)

text = text.replace(ANCHOR, ANCHOR + SECTION, 1)
open(PATH, "w").write(text)
print("added .ramfunc at 0x2000, loading after .data")
