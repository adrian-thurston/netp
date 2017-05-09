#include "writer.h"


int kctrl_write_plain2( struct kctrl_user *u, char *data, int len )
{
	return 0;
}



int WriterThread::main()
{
	int res = kctrl_open( &kring, KCTRL_PLAIN, "stress1", 0, KCTRL_WRITE );
	if ( res < 0 ) {
		log_ERROR( "decrypted data kring open for write failed: " << kctrl_error( &kring, res ) );
		return -1;
	}

	char buf[kctrl_plain_max_data()];

	log_message( "writer " << writerId << " receiveed id " << kring.writer_id );

	long next = 0;

	for ( int i = 0; i < MESSAGES; i++ ) {
		buf[0] = (unsigned char)writerId;
		*( (long*)(buf+1) ) = next;

		next += 1;

		kctrl_write_plain( &kring, buf, 1 + sizeof(long) );
		// log_message( "message sent: " << buf );
	}

	log_message( "writer finished" );

	return 0;
}

