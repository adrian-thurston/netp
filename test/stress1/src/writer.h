#ifndef _WRITER_H
#define _WRITER_H

#include "writer_gen.h"
#include <kring/kring.h>

struct WriterThread
	: public WriterGen
{
	struct kring_user kring;

	int main();
};

#endif
