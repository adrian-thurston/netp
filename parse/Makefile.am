
GENF = @GENF@
RAGEL = @RAGEL@
COLM = @COLM@

noinst_PROGRAMS = parse

lib_LTLIBRARIES = libparse.la

parseincdir = $(includedir)/parse

parseinc_HEADERS = parse.h pattern.h packet.h json.h fmt.h fetch.h module.h

libparse_la_SOURCES = \
	parse.h parse.cc \
	handler.cc \
	udp.cc tcp.cc decrypted.cc \
	gzip.cc blockexec.cc \
	brotli.cc report.cc node.cc \
	connection.cc module.cc \
	$(libparse_la_BUILT_SOURCES)

libparse_la_BUILT_SOURCES = \
	dns.cc httpreq.cc httprsp.cc \
	ident.cc html.cc json.cc jsontree.cc \
	itq_gen.h \
	itq_gen.cc \
	packet_gen.h \
	packet_gen.cc \
	genf.h

BUILT_SOURCES = $(libparse_la_BUILT_SOURCES)

libparse_la_LIBADD = ../genf/libgenf.la 

parse_BUILT_SOURCES = main_gen.cc

parse_SOURCES = main.cc $(parse_BUILT_SOURCES)
parse_LDADD = libparse.la ../genf/libgenf.la -lpcap

pkgdata_DATA = parse.gf

CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = parse.gf ident.rl dns.lm dns.rl httpreq.rl httprsp.rl html.rl

ident.cc: ident.rl
	$(RAGEL) -o $@ $<

httpreq.cc: httpreq.rl
	$(RAGEL) -o $@ $<

httprsp.cc: httprsp.rl
	$(RAGEL) -o $@ $<

dns.cc: dns.rl
	$(RAGEL) -o $@ $<

json.cc: json.rl
	$(RAGEL) -o $@ $<

jsontree.cc: jsontree.rl
	$(RAGEL) -o $@ $<

html.cc: html.rl
	$(RAGEL) -o $@ $<

main_gen.cc: parse.gf
	$(GENF) $(GENFFLAGS) $<

main_gen.cc: $(GENF)
main_gen.h: main_gen.cc
itq_gen.h: main_gen.cc
itq_gen.cc: main_gen.cc
packet_gen.h: main_gen.cc
packet_gen.cc: main_gen.cc

genf.h: $(top_srcdir)/genf/genf.h.in
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,-x" $(SED_SUBST)

BUILT_SOURCES += include/parse

include/parse:
	mkdir -p include
	ln -s .. include/parse
