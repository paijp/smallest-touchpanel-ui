#!/usr/bin/env python3
"""Prefix every line on stdin with the time it arrived, in seconds since start.

    sudo python3 -u rxflash.py log | python3 -u tools/stamp.py > run.log

The board's own timestamp on each line is nominal - the pass counter times the
loop's intended period - so it cannot say how fast a run really went. This can,
and it is what two wrong conclusions here turned on: counting passes and
dividing by the length of the capture cannot tell a run that went four times
slower from a run that stopped a quarter of the way in. With arrival times the
gap between passes settles it, and it settled that nothing runs slowly - a
healthy pass is 277 ms in every build, and the runs that looked slow had
stopped.

It also separates "every pass takes longer" from "most passes are normal and
something stalls now and then", which is the shape a debugger stealing time
would have. So far there is no sign of that: over 211 gaps, 277 ms was the
maximum as well as the median.
"""
import sys
import time

t0 = time.monotonic()
out = sys.stdout
for line in sys.stdin.buffer:
    out.write("%8.3f %s" % (time.monotonic() - t0,
                            line.decode("latin1", "replace")))
    out.flush()
