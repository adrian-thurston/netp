SERVGEN = ../../src/servgen

bin_PROGRAMS = bare1

bare1_SOURCES = main_gen.h main_gen.cc

main_gen.cc: bare1.def
	$(SERVGEN) $<

main_gen.cc: $(SERVGEN)
main_gen.h: main_gen.cc
