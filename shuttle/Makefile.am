GENF = @GENF@
KRING_SYMVERS = @KRING_SYMVERS@

INSTALL_DIRS = @sysconfdir@

kmod_DATA = shuttle.ko
pkgdata_DATA = Module.symvers

EXTRA_DIST = shuttle.gf attribute.c module.h avl.h avl.c

BUILT_SOURCES = attribute.h module.c

CLEANFILES = *.ko *.o

MOBJS = attribute.o avl.o
SOURCES = attribute.c avl.c module.c \
	attribute.h avl.h module.h
DEPS = ../kring/krkern.h

attribute.h: module.c
module.c: shuttle.gf $(GENF)
	$(GENF) --kernf shuttle.gf

shuttle.ko: Module.symvers $(SOURCES) $(DEPS)
	make CPPFLAGS="@CPPFLAGS@" MOBJS="$(MOBJS)" -C $(KERNDIR) M=`pwd`  

Module.symvers: $(KRING_SYMVERS)
	cp $(KRING_SYMVERS) ./
