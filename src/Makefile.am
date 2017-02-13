GENF = @GENF@

bin_PROGRAMS = client

lib_LTLIBRARIES = libkring.la

kmod_DATA = kring.ko

libkring_la_SOURCES = lib.c

pkgdata_DATA = Module.symvers

pkginclude_HEADERS = kring.h krkern.h

client_SOURCES = client.c

EXTRA_DIST = kring.gf attribute.c attribute.h

SOURCES = module.c attribute.c attribute.h

Module.symvers: kring.ko

module.c: kring.gf $(GENF)
	$(GENF) --kernf kring.gf

kring.ko: $(SOURCES) .kbuild

.PHONY: .kbuild

.kbuild:
	make -C /lib/modules/3.13.0-24-generic/build/ M=`pwd`  

