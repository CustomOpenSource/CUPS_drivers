VPATH = src:ppd:bin

DRIVER_NAME=KPM180H

ppds = ${DRIVER_NAME}.ppd.gz
ppdsalt = ${DRIVER_NAME}CUT.ppd.gz

DEFS=
LIBS=-lcupsimage -lcups

ifdef RPMBUILD
DEFS=-DRPMBUILD
LIBS=-ldl
endif

define init
@if [ ! -e bin ]; then echo "mkdir bin"; mkdir bin; fi
endef

define sweep
@if [ -e bin ]; then echo "rm -f bin/*"; rm -f bin/*; rmdir bin; fi
endef


rasterto${DRIVER_NAME}: rasterToPrn.c
	$(dependencies)
	$(init)
	# compiling rasterToPrn filter
	gcc -Wl,-rpath,/usr/lib -Wall -fPIC -O2 $(DEFS) -o bin/rasterto${DRIVER_NAME} src/rasterToPrn.c src/compression.c src/dithering.c $(LIBS)


$(ppds): %.ppd.gz: %.ppd
	# gzip ppd file
	gzip -c ppd/${DRIVER_NAME}.ppd >> bin/$@

$(ppdsalt): %.ppd.gz: %.ppd
	# gzip ppd file
	gzip -c ppd/${DRIVER_NAME}CUT.ppd >> bin/$@

.PHONY: clean
clean:
	# cleaning
	$(sweep)

