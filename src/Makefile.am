GENF = @GENF@
KRING_SYMVERS = @KRING_SYMVERS@

data_DATA = shuttle.ko

EXTRA_DIST = shuttle.gf attribute.c attribute.h

SOURCES = module.c attribute.c attribute.h

module.c: shuttle.gf $(GENF)
	$(GENF) --kernf shuttle.gf

shuttle.ko: $(SOURCES) Module.symvers .kbuild

Module.symvers: $(KRING_SYMVERS)
	cp $(KRING_SYMVERS) ./

.PHONY: .kbuild

.kbuild:
	make CPPFLAGS="@CPPFLAGS@" -C /lib/modules/3.13.0-24-generic/build/ M=`pwd`  

