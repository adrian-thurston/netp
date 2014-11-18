bin_PROGRAMS = genf

lib_LIBRARIES = libgenf.a

libgenf_a_SOURCES = list.h thread.h thread.cc

LM_DEPS = \
	grammar.lm \
	args.lm \
	out_c.lm \
	main.lm \
	semantic.lm \
	thread.lm \
	itq.lm \
	kernf.lm \
	genf.lm

EXTRA_DIST = $(LM_DEPS)

genf$(EXEEXT): genf.lm $(LM_DEPS)
	$(COLM) -o genf $<
