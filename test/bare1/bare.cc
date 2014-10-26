#include "bare.h"
#include "itq_gen.h"

int BareThread::main()
{
	int i = 10;
	while ( i > 0 ) {
		ItHeader *header = control.wait();

		Hello *msg = Hello::read( &control, header );
		log_message( "bare recv: " << msg->s << " - " << msg->l );

		control.release( header );
		i -= 1;
	}
	return 0;
}
