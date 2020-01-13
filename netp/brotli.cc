#include "netp.h"
#include <genf/thread.h>

Brotli::Brotli()
:
	output(0)
{
	destLen = 8192;
	dest = new uint8_t[destLen];
}

void Brotli::start( Context *ctx )
{
	stream = BrotliDecoderCreateInstance( 0, 0, 0 );
	output->start( ctx );
}

int Brotli::receive( Context *ctx, Packet *packet, const wire_t *data, int length )
{
	const uint8_t *next_in = (uint8_t*) data;
	size_t avail_in = (uLong) length;

	while ( true ) {

		uint8_t *next_out = dest;
		size_t avail_out = destLen;

		BrotliDecoderResult res = BrotliDecoderDecompressStream( stream,
				&avail_in, &next_in, &avail_out, &next_out, 0 );

		if ( res == BROTLI_DECODER_RESULT_ERROR ) {
			/* No progress possible. */
			log_ERROR( "brotli decoding error" );
		}
		else if ( res == BROTLI_DECODER_RESULT_SUCCESS ) {
			/* The end of the compressed data has been reached and all
			 * uncompressed output has been produced. */

			/* Send out any produced data. */
			if ( avail_out < destLen ) {
				output->receive( ctx, packet, dest, destLen - avail_out );
			}
		}
		else {
			/* More input processed or more output produced. */

			/* if any output present, send it. */
			if ( avail_out < destLen ) {
				/* Send out. */
				output->receive( ctx, packet, dest, destLen - avail_out );
			}

			/* Maybe need more output space. Input may or may not be consumed. */
			if ( res == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT )
				continue;
		}

		break;
	}
	return 0;
}

void Brotli::finish( Context *ctx )
{
	if ( ! BrotliDecoderIsFinished( stream ) )
		log_ERROR( "brotli decoder not finished" );
	output->finish( ctx );
	BrotliDecoderDestroyInstance( stream );
}
