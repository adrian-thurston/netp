#include "bare.h"

int BareThread::main()
{
	log_message( "bare start" );
	int i = 10;
	while ( i > 0 ) {
		control.wait();

		log_message( "bare recv" );
		i -= 1;
	}
	return 0;
}
