SERVGEN = ../../src/servgen

bin_PROGRAMS = bare1

bare1_SOURCES = main_gen.h main_gen.cc

main_gen.cc: $(SERVGEN)
	$(SERVGEN)

main_gen.h: main_gen.cc
