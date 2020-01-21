GENF = @GENF@

noinst_PROGRAMS = describe1

describe1_SOURCES = \
	$(BUILT_SOURCES) \
	main.h main.cc itq.h

describe1_LDADD = \
	-lparse -lgenf -lpthread -lssl \
	-lcrypto -lpcap -lkring -ldl

BUILT_SOURCES = \
	itq_gen.h \
	itq_gen.cc \
	main_gen.h \
	main_gen.cc \
	packet_gen.h \
	packet_gen.cc

CLEANFILES = $(BUILT_SOURCES)

main_gen.cc: describe1.gf
	$(GENF) -I$(abs_top_srcdir)/broker $(GENFFLAGS) $<

main_gen.cc: $(GENF)
main_gen.h: main_gen.cc
itq_gen.h: main_gen.cc
itq_gen.cc: main_gen.cc
packet_gen.h: main_gen.cc
packet_gen.cc: main_gen.cc

BUILT_SOURCES += genf.h

genf.h: $(top_srcdir)/genf/genf.h.in
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,+x" -e 's/[@]PACKAGE[@]/describe1/' $(SED_SUBST)
