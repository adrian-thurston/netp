GENF = @GENF@

lib_LTLIBRARIES = libkring.la

kmod_DATA = kring.ko

libkring_la_SOURCES = lib.c ctllib.c

pkgdata_DATA = Module.symvers

pkginclude_HEADERS = kring.h krkern.h kckern.h kctl.h

EXTRA_DIST = kring.gf attribute.c attribute.h kctl.c kctl.h

SOURCES = module.c attribute.c attribute.h kctl.c kctl.h
MOBJS = attribute.o kctl.o

Module.symvers: kring.ko

module.c: kring.gf $(GENF)
	$(GENF) --kernf kring.gf

kring.ko: $(SOURCES) .kbuild

.PHONY: .kbuild

.kbuild:
	make MOBJS="$(MOBJS)" -C /lib/modules/3.13.0-24-generic/build/ M=`pwd`  

