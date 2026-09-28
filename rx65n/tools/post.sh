#!/bin/bash
# Let an image run until it stops, then compare flash against it.
#   sudo post.sh <name> [runs]               # reads /tmp/<name>.mot
#
# The question is whether a program that has stopped has damaged its own flash:
# a stray write reaching the flash control unit would erase or rewrite code, and
# nothing else here would notice, while looking exactly like this intermittent
# stop. Entering boot mode power-cycles the target, so the comparison happens
# after the run rather than during it.
#
# On this board it came back clean after every stop, which closes that off.
V="$1"; N="${2:-3}"
cd /home/pi/rxflash || exit 1
for i in 1 2 3 4 5; do
	python3 -u rxflash.py write "/tmp/$V.mot" > "/tmp/$V.write" 2>&1 && break
	sleep 3
done
echo "programmed: $(tail -1 /tmp/$V.write)"
for k in $(seq 1 "$N"); do
	sleep 2
	timeout 12 python3 -u rxflash.py log 2>&1 | python3 -u tools/stamp.py > "/tmp/$V.p$k"
	echo "run $k: $(grep -cE '^ +[0-9]+\.[0-9]+ [0-9]+\.[0-9]+ n=' "/tmp/$V.p$k") pass lines, comparing flash"
	sleep 2
	for i in 1 2 3 4 5; do
		python3 -u rxflash.py verify "/tmp/$V.mot" > "/tmp/$V.v$k" 2>&1 && break
		grep -q DIFFER "/tmp/$V.v$k" && break
		sleep 3
	done
	grep -E "ok$|DIFFER|differ|matches" "/tmp/$V.v$k" | sed 's/^/    /'
done
