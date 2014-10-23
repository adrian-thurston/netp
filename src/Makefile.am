bin_PROGRAMS = servgen

lib_LIBRARIES = libservgen.a

libservgen_a_SOURCES = thread.cc

LM_DEPS = grammar.lm args.lm out_c.lm main.lm

servgen$(EXEEXT): servgen.lm $(LM_DEPS)
	$(COLM) -o servgen $<
