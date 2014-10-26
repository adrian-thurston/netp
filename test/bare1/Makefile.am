GENF = ../../src/genf

bin_PROGRAMS = bare1

bare1_SOURCES = \
	main.h \
	main.cc \
	bare.h \
	bare.cc \
	$(BUILT_SOURCES)

bare1_LDADD = $(top_srcdir)/src/libgenf.a

BUILT_SOURCES = \
	main_gen.h \
	main_gen.cc \
	bare_gen.h \
	bare_gen.cc \
	itq_gen.h \
	itq_gen.cc

main_gen.cc: bare1.gf
	$(GENF) $<

main_gen.cc: $(GENF)
main_gen.h: main_gen.cc
