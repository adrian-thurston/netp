#ifndef _SERVGEN_THREAD_H
#define _SERVGEN_THREAD_H

#include <iostream>
#include <pthread.h>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <genf/list.h>
#include <stdint.h>
#include <sys/time.h>

#include "list.h"
#include <vector.h>
#include <rope.h>

#define IT_BLOCK_SZ 4098

struct ItWriter;
struct ItQueue;
struct Thread;

struct ItHeader
{
	unsigned short msgId;
	unsigned short writerId;
	unsigned int length;
	ItHeader *next;
};

struct ItBlock
{
	char *data;
	unsigned int size;

	ItBlock *prev, *next;
};

struct ItWriter
{
	ItWriter();

	Thread *writer;
	Thread *reader;
	ItQueue *queue;
	int id;

	/* Write to the tail block, at tail offset. */
	ItBlock *hblk;
	ItBlock *tblk;

	/* Head and tail offset. */
	int hoff;
	int toff;

	int mlen;

	ItHeader *toSend;
	void *contents;

	ItWriter *prev, *next;
};

typedef List<ItWriter> ItWriterList;
typedef std::vector<ItWriter*> ItWriterVect;

struct ItQueue
{
	ItQueue( int blockSz = IT_BLOCK_SZ );

	void *allocBytes( ItWriter *writer, int size );
	void send( ItWriter *writer, bool sendSignal );

	ItHeader *wait();
	bool poll();
	void release( ItHeader *header );

	pthread_mutex_t mutex;
	pthread_cond_t cond;

	ItHeader *head, *tail;
	int blockSz;

	ItBlock *allocateBlock( int needed );
	void freeBlock( ItBlock *block );

	ItWriter *registerWriter( Thread *writer, Thread *reader );

	/* Free list for blocks. */
	ItBlock *free;

	/* The list of writers in the order of registration. */
	ItWriterList writerList;

	/* A vector for finding writers. This lets us identify the writer in the
	 * message header with only a byte. */
	ItWriterVect writerVect;
};

struct SelectFd
{
	SelectFd( int fd, void *local )
	:
		fd(fd),
		local(local),
		wantRead(false),
		wantWrite(false),
		abortRound(false)
	{}

	int fd;
	void *local;
	bool wantRead;
	bool wantWrite;
	bool abortRound;

	SelectFd *prev, *next;
};

typedef List<SelectFd> SelectFdList;

struct PacketHeader;
struct PacketWriter
{
	PacketWriter( int fd )
		: fd(fd) {}

	int fd;
	PacketHeader *toSend;
	void *content;
	Rope buf;

	char *allocBytes( int nb )
	{
		char *b = buf.append( 0, nb );
		return b;
	}

	int length()
		{ return buf.length(); }

	void reset()
		{ buf.empty(); }
};

struct PacketHeader
{
	int msgId;
	int writerId;
	int length;
};

struct Thread
{
	Thread( const char *type )
	:
		type( type ),
		recvRequiresSignal( false ),
		logFile( &std::cerr ),
		loop( true )
	{
	}

	const char *type;
	struct endp {};
	typedef List<Thread> ThreadList;

	pthread_t pthread;
	pid_t tid;

	void breakLoop()
		{ loop = false; }
	bool loopContinue()
		{ return loop; }
	void loopBegin()
		{ loop = true; }

	/* Set this true in a thread's constructor if the main loop is not driven
	 * by listening for genf messages. Signals will be sent automatically on
	 * message send. */
	bool recvRequiresSignal;

	std::ostream *logFile;
	ItQueue control;

	Thread *prev, *next;

	ThreadList childList;

	virtual int start() = 0;

	virtual void handleTimer() {};

	const Thread &log_prefix() { return *this; }

	virtual	int poll() = 0;
	int inetListen( uint16_t port );
	int selectLoop( timeval *timer = 0, bool wantPoll = true )
		{ return pselectLoop( 0, timer, wantPoll ); }

	int pselectLoop( sigset_t *sigmask, timeval *timer, bool wantPoll );
	int inetConnect( const char *host, uint16_t port );

	virtual void selectFdReady( SelectFd *selectFd, uint8_t readyField ) {}
	virtual void handleSignal( int sig ) {}

	static const uint8_t READ_READY  = 0x01;
	static const uint8_t WRITE_READY = 0x02;

	SelectFdList selectFdList;

	static pthread_key_t thisKey;

	static void initThis()
		{ pthread_key_create( &thisKey, 0 ); }

	static Thread *getThis()
		{ return (Thread*) pthread_getspecific( thisKey ); }

	void setThis()
		{ pthread_setspecific( thisKey, this ); }

	static long enabledRealms;

	int signalLoop( sigset_t *set, struct timeval *timer = 0 );

protected:
	bool loop;
};

void *thread_start_routine( void *arg );
void thread_funnel_handler( int s );

struct log_prefix { };
struct log_time { };

struct fdoutbuf
:
	public std::streambuf
{
	fdoutbuf( int fd )
	:
		fd(fd)
	{
	}

	int_type overflow( int_type c )
	{
		if ( c != EOF ) {
			char z = c;
			if ( write( fd, &z, 1 ) != 1 )
				return EOF;
		}
		return c;
	}

	std::streamsize xsputn( const char* s, std::streamsize num )
	{
		return write(fd,s,num);
	}

	int fd;
};

struct lfdostream
:
	public std::ostream
{
	lfdostream( int fd )
	:
		std::ostream( 0 ),
		buf( fd )
	{
		pthread_mutex_init( &mutex, 0 );
		rdbuf( &buf );
	}

	pthread_mutex_t mutex;
	fdoutbuf buf;
};

struct log_lock {};
struct log_unlock {};

std::ostream &operator <<( std::ostream &out, const Thread::endp & );
std::ostream &operator <<( std::ostream &out, const log_lock & );
std::ostream &operator <<( std::ostream &out, const log_unlock & );
std::ostream &operator <<( std::ostream &out, const log_time & );

std::ostream &operator <<( std::ostream &out, const log_prefix & );
std::ostream &operator <<( std::ostream &out, const Thread &thread );

namespace genf
{
	extern lfdostream *lf;
}

struct log_array
{
	log_array( const char *data, int len )
	:
		data(data), len(len)
	{}

	log_array( const unsigned char *data, int len )
	:
		data((const char*)data),
		len(len)
	{}

	const char *data;
	int len;
};

inline std::ostream &operator <<( std::ostream &out, const log_array &a )
{
	out.write( a.data, a.len );
	return out;
}

/* The log_prefix() expression can reference a struct or a function that
 * returns something used to write a different prefix. The macros don't care.
 * This allows for context-dependent log messages. */

#define log_FATAL( msg ) \
	*genf::lf << log_lock() << "FATAL: " << log_prefix() << \
	msg << std::endl << log_unlock() << Thread::endp()

#define log_ERROR( msg ) \
	*genf::lf << log_lock() << "ERROR: " << log_prefix() << \
	msg << std::endl << log_unlock()
	
#define log_message( msg ) \
	*genf::lf << log_lock() << "message: " << log_prefix() << \
	msg << std::endl << log_unlock()

#define log_warning( msg ) \
	*genf::lf << log_lock() << "warning: " << log_prefix() << \
	msg << std::endl << log_unlock()

#define log_debug( realm, msg ) \
	if ( Thread::enabledRealms & realm ) \
		*genf::lf << log_lock() << "debug: " << log_prefix() << \
		msg << std::endl << log_unlock()

#endif
