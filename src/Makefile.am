GENF = @GENF@

bin_PROGRAMS = client

kmod_DATA = kring.ko

pkgdata_DATA = Module.symvers

include_HEADERS = kring.h

client_SOURCES = client.c

EXTRA_DIST = kring.gf attribute.c attribute.h

SOURCES = module.c attribute.c attribute.h

Module.symvers: kring.ko

module.c: kring.gf $(GENF)
	$(GENF) --kernf kring.gf

kring.ko: $(SOURCES)
	make -C /lib/modules/3.13.0-24-generic/build/ M=`pwd`  

