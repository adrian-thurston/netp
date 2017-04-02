#include "reader.h"
#include <kring/kring.h>

void ReaderThread::recvShutdown( Shutdown * )
{
	loopBreak();
}

int ReaderThread::main()
{
	char buf[1];

	log_message("reader");

	struct kring_user kring;

	int r = kring_open( &kring, KRING_PLAIN, "stress1", KR_RING_ID_ALL, KRING_READ );
	if ( r < 0 ) {
		log_ERROR( "decrypted data kring open failed: " << kring_error( &kring, r ) );
		return -1;
	}

	loopBegin();

	while ( true ) {
		poll();

		if ( !loopContinue() )
			break;

		/* Spin. */
		if ( kring_avail( &kring ) ) {
			/* Load. */
			struct kring_plain plain;
			kring_next_plain( &kring, &plain );

			log_message( "plain: " << log_array( plain.bytes, plain.len ) );
		}

		int r = recv( kring.socket, buf, 1, 1 );
		if ( r < 0 )
			log_ERROR( "kring recv failed: " << strerror( r ) );
	}

	return 0;
}

