GENF = @GENF@

data_DATA = shuttle.ko

EXTRA_DIST = shuttle.gf attribute.c attribute.h

SOURCES = module.c attribute.c attribute.h

module.c: shuttle.gf $(GENF)
	$(GENF) --kernf shuttle.gf

shuttle.ko: $(SOURCES)
	make -C /lib/modules/3.13.0-24-generic/build/ M=`pwd`  

