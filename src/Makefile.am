GENF = @GENF@

bin_PROGRAMS = client

data_DATA = kring.ko

client_SOURCES = client.c

EXTRA_DIST = kring.gf attribute.c attribute.h

SOURCES = module.c attribute.c attribute.h

module.c: kring.gf $(GENF)
	$(GENF) --kernf kring.gf

kring.ko: $(SOURCES)
	make -C /lib/modules/3.13.0-24-generic/build/ M=`pwd`  

