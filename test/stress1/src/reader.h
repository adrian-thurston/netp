#ifndef _READER_H
#define _READER_H

#include "reader_gen.h"

struct ReaderThread
	: public ReaderGen
{
	void recvShutdown( Shutdown * );
	int main();
};

#endif
