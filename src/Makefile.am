GENF = @GENF@

lib_LTLIBRARIES = libkring.la

kmod_DATA = kring.ko

libkring_la_SOURCES = libdata.c libctrl.c

pkgdata_DATA = Module.symvers

pkginclude_HEADERS = kring.h krkern.h kdata.h kctrl.h

EXTRA_DIST = kring.gf kdata.c attribute.h kctrl.c kctrl.h

SOURCES = module.c kdata.c attribute.h kctrl.c kctrl.h
MOBJS = kdata.o kctrl.o

Module.symvers: kring.ko

module.c: kring.gf $(GENF)
	$(GENF) --kernf kring.gf

kring.ko: $(SOURCES) .kbuild

.PHONY: .kbuild

.kbuild:
	make MOBJS="$(MOBJS)" -C /lib/modules/3.13.0-24-generic/build/ M=`pwd`  

