#!/usr/bin/env python3
"""Run all C code out of RAM, by patching the fetched linker script and start.S.

Kept for the record; not needed. The fault it was built to work around was
SCKCR written one field at a time, which left ICK at 0 and ran ICLK at 240 MHz
against this part's 120 MHz maximum, so the code flash was being read at twice
the speed its wait states were set for and instruction fetch came back wrong.
envision_hw.c has the measurement and the fix.

That is also why running from RAM looked like the answer: RAM has no wait
states, so an overclocked CPU can still fetch from it. solo.c's 207 reads from
RAM against a stop on the first pass from flash were a real measurement of a
real effect, pointing at the level below the cause. With the clock right, the
flash-resident build has no reason to fail, and this script has no job left.

What follows is how it works, for anyone who wants code in RAM for some other
reason.

How: -ffunction-sections puts every C function in its own .text.<name>, while
start.S's own code sits in plain .text. So .text stays in ROM and every
.text.* goes to a section whose addresses are in RAM and whose contents load
from ROM, and start.S copies it across before it calls anything - which is why
the copy has to go there rather than in main(): main is one of the functions
being moved.

    bash tools/patch-ramtext.py generate/linker_script.ld generate/start.S

It also has to take *(.text.*) out of the ROM .text section: output sections are
filled in the order the script lists them, so leaving it there means .text
collects every function first and the RAM section comes out empty - which is
exactly what happened the first time.

Needs -ffunction-sections, which this project already builds with. Idempotent.
"""
import sys

LD = sys.argv[1] if len(sys.argv) > 1 else "generate/linker_script.ld"
ST = sys.argv[2] if len(sys.argv) > 2 else "generate/start.S"

# 0x2000 is clear of the stacks (0x100..0x500) and of .data and .bss, which end
# below 0x1600 in these programs, and the framebuffer is elsewhere again - it
# lives in expansion RAM at 0x00800000.
#
# It has to be this low. The first version used 0x10000, which is inside the RAM
# the linker script declares and well clear of everything, and the board produced
# no output at all - not the banner, nothing - while the same build at 0x2000 runs.
# So the usable RAM on this part stops short of what the script claims, and the
# section is kept just above .bss where it is known to work rather than at a
# round address that is not.
RAMTEXT_ADDR = "0x2000"

LD_SECTION = """	.ramtext %s : AT(_mdata + SIZEOF(.data))
	{
		_ramtext = .;
		*(.text.*)
		. = ALIGN(4);
		_eramtext = .;
	} > RAM
	_mramtext = LOADADDR(.ramtext);
""" % RAMTEXT_ADDR

LD_ANCHOR = """		_ebss = .;
		_end = .;
	} > RAM
"""

# .text must stop collecting the per-function sections, or it takes them all
# before .ramtext is reached and the RAM section links empty.
LD_STRIP = ("\t\t*(.text)\n\t\t*(.text.*)\n", "\t\t*(.text)\n")

# The copy, in front of everything else in the reset handler. It uses no stack,
# so it is safe before the stack pointers are even set.
ST_COPY = """/* copy .text.* from ROM to RAM before calling any of it - see
   tools/patch-ramtext.py for why this build runs its code from RAM */
    mov     #_mramtext, r2
    mov     #_ramtext, r1
    mov     #_eramtext, r3
    sub     r1, r3
    cmp     #0, r3
    beq     8f
7:  mov.b   [r2+], r5
    mov.b   r5, [r1+]
    sub     #1, r3
    bne     7b
8:
"""

ST_ANCHOR = "_PowerON_Reset :\n"


def patch(path, anchor, insert, what):
    text = open(path).read()
    if "ramtext" in text:
        print("%s: already patched" % what)
        return True
    if anchor not in text:
        print("%s: anchor not found, check the file" % what)
        return False
    open(path, "w").write(text.replace(anchor, anchor + insert, 1))
    print("%s: patched" % what)
    return True


text = open(LD).read()
if "ramtext" not in text:
    if LD_STRIP[0] not in text:
        print("linker script: could not find *(.text.*) in the .text section")
        sys.exit(1)
    open(LD, "w").write(text.replace(LD_STRIP[0], LD_STRIP[1], 1))
    print("linker script: .text no longer collects *(.text.*)")

ok = patch(LD, LD_ANCHOR, LD_SECTION, "linker script")
ok = patch(ST, ST_ANCHOR, ST_COPY, "start.S") and ok
if not ok:
    sys.exit(1)
print("C code will run from RAM at %s; .text (start.S) stays in ROM" % RAMTEXT_ADDR)
