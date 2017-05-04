#include "writer.h"

int WriterThread::main()
{
	int res = kctrl_open( &kring, KCTRL_PLAIN, "stress1", 0, KCTRL_WRITE );
	if ( res < 0 ) {
		log_ERROR( "decrypted data kring open for write failed: " << kctrl_error( &kring, res ) );
		return -1;
	}

	char buf[kctrl_plain_max_data()];

	for ( int i = 0; i < MESSAGES; i++ ) {
		sprintf( buf, "w: %d", i );
		kctrl_write_plain( &kring, buf, strlen(buf) );
		// log_message( "message sent: " << buf );
	}

	log_message( "writer finished, spins: " << kctrl_spins( &kring ) );

	return 0;
}

