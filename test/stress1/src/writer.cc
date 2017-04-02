#include "writer.h"

int WriterThread::main()
{
	int res = kring_open( &kring, KRING_PLAIN, "stress1", 0, KRING_WRITE );
	if ( res < 0 ) {
		log_ERROR( "decrypted data kring open for write failed: " << kring_error( &kring, res ) );
		return -1;
	}

	char buf[kring_plain_max_data()];

	for ( int i = 0; i < 10; i++ ) {
		sprintf( buf, "w: %d", i );
		kring_write_plain( &kring, buf, strlen(buf) );
		log_message( "message sent: " << buf );
	}

	log_message("writer finished");

	return 0;
}

