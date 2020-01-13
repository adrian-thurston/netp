#include "netp.h"
#include "itq_gen.h"

#include <aapl/vector.h>
#include <fstream>

%%{
	machine identifier;

	alphtype unsigned char;

	main := 
		'GET ' [^ \r\n]+ ' HTTP\/1.1' '\r'? '\n' @{
			proto = HTTP_REQ;
			log_debug( DBG_IDENT, "identified HTTP REQUEST" );
			fbreak;
		} |
		'HTTP/1.1 ' [^ \r\n]+ ' ' [^ \r\n]+ '\r'? '\n' @{
			proto = HTTP_RSP;
			log_debug( DBG_IDENT, "identified HTTP RESPONSE" );
			fbreak;
		};
}%%

%% write data;

void Identifier::start( Context *ctx )
{
	%% write init;

	proto = Working;
}

int Identifier::receive( Context *ctx, Packet *packet, const wire_t *data, int length )
{
	if ( cs == identifier_error || proto != Working )
		return 0;

	const wire_t *p = data;
	const wire_t *pe = data + length;

	%% write exec;

	if ( cs == identifier_error )
		proto = Unknown;

	return 0;
}

void Identifier::finish( Context *ctx )
{
}

const char *FileIdent::FtStr[] =
{
	/* Working = 1 */
	0, "<WORKING>",
	"HTML",
	"PNG", "JPG", "GIF",
	"BZ", "Z", "GZ", "ZIP",
	"PE", "<UNKNOWN>"
};

%%{
	machine file_ident;

	alphtype unsigned char;

	action nb_init { c = 0; }
	action nb_inc  { c++; }
	action nb_min  { c >= nbytes }
	action nb_max  { c < nbytes }


	main :=
		[ \t\r\n]* '<' [ \t]* 'html'i [ \t]* '>' @{
			fileType = HTML;
			fbreak;
		} |
		[ \t\r\n]* '<!' [ \t]* 'doctype'i [ \t]+ 'html'i '>' @{
			fileType = HTML;
			fbreak;
		}
		0xff 0xd8 0xff 0xe0 @{
			fileType = JPG;
			fbreak;
		} |
		0x89 0x50 0x4e 0x47 @{
			fileType = PNG;
			fbreak;
		} |
		0x47 0x49 0x46 0x38 @{
			fileType = GIF;
			fbreak;
		} |
		0x47 0x49 0x46 0x38 @{
			fileType = GIF;
			fbreak;
		} |
		0x42 0x5a @{
			fileType = BZ;
			fbreak;
		} |
		0x1f 0x9d @{
			fileType = Z;
			fbreak;
		} |
		0x1f 0x8b @{
			fileType = GZ;
			fbreak;
		} |
		0x50 0x4b 0x03 0x04 @{
			fileType = ZIP;
			fbreak;
		} |

		'M' 'Z' any{58}
		any @{
			unsigned int off = *p;
			log_debug( DBG_FILE, "PE header off: " << off );
			nbytes = off - 61;
			if ( nbytes < 1 || nbytes > 255 )
				fgoto *file_ident_error;
		}

		:condplus( any, nb_init, nb_inc, nb_min, nb_max ):

		'PE' 0 0 @{
			fileType = PE;
			fbreak;
		};
}%%

%% write data;

void FileIdent::start( Context *ctx )
{
	%% write init;

	fileType = Working;
}

int FileIdent::receive( Context *ctx, Packet *packet, const wire_t *data, int length )
{
	if ( cs == file_ident_error || fileType != Working )
		return 0;

	const wire_t *p = data;
	const wire_t *pe = data + length;

	%% write exec;

	if ( cs == file_ident_error )
		fileType = Unknown;

	return 0;
}

void FileIdent::finish( Context *ctx )
{
}
