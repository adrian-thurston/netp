#ifndef _SERVGEN_THREAD_H
#define _SERVGEN_THREAD_H

#include <iostream>
#include <pthread.h>
#include "list.h"

struct Thread
{
	Thread()
	:
		logFile( &std::cerr )
	{
	}

	struct endp {};
	typedef DList<Thread> ThreadList;

	pthread_t pthread;
	std::ostream *logFile;

	Thread *prev, *next;

	ThreadList childList;
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
