GENF = @GENF@

bin_PROGRAMS = stress1

stress1_SOURCES = \
	main.h main.cc \
	reader.h reader.cc \
	writer.h writer.cc \
	$(BUILT_SOURCES)

BUILT_SOURCES = \
	main_gen.h main_gen.cc \
	reader_gen.h reader_gen.cc \
	writer_gen.h writer_gen.cc \
	itq_gen.h itq_gen.cc \
	packet_gen.h packet_gen.cc

SUFFIXES = .in .sh

.in:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w" $(SED_SUBST)

.sh:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,+x" $(SED_SUBST)

CLEANFILES = $(BUILT_SOURCES)

main_gen.cc: stress1.gf
	$(GENF) $<

main_gen.cc: $(GENF)
main_gen.h: main_gen.cc

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(pkgstatedir)

