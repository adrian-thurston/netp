#ifndef _MAIN_H
#define _MAIN_H

#include "genf.h"
#include "main_gen.h"

#define PARSE_REPORT 1

struct MainThread
	: public MainGen
{
	int main();
};

#endif
