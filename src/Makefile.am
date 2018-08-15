bin_PROGRAMS = genf
bin_SCRIPTS = autogen.sh

lib_LTLIBRARIES = libgenf.la

include_HEADERS =

libgenf_la_SOURCES = \
	list.h \
	thread.h \
	thread.cc \
	signal.cc \
	background.cc \
	itq.cc \
	tls.cc \
	connection.cc \
	packet.cc \
	select.cc \
	lookup.cc \
	inet.cc

libgenf_la_CPPFLAGS = -Iinclude

genfincdir = $(includedir)/genf

genfinc_HEADERS = list.h thread.h packet.h RecordBase.java

pkgdata_DATA = \
	with-aapl \
	common.m4 \
	genf.mk \
	genf.h.in \
	sedsubst \
	init.d.sh \
	bootstrap.sh

with-aapl:
	echo $(WITH_AAPL) > with-aapl

SUFFIXES = .pl .sh .in

.in.sh:
	@$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,+x" $(SED_SUBST)

LM_DEPS = \
	grammar.lm \
	args.lm \
	out_c.lm \
	main.lm \
	analysis.lm \
	thread.lm \
	itq.lm \
	record.lm \
	java.lm \
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
