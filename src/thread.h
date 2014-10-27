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

	unsigned int size;
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

	int mlen;

	ItHeader *toSend;

	ItWriter *prev, *next;
};

typedef List<ItWriter> ItWriterList;
typedef std::vector<ItWriter*> ItWriterVect;

struct ItQueue
{
	ItQueue( int blockSz = IT_BLOCK_SZ );

	void *allocBytes( ItWriter *writer, int size );
	void send( ItWriter *writer );

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
	Thread( const char *type )
	:
		type( type ),
		logFile( &std::cerr )
	{
	}

	const char *type;
	struct endp {};
	typedef List<Thread> ThreadList;

	pthread_t pthread;
	std::ostream *logFile;
	ItQueue control;

	Thread *prev, *next;

	ThreadList childList;

	virtual int start() = 0;

	const Thread &log_prefix() { return *this; }
};

void *thread_start_routine( void *arg );

struct log_prefix { };
struct log_time { };

std::ostream &operator <<( std::ostream &out, const Thread::endp & );
std::ostream &operator <<( std::ostream &out, const log_prefix & );
std::ostream &operator <<( std::ostream &out, const log_time & );
std::ostream &operator <<( std::ostream &out, const Thread &thread );

/* The log_prefix() expression can reference a struct or a function that
 * returns something used to write a different prefix. The macros don't care.
 * This allows for context-dependent log messages. */

#define log_FATAL( msg ) \
	*logFile << "FATAL: " << log_prefix() << msg << std::endl << endp()

#define log_ERROR( msg ) \
	*logFile << "ERROR: " << log_prefix() << msg << std::endl
	
#define log_message( msg ) \
	*logFile << "message: " << log_prefix() << msg << std::endl

#define log_warning( msg ) \
	*logFile << "warning: " << log_prefix() << msg << std::endl

#endif
