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

#include <openssl/bio.h>
#include <openssl/ssl.h>

#include <ares.h>
#include <arpa/nameser.h>

#define IT_BLOCK_SZ 4098

/* TLS */
#define EC_SOCKET_CONNECT_FAILED        104
#define EC_SSL_PEER_FAILED_VERIFY       100
#define EC_SSL_CONNECT_FAILED           105
#define EC_SSL_WRONG_HOST               106
#define EC_SSL_CA_CERT_LOAD_FAILURE     107
#define EC_SSL_NEW_CONTEXT_FAILURE      120
#define EC_SSL_CA_CERTS_NOT_SET         121
#define EC_SSL_ACCEPT_FAILED            152
#define EC_SSL_FAILED_TO_LOAD           159
#define EC_SSL_PARAM_NOT_SET            160
#define EC_SSL_CONTEXT_CREATION_FAILED  161
#define EC_NONBLOCKING_IO_NOT_AVAILABLE 162
#define EC_FCNTL_QUERY_FAILED           163
#define EC_WRITE_ERROR                  164
#define EC_DAEMON_DAEMON_TIMEOUT        165
#define EC_SOCKADDR_ERROR               166
#define EC_SOCK_NOT_LOCAL               167
#define EC_CONF_PARSE_ERROR             168
#define EC_WRITE_ON_NULL_SOCKET_BIO     169

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
	enum State {
		User = 1,
		Lookup,
		Connect,
		PktListen,
		PktData,
		TlsAccept,
		TlsConnect,
		TlsEstablished,
		Closed
	};

	SelectFd( Thread *thread, int fd, void *local )
	:
		state(User),
		thread(thread),
		fd(fd),
		local(local),
		wantRead(false),
		wantWrite(false),
		abortRound(false),
		ssl(0),
		bio(0),
		remoteHost(0),
		tlsWantRead(false),
		tlsWantWrite(false),
		tlsWriteWantsRead(false),
		tlsReadWantsWrite(false)
	{}

	State state;
	Thread *thread;
	int fd;
	void *local;
	bool wantRead;
	bool wantWrite;
	bool abortRound;
	SSL *ssl;
	BIO *bio;
	const char *remoteHost;

	/* If connection is tls, the application should use these. */
	bool tlsWantRead;
	bool tlsWantWrite;

	bool tlsWriteWantsRead;
	bool tlsReadWantsWrite;

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
		pthread_this( 0 ),
		pthread_parent( 0 ),
		tid( 0 ),
		recvRequiresSignal( false ),
		pendingNotifSignal( false ),
		logFile( &std::cerr ),
		selectTimeout( 0 ),
		loop( true )
	{
	}

	Thread()
	{
	}

	const char *type;
	struct endp {};
	typedef List<Thread> ThreadList;

	/* First value is for use by child and is retrieved on startup, second is
	 * for use by parent and is set by pthread_create. */
	pthread_t pthread_this;
	pthread_t pthread_parent;
	pid_t tid;

	ares_channel ac;

	/* Set this true in a thread's constructor if the main loop is not driven
	 * by listening for genf messages. Signals will be sent automatically on
	 * message send. */
	bool recvRequiresSignal;

	/* If a sender tries to signal this thread before it has started up and
	 * properly set it's self paramter then this will be tripped and the thread
	 * will send itself the signal in order to break from any syscal and handle
	 * the message. */
	bool pendingNotifSignal;

	std::ostream *logFile;
	ItQueue control;

	ThreadList childList;

	/* Default timeout for select. This is not a timer, but rather a maximum
	 * time to wait in select call. Unlike a timer it resets with every run
	 * around the loop. Only used if there is no timer. No specific action can
	 * be triggered, just causes a poll of the message queue. Default value of
	 * zero means no timeout. */
	int selectTimeout;

protected:
	bool loop;

public:
	Thread *prev, *next;

	void breakLoop()
		{ loop = false; }
	bool loopContinue()
		{ return loop; }
	void loopBegin()
		{ loop = true; }

	virtual int start() = 0;

	void funnelSigs( sigset_t *set );

	virtual void handleTimer() {};

	const Thread &log_prefix() { return *this; }

	virtual	bool poll() = 0;
	int inetListen( uint16_t port );
	int selectLoop( timeval *timer = 0, bool wantPoll = true );

	int pselectLoop( sigset_t *sigmask, timeval *timer, bool wantPoll );
	int inetConnect( sockaddr_in *sa, bool nonBlocking );
	int inetConnect( const char *host, uint16_t port, bool nonBlocking = false );

	virtual void recvSingle() {}

	void _selectFdReady( SelectFd *selectFd, uint8_t readyField );
	virtual void selectFdReady( SelectFd *selectFd, uint8_t readyField ) {}
	virtual void handleSignal( int sig ) {}

	static const uint8_t READ_READY  = 0x01;
	static const uint8_t WRITE_READY = 0x02;

	SelectFdList selectFdList;

	static pthread_key_t thisKey;

	static Thread *getThis()
		{ return (Thread*) pthread_getspecific( thisKey ); }

	/* Must be called from the thread this struct represents before it is run. */
	void initId();

	typedef long RealmSet;
	static RealmSet enabledRealms;

	int signalLoop( sigset_t *set, struct timeval *timer = 0 );
	virtual void data( SelectFd *fd ) {}
	virtual void writeReady( SelectFd *fd ) {}
	virtual void notifAsyncConnect( SelectFd *fd ) {}
	virtual void notifAccept( SelectFd *fd ) {}

	/*
	 * SSL
	 */
	SSL_CTX *sslClientCtx();
	SSL_CTX *sslServerCtx();
	SSL_CTX *sslServerCtx( const char *key, const char *cert );
	SSL_CTX *sslServerCtx( EVP_PKEY *pkey, X509 *x509 );

	static bool makeNonBlocking( int fd );

	void startSslServer( SSL_CTX *defaultCtx, SelectFd *selectFd );
	void startSslClient( SSL_CTX *clientCtx, SelectFd *selectFd, const char *remoteHost );

	virtual void lookupCallback( SelectFd *fd, int status, int timeouts, unsigned char *abuf, int alen ) {}

	void asyncLookup( SelectFd *fd, const char *host );

	void clientConnect( SelectFd *fd );
	virtual bool sslReadReady( SelectFd *fd ) { return false; }
	int tlsWrite( SelectFd *fd, char *data, int len );
	int tlsRead( SelectFd *fd, void *buf, int len );

	virtual void tlsSelectFdReady( SelectFd *fd, uint8_t readyMask ) {}

	void tlsStartup( const char *randFile = 0 );
	void tlsShutdown();

	virtual void writeRetry( SelectFd *fd ) {}

	void tlsError( RealmSet realm, int e );
	bool prepNextRound( SelectFd *fd, int result );
	void serverAccept( SelectFd *fd );

	virtual void tlsConnectResult( SelectFd *fd, int sslError ) {}
	virtual void tlsAcceptResult( SelectFd *fd, int sslError ) {}

	void asyncResolve( const char *name );
};

extern "C" void *genf_thread_start( void *arg );
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

/* FIXME: There is a gotchas here. The class-specific of log_prefix does not
 * work with static functions. */

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
	do { if ( Thread::enabledRealms & realm ) \
		*genf::lf << log_lock() << "debug: " << log_prefix() << \
		msg << std::endl << log_unlock(); } while(0)

#endif
