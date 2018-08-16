#include <sys/types.h>
#include <sys/wait.h>

#include "thread.h"

int Process::create( const char *cmdline )
{
	pid_t p;
	int pipe_stdin[2], pipe_stdout[2];

	if ( pipe( pipe_stdin ) )
		return -1;

	if ( pipe( pipe_stdout ) ) {
		close( pipe_stdin[0] );
		close( pipe_stdin[1] );
		return -1;
	}

	p = fork();
	if ( p < 0 ) {
		/* Fork failed */
		close( pipe_stdin[0] );
		close( pipe_stdin[1] );
		close( pipe_stdout[0] );
		close( pipe_stdout[1] );
		return p;
	}

	if ( p == 0 ) {
		/* child */
		close( pipe_stdin[1] );
		dup2( pipe_stdin[0], 0 );
		close( pipe_stdout[0] );
		dup2( pipe_stdout[1], 1 );
		execl( "/bin/sh", "sh", "-c", cmdline, NULL );
		perror( "execl" );
		exit( 99 );
	}

	/* Parent. Close child ends. */
	close( pipe_stdin[0] );
	close( pipe_stdout[1] );

	this->pid = p;
	this->to = fdopen( pipe_stdin[1], "w" );
	this->from = fdopen( pipe_stdout[0], "r" );

	return 0; 
}

int Process::wait()
{
	return waitpid( pid, NULL, 0 );
}


