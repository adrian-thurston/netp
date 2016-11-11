GENF = @SUBJECT@

bin_PROGRAMS = app1

app1_SOURCES = \
	main.h \
	main.cc \
	user.h \
	user.cc \
	$(BUILT_SOURCES)

app1_LDADD = -lgenf -lpthread -lcrypto -lssl -lcares

BUILT_SOURCES = \
	main_gen.h \
	main_gen.cc \
	user_gen.h \
	user_gen.cc \
	itq_gen.h \
	itq_gen.cc

CLEANFILES = \
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
