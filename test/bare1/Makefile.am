SERVGEN = ../../src/servgen

bin_PROGRAMS = bare1

bare1_SOURCES = \
	main.h \
	main.cc \
	main_gen.h \
	main_gen.cc

bare1_LDADD = $(top_srcdir)/src/libservgen.a

BUILT_SOURCES = \
	main_gen.cc \
	main_gen.h

main_gen.cc: bare1.def
	$(SERVGEN) $<

main_gen.cc: $(SERVGEN)
main_gen.h: main_gen.cc
