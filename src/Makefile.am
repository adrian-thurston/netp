bin_PROGRAMS = servgen

LM_DEPS = args.lm out_c.lm main.lm

servgen$(EXEEXT): servgen.lm $(LM_DEPS)
	$(COLM) -o servgen $<
