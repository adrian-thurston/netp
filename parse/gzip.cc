#include "parse.h"
#include "itq_gen.h"
#include <zlib.h>

extern "C" int uncompress2( Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen );

Gzip::Gzip()
	: output(0)
{
	destLen = 8192;
	dest = new Bytef[destLen];
}

void Gzip::logResult( int r )
{
	switch( r ) {
		case Z_OK:
			log_debug( DBG_GZIP, "uncompress result: Z_OK" );
			break;
		case Z_STREAM_END:
			log_debug( DBG_GZIP, "uncompress result: Z_STREAM_END" );
			break;
		case Z_NEED_DICT:
			log_debug( DBG_GZIP, "uncompress result: Z_NEED_DICT" );
			break;
		case Z_ERRNO:
			log_debug( DBG_GZIP, "uncompress result: Z_ERRNO" );
			break;
		case Z_STREAM_ERROR:
			log_debug( DBG_GZIP, "uncompress result: Z_STREAM_ERROR" );
			break;
		case Z_DATA_ERROR:
			log_debug( DBG_GZIP, "uncompress result: Z_DATA_ERROR" );
			break;
		case Z_MEM_ERROR:
			log_debug( DBG_GZIP, "uncompress result: Z_MEM_ERROR" );
			break;
		case Z_BUF_ERROR:
			log_debug( DBG_GZIP, "uncompress result: Z_BUF_ERROR" );
			break;
		case Z_VERSION_ERROR:
			log_debug( DBG_GZIP, "uncompress result: Z_VERSION_ERROR" );
			break;
		default:
			log_debug( DBG_GZIP, "uncompress result: UNKNOWN" );
			break;
	}
}

void Gzip::start( Context *ctx )
{
	//int err;

	memset( &stream, 0, sizeof(stream) );

	/* ADT: modified here. */
	/* err = */ inflateInit2( &stream, 16+MAX_WBITS );

	//if (err != Z_OK) return err;

	output->start( ctx );
}

int Gzip::uncompress2( Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen )
{
	int err;

	stream.next_in = (z_const Bytef *)source;
	stream.avail_in = (uInt)sourceLen;
	/* Check for source > 64K on 16-bit machine: */
	if ((uLong)stream.avail_in != sourceLen) return Z_BUF_ERROR;

	stream.next_out = dest;
	stream.avail_out = (uInt)*destLen;
	if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;

	err = inflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		inflateEnd(&stream);
		if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0))
			return Z_DATA_ERROR;
		return err;
	}
	*destLen = stream.total_out;

	err = inflateEnd(&stream);
	return err;
}

void Gzip::decompressBuf( Context *ctx, Packet *packet, char *buf, int len )
{
	int err;

	stream.next_in = (z_const Bytef*) buf;
	stream.avail_in = (uLong) len;

	while ( true ) {

		stream.next_out = dest;
		stream.avail_out = destLen;

		err = inflate( &stream, Z_NO_FLUSH );
		logResult( err );

		if ( err == Z_OK ) {
			/* More input processed or more output produced. */

			/* if any output present, send it. */
			if ( stream.avail_out < destLen ) {
				/* Send out. */
				output->receive( ctx, packet, dest, destLen - stream.avail_out );
			}

			/* if any input left, continue */
			if ( stream.avail_out == 0 )
				continue;
		}
		else if ( err == Z_BUF_ERROR ) {
			/* No progress possible. */
		}
		else if ( err == Z_STREAM_END ) {
			/* The end of the compressed data has been reached and all
			 * uncompressed output has been produced. */

			/* Send out any produced data. */
			if ( stream.avail_out < destLen ) {
				output->receive( ctx, packet, dest, destLen - stream.avail_out );
			}
		}

		break;
	}
}

void Gzip::finish( Context *ctx )
{
	/* The decompress class with call libz with a flush, but it's not necessary
	 * so far because the streams we process have end markers. */
	output->finish( ctx );
}
