
DEVICE	= 32MX270F256B
CC	= /opt/microchip/xc32/v1.42/bin/xc32-gcc -mprocessor=$(DEVICE) -ffreestanding -fno-hosted -nodefaultlibs -membedded-data -Wall -Wno-pointer-sign -O1 -I .
LD	= /opt/microchip/xc32/v1.42/bin/xc32-gcc -mprocessor=$(DEVICE) -mno-float
BIN2HEX	= /opt/microchip/xc32/v1.42/bin/xc32-bin2hex
DATE	= $(shell date '+_%y%m%d_%H%M%S')

HEADER	= basic.h.done lcdtp.h.done tplib.h.done

.SECONDARY:

help:	makefile.done
	@awk '/#[#]/{sub(":[^#]*", "\t\t");print;}' makefile


%.burn:	%.hex %.mdb		## program a device.
	mdb $*.mdb


%.burnp:	%.hex %.mdbp		## program a device with power.
	mdb $*.mdbp


%.mdbp:	makefile.done
	echo 'device pic$(DEVICE)' > $@
	echo 'set AutoSelectMemRanges auto' >> $@
	echo 'set poweroptions.powerenable true' >> $@
	echo 'set voltagevalue 3.3' >> $@
	echo 'hwtool pickit3 -p' >> $@
	echo 'program $*.hex' >> $@
	echo 'quit' >> $@


%.mdb:	makefile.done
	echo 'device pic$(DEVICE)' > $@
	echo 'set AutoSelectMemRanges auto' >> $@
#	echo 'set poweroptions.powerenable true' >> $@
	echo 'set voltagevalue 3.3' >> $@
	echo 'hwtool pickit3 -p' >> $@
	echo 'program $*.hex' >> $@
	echo 'quit' >> $@


%.hex:	%.o tplib.o lcdtp.o		## build
	$(LD) -Wl,-Map=$*.map -o $*.out $^
	$(BIN2HEX) $*.out
	@mkdir -p hist
	cp $@ hist/$@$(DATE)


%.o:	%.c $(HEADER)
	@mkdir -p hist
	cp $< hist/$<$(DATE)
	$(CC) -c $<


%.done:	%
	@mkdir -p hist
	cp $< hist/$<$(DATE)
	touch $@


clean:			## clean
	-rm *.o
	-rm $(TARGET)

