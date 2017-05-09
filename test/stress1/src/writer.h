#ifndef _WRITER_H
#define _WRITER_H

#include "writer_gen.h"
#include <kring/kring.h>

#define MESSAGES (1024 * 64)
#define WRITERS 6

struct WriterThread
	: public WriterGen
{
	WriterThread( int id )
	:
		writerId(id)
	{}

	int writerId;
	struct kctrl_user kring;

	int main();
};

#endif
