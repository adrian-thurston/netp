#include "reader.h"
#include "writer.h"
#include <kring/kring.h>

void ReaderThread::recvShutdown( Shutdown * )
{
	loopBreak();
}

int ReaderThread::main()
{
	int received = 0;
	long expected[WRITERS];

	for ( int w = 0; w < WRITERS; w++ )
		expected[w] = 0;

	log_message( "reader" );

	struct kctrl_user kring;

	int r = kctrl_open( &kring, KCTRL_PLAIN, "stress1", KCTRL_READ );
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

			unsigned char w = plain.bytes[0];
			long l = *( (long*)(plain.bytes+1) );

			// log_message( "read " << (int)w << " l: " << l << " received: " << received );

			if ( expected[w] != l ) {
				log_FATAL( "failure at w: " << (int)w << " l: " << l << " expected: " << expected[w] << " received: " << received );
			}

			expected[w] += 1;

			received += 1;
		}

		if ( received == ( WRITERS * MESSAGES ) ) {
			log_message( "reader finished" );
			break;
		}
	}

	return 0;
}

