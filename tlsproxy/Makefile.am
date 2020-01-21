GENF = @GENF@
GENFFLAGS = @GENFFLAGS@

EXTRA_DIST = \
	tlsproxy.gf \
	ca.sh \
	cert.sh \
	openssl.conf.in \
	self-signed.crt \
	self-signed.key

bin_PROGRAMS = tlsproxy

pkglibexec_SCRIPTS = ca cert

pkgdata_DATA = openssl.conf self-signed.crt self-signed.key

tlsproxy_SOURCES = \
	main.h main.cc \
	listen.h listen.cc \
	service.h service.cc \
	proxy.h proxy.cc \
	crypto.cc itq.h \
	$(BUILT_SOURCES)

tlsproxy_LDADD = -lparse -lgenf -lgenf -lpthread -lssl -lcrypto -lpcap -lkring

BUILT_SOURCES = \
	main_gen.h main_gen.cc \
	itq_gen.h itq_gen.cc \
	packet_gen.h packet_gen.cc \
	listen_gen.h listen_gen.cc \
	service_gen.h service_gen.cc \
	proxy_gen.h proxy_gen.cc

CLEANFILES = $(BUILT_SOURCES)

main_gen.cc: tlsproxy.gf
	$(GENF) $(GENFFLAGS) $<

main_gen.cc: $(GENF)
main_gen.h: main_gen.cc
itq_gen.cc: main_gen.cc
itq_gen.h: main_gen.cc
service_gen.h: main_gen.cc
service_gen.cc: main_gen.cc
listen_gen.h: main_gen.cc
listen_gen.cc: main_gen.cc
proxy_gen.h: main_gen.cc
proxy_gen.cc: main_gen.cc
packet_gen.h: main_gen.cc
packet_gen.cc: main_gen.cc

INSTALL_DIRS = $(pkgstatedir)

BUILT_SOURCES += genf.h

genf.h: $(top_srcdir)/genf/genf.h.in
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,+x" -e 's/[@]PACKAGE[@]/tlsproxy/' $(SED_SUBST)

SUFFIXES = .in .sh

.in:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w" $(SED_SUBST)
.sh:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,+x" $(SED_SUBST)

