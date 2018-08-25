#include <sys/wait.h>
#include <signal.h>
#include "thread.h"

void Thread::funnelSigs( sigset_t *set )
{
	sigaddset( set, SIGHUP );
	sigaddset( set, SIGINT );
	sigaddset( set, SIGQUIT );
	// sigaddset( set, SIGKILL );
	sigaddset( set, SIGTERM );
	sigaddset( set, SIGCHLD );
	sigaddset( set, SIGPIPE );
}


void MainBase::handleSignal( int sig )
{
	switch ( sig ) {
		case SIGHUP: {
			log_message( "received SIGHUP" );
			break;
		}

		case SIGINT:
		case SIGQUIT:
		// case SIGKILL:
		case SIGTERM: {
			log_message( "breaking from signal loop" );
			breakLoop();
			break;
		}
		case SIGCHLD:
			/* Be quiet on a sigchld. */
			log_message( "received sigchld" );
			while ( true ) {
				pid_t pid = waitpid( -1, 0, WNOHANG );
				if ( pid <= 0 )
					break;

				log_message( "reaped child: " << pid );
			}
			break;

		case SIGPIPE:
			/* no special treatment for pipes. */
			break;
		default: {
			log_message( "received sig: " << sig );
			break;
		}
	}
}
		
static void sigusrHandler( int s )
{
}

void MainBase::signalSetup()
{
	signal( SIGUSR1, sigusrHandler );
	signal( SIGUSR2, sigusrHandler );

	sigset_t set;
	sigemptyset( &set );

	funnelSigs( &set );

	sigaddset( &set, SIGUSR1 );
	sigaddset( &set, SIGUSR2 );

	pthread_sigmask( SIG_SETMASK, &set, 0 );
}

int MainBase::signalLoop( struct timeval *timer )
{
	sigset_t set;
	sigfillset( &set );

	sigemptyset( &set );
	funnelSigs( &set );
	sigaddset( &set, SIGUSR1 );
	sigaddset( &set, SIGUSR2 );
	pthread_sigmask( SIG_SETMASK, &set, 0 );

	return Thread::signalLoop( &set, timer );
}

int MainBase::selectLoop( timeval *timer, bool wantPoll )
{
	sigset_t set;
	sigemptyset( &set );

	signal( SIGHUP,  thread_funnel_handler );
	signal( SIGINT,  thread_funnel_handler );
	signal( SIGQUIT, thread_funnel_handler );
	// signal( SIGKILL, thread_funnel_handler );
	signal( SIGTERM, thread_funnel_handler );
	signal( SIGCHLD, thread_funnel_handler );
	signal( SIGPIPE, thread_funnel_handler );

	return pselectLoop( &set, timer, wantPoll );
}
