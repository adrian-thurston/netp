#include "main.h"
#include "bare.h"
#include "itq_gen.h"

#include <unistd.h>

int MainThread::main()
{
	BareThread *bare = new BareThread;
	create( bare );

	ItWriter *writer = bare->control.registerWriter( this );

	int i = 5;
	while ( i > 0 ) {
		sleep( 1 );

		static long l = 1;

		Hello *hello = Hello::open( writer );
		hello->b = true;
		hello->l = l++;
		hello->s = "hello";
		bare->control.send( writer );

		i -= 1;
	}

	join();

	log_message( "main exiting" );

	return 0;
}
