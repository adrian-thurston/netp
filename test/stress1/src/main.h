#ifndef _MAIN_H
#define _MAIN_H

#include "main_gen.h"

struct MainThread
	: public MainGen
{
	MainThread();
	int entered;
	void recvEntered( Entered * );
	int main();
};

#endif
