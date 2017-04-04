#ifndef _WRITER_H
#define _WRITER_H

#include "writer_gen.h"
#include <kring/kring.h>

#define MESSAGES 10
#define WRITERS 4

struct WriterThread
	: public WriterGen
{
	WriterThread( int id )
	:
		writerId(id)
	{}

	int writerId;
	struct kring_user kring;

	int main();
};

#endif
