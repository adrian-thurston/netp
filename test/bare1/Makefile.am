GENF = ../../src/genf

bin_PROGRAMS = bare1

bare1_SOURCES = \
	main.h \
	main.cc \
	main_gen.h \
	main_gen.cc

bare1_LDADD = $(top_srcdir)/src/libgenf.a

BUILT_SOURCES = \
	main_gen.cc \
	main_gen.h

main_gen.cc: bare1.srd
	$(GENF) $<

main_gen.cc: $(GENF)
main_gen.h: main_gen.cc
