#include <sys/types.h>
#include <sys/wait.h>

#include "thread.h"

int Thread::createProcess( Process *proc )
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
		execl( "/bin/sh", "sh", "-c", proc->cmdline, NULL );
		perror( "execl" );
		exit( 99 );
	}

	/* Parent. Close child ends. */
	close( pipe_stdin[0] );
	close( pipe_stdout[1] );

	proc->pid = p;
	proc->to.fd = pipe_stdin[1];
	proc->from.fd = pipe_stdout[0];

	proc->to.type = SelectFd::Process;
	proc->from.type = SelectFd::Process;

	proc->to.local = proc;
	proc->from.local = proc;

	selectFdList.append( &proc->to );
	selectFdList.append( &proc->from );

	return 0; 
}
