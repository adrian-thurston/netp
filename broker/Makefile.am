GENF = @GENF@

INSTALL_DIRS = $(pkgstatedir)

EXTRA_DIST = broker.gf

bin_PROGRAMS = broker

broker_SOURCES = \
	main.h main.cc itq.h \
	$(BUILT_SOURCES)

broker_LDADD = -lgenf -lpthread -lssl -lcrypto -lparse -lpcap

BUILT_SOURCES = \
	main_gen.h \
	main_gen.cc \
	itq_gen.h \
	itq_gen.cc \
	packet_gen.h \
	packet_gen.cc \
	genf.h

pkgdata_DATA = broker.gf

CLEANFILES = $(BUILT_SOURCES)

main_gen.cc: broker.gf
	$(GENF) $(GENFFLAGS) $<

main_gen.cc: $(GENF)
main_gen.h: main_gen.cc
itq_gen.cc: main_gen.cc
itq_gen.h: main_gen.cc
packet_gen.cc: main_gen.cc
packet_gen.h: main_gen.cc

genf.h: $(top_srcdir)/genf/genf.h.in
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,+x" -e 's/[@]PACKAGE[@]/broker/' $(SED_SUBST)

