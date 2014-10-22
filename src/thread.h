#ifndef _SERVGEN_THREAD_H
#define _SERVGEN_THREAD_H

#include <iostream>
#include <stdlib.h>

struct Thread
{
	Thread()
		: logFile( &std::cerr ) {}

	std::ostream *logFile;

	struct endp {};
};


inline std::ostream &operator <<( std::ostream &out, const Thread::endp & )
{
	exit( 1 );
}

#define log_FATAL(msg) \
	*logFile << "FATAL: " << msg << std::endl << endp()

#endif
