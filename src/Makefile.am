data_DATA = filter.ko

SOURCES = module.c attribute.c attribute.h
GENF = $(HOME)/devel/genf/src/genf

module.c: filter.gf $(GENF)
	$(GENF) --kernf filter.gf

filter.ko: $(SOURCES)
	make -C /lib/modules/3.13.0-37-generic/build/ M=`pwd`  

