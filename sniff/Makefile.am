GENF = @GENF@
RAGEL = @RAGEL@
COLM = @COLM@

bin_PROGRAMS = sniff

sniff_SOURCES = \
	main.h sniff.h service.h fmt.h packet.h \
	main.cc sniff.cc service.cc itq.h \
	$(sniff_BUILT_SOURCES)

sniff_BUILT_SOURCES = \
	main_gen.h \
	main_gen.cc \
	sniff_gen.h \
	sniff_gen.cc \
	service_gen.h \
	service_gen.cc \
	itq_gen.h \
	itq_gen.cc \
	packet_gen.h \
	packet_gen.cc

BUILT_SOURCES = $(sniff_BUILT_SOURCES) genf.h

sniff_LDADD = -lparse -lgenf -lpthread -lssl -lcrypto -lpcap -lkring

pkgdata_DATA = sniff.gf

CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = sniff.gf ident.rl dns.lm dns.rl httpreq.rl httprsp.rl html.rl

main_gen.cc: sniff.gf
	$(GENF) $(GENFFLAGS) $<

main_gen.cc: $(GENF)
main_gen.h: main_gen.cc
sniff_gen.h: main_gen.cc
sniff_gen.cc: main_gen.cc
itq_gen.cc: main_gen.cc
itq_gen.h: main_gen.cc
service_gen.h: main_gen.cc
service_gen.cc: main_gen.cc
packet_gen.h: main_gen.cc
packet_gen.cc: main_gen.cc

genf.h: $(top_srcdir)/genf/genf.h.in
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,+x" -e 's/[@]PACKAGE[@]/sniff/' $(SED_SUBST)

