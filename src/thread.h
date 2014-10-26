#ifndef _SERVGEN_THREAD_H
#define _SERVGEN_THREAD_H

#include <iostream>
#include <pthread.h>
#include <vector>
#include "list.h"

#define IT_BLOCK_SZ 4098

struct ItWriter;
struct ItQueue;
struct Thread;

struct ItHeader
{
	unsigned short msgId;
	unsigned char writerId;
	unsigned int length;
	ItHeader *next;
};

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

	Thread *thread;
	ItQueue *queue;
	int id;

	/* Write to the tail block, at tail offset. */
	ItBlock *head;
	ItBlock *tail;

	/* Head and tail offset. */
	int hoff;
	int toff;

	ItHeader *toSend;

	ItWriter *prev, *next;
};

typedef List<ItWriter> ItWriterList;
typedef std::vector<ItWriter*> ItWriterVect;

struct ItQueue
{
	ItQueue( int blockSz = IT_BLOCK_SZ );

	void *allocBytes( ItWriter *writer, int size );
	void send( ItHeader *header );

	ItHeader *wait();
	void release( ItHeader *header );

	pthread_mutex_t mutex;
	pthread_cond_t cond;

	ItHeader *head, *tail;
	int blockSz;

	ItBlock *allocateBlock();
	void freeBlock( ItBlock *block );

	ItWriter *registerWriter( Thread *writer );

	/* Free list for blocks. */
	ItBlock *free;

	/* The list of writers in the order of registration. */
	ItWriterList writerList;

	/* A vector for finding writers. This lets us identify the writer in the
	 * message header with only a byte. */
	ItWriterVect writerVect;
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
