#ifndef _SERVGEN_THREAD_H
#define _SERVGEN_THREAD_H

#include <iostream>
#include <pthread.h>

struct Thread
{
	Thread()
	:
		logFile( &std::cerr )
	{
	}

	pthread_t pthread;
	std::ostream *logFile;

	struct endp {};
};

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
