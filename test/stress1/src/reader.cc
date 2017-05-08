#include "reader.h"
#include "writer.h"
#include <kring/kring.h>

void ReaderThread::recvShutdown( Shutdown * )
{
	loopBreak();
}

int ReaderThread::main()
{
	char buf[1];
	int received = 0;

	log_message("reader");

	struct kctrl_user kring;

	int r = kctrl_open( &kring, KCTRL_PLAIN, "stress1", 0, KCTRL_READ );
	if ( r < 0 ) {
		log_ERROR( "decrypted data kring open failed: " << kctrl_error( &kring, r ) );
		return -1;
	}

	/* Notify the main thread that we have entered. */
	sendsToMain->openEntered();
	sendsToMain->send();

	loopBegin();

	while ( true ) {
		poll();

		if ( !loopContinue() )
			break;

		/* Spin. */
		if ( kctrl_avail( &kring ) ) {
			/* Load. */
			struct kctrl_plain plain;
			kctrl_next_plain( &kring, &plain );

			log_message( "plain: " << log_array( plain.bytes, plain.len ) );

			received += 1;
		}

		if ( received == ( WRITERS * MESSAGES ) ) {
			log_message( "breaking" );
			break;
		}

		int r = recv( kring.socket, buf, 1, 1 );
		if ( r < 0 )
			log_ERROR( "kring recv failed: " << strerror( r ) );

	}

	return 0;
}

