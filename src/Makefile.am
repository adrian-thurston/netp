bin_PROGRAMS = genf kernf

lib_LIBRARIES = libgenf.a

libgenf_a_SOURCES = thread.cc

LM_DEPS = \
	grammar.lm \
	args.lm \
	out_c.lm \
	main.lm \
	semantic.lm \
	thread.lm \
	itq.lm

genf$(EXEEXT): genf.lm $(LM_DEPS)
	$(COLM) -o genf $<

kernf$(EXEEXT): kernf.lm
	$(COLM) -o kernf $<
