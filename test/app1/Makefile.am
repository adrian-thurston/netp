GENF = ../../src/genf

bin_PROGRAMS = app1

app1_SOURCES = \
	main.h \
	main.cc \
	user.h \
	user.cc \
	$(BUILT_SOURCES)

app1_LDADD = $(top_srcdir)/src/libgenf.a

BUILT_SOURCES = \
	main_gen.h \
	main_gen.cc \
	user_gen.h \
	user_gen.cc \
	itq_gen.h \
	itq_gen.cc

main_gen.cc: app1.gf
	$(GENF) $<

main_gen.cc: $(GENF)
main_gen.h: main_gen.cc
