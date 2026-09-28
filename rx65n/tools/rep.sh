#!/bin/bash
# Repeat one image many times and report how long each run lasted.
#   sudo rep.sh <name> <runs> [seconds]      # reads /tmp/<name>.mot
#
# The fault this port is chasing is intermittent, so a single capture says
# nothing and a survival rate is the only thing a change can be judged against.
# It was this that separated the build conditions: all sources at -O0 survived
# 8 of 8 runs, while every build with any file at -O2 survived 0 to 2 of 8.
#
# The image is programmed once and only reset between runs, which is most of the
# time saved.
#
# How a stop is recognised: a healthy pass arrives every 277 ms without
# exception - over 211 consecutive gaps that was the maximum as well as the
# median - so any gap over a second is the program stopping, and the time of the
# last pass before it is how long the run lasted. Two things that look like
# stops and are not, both of which cost a wrong conclusion here: the pass count
# alone cannot tell a slow run from a short one, and the final line always
# arrives as the capture is torn down, because killing the reader flushes
# whatever the probe still held. So neither the pass count nor a log that ends
# mid-line means anything; the gaps do.
V="$1"; N="${2:-10}"; S="${3:-12}"
cd /home/pi/rxflash || exit 1
for i in 1 2 3 4 5; do
	python3 -u rxflash.py write "/tmp/$V.mot" > "/tmp/$V.write" 2>&1 && break
	sleep 3	# the probe's interface stays claimed for a moment after a process exits
done
tail -1 "/tmp/$V.write"
ok=0
for k in $(seq 1 "$N"); do
	sleep 2
	timeout "$S" python3 -u rxflash.py log 2>&1 | python3 -u tools/stamp.py > "/tmp/$V.r$k"
	read -r verdict rest <<< "$(python3 - "/tmp/$V.r$k" "$S" <<'PY'
import re, sys
d = open(sys.argv[1], "rb").read()
secs = float(sys.argv[2])
ts = [float(l[:8]) for l in d.split(b"\n")
      if re.match(rb"\s*\d+\.\d+ \d+\.\d+ n=", l)]
if not ts:
	print("dead 0 passes, nothing after the banner")
	raise SystemExit
run = 1
while run < len(ts) and ts[run] - ts[run - 1] < 1.0:
	run += 1
if run == len(ts) and ts[-1] > secs - 1.0:
	print("alive %d passes, no gap over 1s" % len(ts))
else:
	print("stopped %d passes, ran %.1fs, then a %.1fs gap" % (
		run, ts[run - 1],
		(ts[run] - ts[run - 1]) if run < len(ts) else secs - ts[-1]))
PY
)"
	printf "  run %2d: %-8s %s\n" "$k" "$verdict" "$rest"
	[ "$verdict" = alive ] && ok=$((ok + 1))
done
echo "$V: survived $ok of $N runs of ${S}s"
