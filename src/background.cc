#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "thread.h"

const char *_PIDFILE = 0;
const char *_LOGFILE = 0;

void on_exit(void)
{
	unlink( _PIDFILE );
}

FILE *pidExists()
{
	long pid;
	FILE *pidf = fopen( _PIDFILE, "r" );
	if ( pidf != NULL ) {
		int r = fscanf( pidf, "%ld", &pid );
		if ( r != 1 )
			log_FATAL( "PID file exists but we could not read a pid from it: " << _PIDFILE );
		fclose( pidf );

		/* Check for a running process. */
		char fn[64];
		snprintf( fn, sizeof(fn), "/proc/%ld/comm", pid );
		fn[sizeof(fn)-1] = 0;
		FILE *comm = fopen( fn, "rd" );
		if ( comm == NULL && errno == ENOENT ) {
			/* Dangling pid file and overwrite is desired. */
			log_WARNING( "overwriting a dangling PID file" );
			return fopen( _PIDFILE, "w" );
		}
		else {
			log_FATAL( "PID file exists and points to a running process" );
		}
	}

	log_FATAL( "could not open PID file: " << _PIDFILE << ": " << strerror(EEXIST) );
	return NULL;
}

FILE *openPidFile()
{
	/* NOTE: using a glibc extension here to get an exclusive open. */
	FILE *pidf = fopen( _PIDFILE, "wx" );
	if ( pidf == NULL && errno == EEXIST ) {
		/* If the pid file creation failed because the file exists then
		 * open it up and investigate a bit. User can explicitly
		 * overwrite it. */
		pidf = pidExists();
	}

	if ( pidf == NULL )
		log_FATAL( "pid file " << _PIDFILE << ": " << strerror(errno) );

	/* Setup the pidfile unlink. Used in normal exit as well as adverse
	 * failure. Also passed to child process. */
	atexit( on_exit );

	return pidf;
}

void writePidFile( FILE *pidf )
{
	/* Write the pid file. */
	pid_t pid = getpid();
	fprintf( pidf, "%ld\n", (long)pid );
	fclose( pidf );
}
		
void maybeBackground( bool background, bool usePid )
{
	if ( background && !usePid )
		log_FATAL( "cannot run in the background with no PID file" );

	if ( !background ) {
		if ( usePid ) {
			FILE *pidf = openPidFile();
			writePidFile( pidf );
		}
	}
	else {
		/* Ensure we can open the files we need to open. */
		int rdfd = open( "/dev/null", O_RDONLY );
		if ( rdfd == -1 )
			log_FATAL( "daemonize: could not open /dev/null for stdin replacement: " << strerror(errno) );

		int lgfd = open( _LOGFILE, O_WRONLY | O_APPEND | O_CREAT,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
		if ( lgfd == -1 )
			log_FATAL( "daemonize: could not open log file: " << strerror(errno) );

		FILE *pidf = openPidFile();

		/* Change dirs, but don't close fds. */
		int result = daemon( 0, 1 );
		if ( result == -1 )
			log_FATAL( "damonize: daemon() failed: " << strerror(errno) );

		/* Redirect stdin from /dev/null. */
		dup2( rdfd, 0 );
		::close( rdfd );

		/* Redirect stdout and stderr to the log file. This will capture
		 * the log file since it is writing to stderr. */
		dup2( lgfd, 1 );
		dup2( lgfd, 2 );
		::close( lgfd );

		writePidFile( pidf );
	}
}

