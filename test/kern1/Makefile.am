data_DATA = ring.ko

SOURCES = module.c
GENF = ../../src/genf

module.c: kern1.gf $(GENF)
	$(GENF) --kernf kern1.gf

ring.ko: module.c
	make -C /lib/modules/3.13.0-37-generic/build/ M=`pwd`  
