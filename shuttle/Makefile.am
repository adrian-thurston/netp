GENF = @GENF@
KRING_SYMVERS = @KRING_SYMVERS@

INSTALL_DIRS = @sysconfdir@

kmod_DATA = shuttle.ko
pkgdata_DATA = Module.symvers

EXTRA_DIST = shuttle.gf attribute.c module.h avl.h avl.c

SOURCES = attribute.h attribute.c module.c avl.h avl.c

BUILT_SOURCES = attribute.h module.c

MOBJS = attribute.o avl.o

attribute.h: module.c
module.c: shuttle.gf $(GENF)
	$(GENF) --kernf shuttle.gf

shuttle.ko: $(SOURCES) Module.symvers .kbuild

Module.symvers: $(KRING_SYMVERS)
	cp $(KRING_SYMVERS) ./

.PHONY: .kbuild

.kbuild:
	make CPPFLAGS="@CPPFLAGS@" MOBJS="$(MOBJS)" -C $(KERNDIR) M=`pwd`  
