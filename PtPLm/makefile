VPATH = src:ppd:bin:PtPLm


INSTALL = out

define init
@if [ ! -e $(INSTALL) ]; then mkdir $(INSTALL); fi
@if [ ! -e $(INSTALL)/include ]; then mkdir $(INSTALL)/include; fi
endef

define sweep
@if [ -e $(INSTALL) ]; then echo "rm -fr $(INSTALL)/*"; rm -fr $(INSTALL)/*; rmdir $(INSTALL); fi
endef

$(INSTALL)/setup: libPtPLm.so
	# packaging
	cp -f src/PtPLm.h $(INSTALL)/include/
	cp -f src/ComErrors.h $(INSTALL)/include/


libPtPLm.so: PtPLm.c  PtPLm.h  ComDefines.h  ComErrors.h
	$(dependencies)
	$(init)
	# compiling pageToPage support library
	gcc -W -Wall -fPIC -c src/PtPLm.c -o src/PtPLm.o
	gcc -W -Wall -shared -Wl,-soname,libPtPLm.so -o $(INSTALL)/libPtPLm.so src/PtPLm.o -lc
	rm -f src/PtPLm.o

.PHONY: clean
clean:
	# cleaning
	$(sweep)

