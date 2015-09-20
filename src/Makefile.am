GENF = @GENF@

data_DATA = filter.ko

EXTRA_DIST = filter.gf attribute.c attribute.h

SOURCES = module.c attribute.c attribute.h

module.c: filter.gf $(GENF)
	$(GENF) --kernf filter.gf

filter.ko: $(SOURCES)
	make -C /lib/modules/3.13.0-37-generic/build/ M=`pwd`  

