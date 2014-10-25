#ifndef _SERVGEN_THREAD_H
#define _SERVGEN_THREAD_H

#include <iostream>
#include <pthread.h>
#include "list.h"

#define IT_BLOCK_SZ 4098

struct ItBlock
{
	ItBlock( int size );

	char data[IT_BLOCK_SZ];

	int size;
	ItBlock *prev, *next;
};

struct ItWriter
{
	ItWriter();

	ItBlock *head;
	ItBlock *tail;
	int nextWrite;

	ItWriter *prev, *next;
};

typedef List<ItWriter> ItWriterList;

struct ItQueue
{
	ItQueue( int blockSz = IT_BLOCK_SZ );

	void wait();
	void notify();

	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int messages;
	int blockSz;

	/* Call when more space is required. */
	void allocateBlock();
	void freeBlock();

	/* Free list for blocks. */
	ItBlock *free;
	ItWriterList writerList;
};

struct Thread
{
	Thread()
	:
		logFile( &std::cerr )
	{
	}

	struct endp {};
	typedef List<Thread> ThreadList;

	pthread_t pthread;
	std::ostream *logFile;
	ItQueue control;

	Thread *prev, *next;

	ThreadList childList;

	virtual int start() = 0;
};

void *thread_start_routine( void *arg );

std::ostream &operator <<( std::ostream &out, const Thread::endp & );

#define log_FATAL(msg) \
	*logFile << "FATAL: " << msg << std::endl << endp()

#define log_ERROR(msg) \
	*logFile << "ERROR: " << msg << std::endl;
	
#define log_message(msg) \
	*logFile << "msg: " << msg << std::endl;

#define log_warning(msg) \
	*logFile << "warning: " << msg << std::endl;

#endif
