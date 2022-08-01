
DATE	=	$(shell date '+%y%m%d_%H%M%S')
VER	=	$(shell date '+%y%m%d')

.SECONDARY:

help:	makefile.done
	@awk '/#[#]/{sub(":[^#]*", "\t\t");print;}' makefile


%:	%.c
%:	%.c lcdtp.o tplib.o		## build with x11 and tplib
	mkdir -p hist
	cp $< hist/$<_$(DATE)
	gcc -Wall -O2 -L/usr/X11R6/lib -o $@ $^ -lX11


%.o:	%.c tplib.h basic.h
	mkdir -p hist
	cp $< hist/$<_$(DATE)
	gcc -Wall -O2 -c -o $@ $<


%.done:	%
	mkdir -p hist
	cp $< hist/$<_$(DATE)
	touch $@

