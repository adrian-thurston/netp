GENF = @GENF@

lib_LTLIBRARIES = libkring.la

kmod_DATA = kring.ko

libkring_la_SOURCES = libkring.h libkring.c libdata.c libctrl.c

pkgdata_DATA = Module.symvers

pkginclude_HEADERS = kring.h krdep.h krkern.h kdata.h kctrl.h

EXTRA_DIST = kring.gf attribute.c module.h socket.c kring.h kdata.c kdata.h kctrl.c kctrl.h

BUILT_SOURCES = attribute.h module.c

MOBJS = attribute.o kdata.o kctrl.o socket.o

Module.symvers: kring.ko

attribute.h: module.c

module.c: kring.gf $(GENF)
	$(GENF) --kernf kring.gf

kring.ko: .kbuild

.PHONY: .kbuild

.kbuild:
	make CPPFLAGS="$(CPPFLAGS)" MOBJS="$(MOBJS)" -C $(KERNDIR) M=`pwd`  

BUILT_SOURCES += include/kring
    
include/kring:
	mkdir -p include
	ln -s .. include/kring
