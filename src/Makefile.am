bin_PROGRAMS = genf

lib_LIBRARIES = libgenf.a

include_HEADERS =

libgenf_a_SOURCES = list.h thread.h thread.cc
libgenf_a_CPPFLAGS = -Iinclude

genfincdir = $(includedir)/genf

genfinc_HEADERS = list.h thread.h

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

genf_SOURCES =

BUILT_SOURCES = include/genf

include/genf:
	mkdir -p include
	ln -s .. include/genf

genf$(EXEEXT): genf.lm $(LM_DEPS)
	$(COLM) -o genf $<
