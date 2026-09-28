#!/bin/bash
# Compare builds by alternating them inside one session.
#   sudo interleave.sh 8 12 A0 A2     # 8 rounds of 12s, A0 then A2 each round
#
# Running one build's captures and then the other's is not a comparison. The same
# image measured in two sessions here gave 6 of 16 and 13 of 16, so the rate
# drifts with something other than the image - which quietly invalidated the
# cleanest result this investigation had. Alternating puts both builds through
# whatever is drifting, at the cost of reprogramming between every capture.
ROUNDS="${1:-8}"; S="${2:-12}"; shift 2
cd /home/pi/rxflash || exit 1
declare -A ok
for v in "$@"; do ok[$v]=0; done
for r in $(seq 1 "$ROUNDS"); do
	for v in "$@"; do
		for i in 1 2 3 4 5; do
			python3 -u rxflash.py write "/tmp/$v.mot" > /tmp/il.write 2>&1 && break
			sleep 3
		done
		sleep 2
		timeout "$S" python3 -u rxflash.py log 2>&1 \
			| python3 -u tools/stamp.py > "/tmp/$v.i$r"
		read -r verdict rest <<< "$(python3 - "/tmp/$v.i$r" "$S" <<'PY'
import re, sys
d = open(sys.argv[1], "rb").read()
secs = float(sys.argv[2])
ts = [float(l[:8]) for l in d.split(b"\n") if re.search(rb" n=\d+", l)]
if not ts:
	print("dead nothing after the banner")
	raise SystemExit
run = 1
while run < len(ts) and ts[run] - ts[run - 1] < 1.0:
	run += 1
if run == len(ts) and ts[-1] > secs - 1.0:
	print("alive %d passes" % len(ts))
else:
	print("stopped after %.1fs (%d passes)" % (ts[run - 1], run))
PY
)"
		# Any letter patch-inthandler.py emits says which exception fired, and
		# a flood of one says it is repeating - which is worth more than the
		# verdict, so it goes on the line.
		letters=$(tail -c 2000 "/tmp/$v.i$r" | tr -dc 'SAUFBRNK' | head -c 12)
		printf "  round %2d %-6s %-28s %s\n" "$r" "$v" "$verdict $rest" \
			"${letters:+exceptions: $letters}"
		[ "$verdict" = alive ] && ok[$v]=$(( ${ok[$v]} + 1 ))
		sleep 2
	done
done
for v in "$@"; do echo "$v: survived ${ok[$v]} of $ROUNDS"; done
