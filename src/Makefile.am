bin_PROGRAMS = genf

lib_LIBRARIES = libgenf.a

libgenf_a_SOURCES = thread.cc

LM_DEPS = grammar.lm args.lm out_c.lm main.lm

genf$(EXEEXT): genf.lm $(LM_DEPS)
	$(COLM) -o genf $<
