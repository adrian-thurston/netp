GENF = @GENF@

lib_LTLIBRARIES = libkring.la

kmod_DATA = kring.ko

libkring_la_SOURCES = libkring.h libkring.c libdata.c libctrl.c

pkgdata_DATA = Module.symvers

kringincdir = $(includedir)/kring
kringinc_HEADERS = kring.h krdep.h krkern.h kdata.h kctrl.h

EXTRA_DIST = kring.gf attribute.c module.h socket.c kring.h kdata.c kdata.h kctrl.c kctrl.h

BUILT_SOURCES = attribute.h module.c

CLEANFILES = *.ko *.o

MOBJS = attribute.o kdata.o kctrl.o socket.o

SOURCES = attribute.c kdata.c kctrl.c socket.c module.c \
	attribute.h kctrl.h kdata.h krdep.h kring.h krkern.h \
	libkring.h module.h

attribute.h: module.c

module.c: kring.gf $(GENF)
	$(GENF) --kernf kring.gf

#Module.symvers: kring.ko
#kring.ko: $(SOURCES)
#	make CPPFLAGS="$(CPPFLAGS)" MOBJS="$(MOBJS)" -C $(KERNDIR) M=`pwd`

kring.ko: $(SOURCES)
	touch $@

Module.symvers: kring.ko
	touch $@

BUILT_SOURCES += include/kring

include/kring:
	mkdir -p include
	ln -s .. include/kring
