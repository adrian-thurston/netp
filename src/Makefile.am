bin_PROGRAMS = genf

lib_LTLIBRARIES = libgenf.la

include_HEADERS =

libgenf_la_SOURCES = \
	list.h \
	thread.h \
	thread.cc \
	tls.cc \
	connection.cc \
	packet.cc

libgenf_la_CPPFLAGS = -Iinclude

genfincdir = $(includedir)/genf

genfinc_HEADERS = list.h thread.h packet.h

pkgdata_DATA = with-aapl

with-aapl:
	echo $(WITH_AAPL) > with-aapl

LM_DEPS = \
	grammar.lm \
	args.lm \
	out_c.lm \
	main.lm \
	analysis.lm \
	thread.lm \
	itq.lm \
	packet.lm \
	kernf.lm \
	genf.lm

EXTRA_DIST = $(LM_DEPS)

genf_SOURCES =

BUILT_SOURCES = include/genf

include/genf:
	mkdir -p include
	ln -s .. include/genf

genf$(EXEEXT): genf.lm $(LM_DEPS)
	$(COLM) -o genf $<
