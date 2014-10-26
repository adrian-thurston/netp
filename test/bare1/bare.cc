#include "bare.h"

int BareThread::main()
{
	log_message( "bare start" );
	int i = 10;
	while ( i > 0 ) {
		ItHeader *header = control.wait();

		log_message( "bare recv: " << header->msgId );

		control.release( header );
		i -= 1;
	}
	return 0;
}
