#include "bare.h"
#include "itq_gen.h"

int BareThread::main()
{
	int i = 5;
	while ( i > 0 ) {
		ItHeader *header = control.wait();

		Hello *msg = Hello::read( &control, header );
		log_message( "recv: " << msg->s << " - " << msg->l );

		control.release( header );
		i -= 1;
	}

	log_message( "exiting" );

	return 0;
}
