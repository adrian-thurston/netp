#ifndef _WRITER_H
#define _WRITER_H

#include "writer_gen.h"
#include <kring/kring.h>

#define MESSAGES 100
#define WRITERS 8

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
