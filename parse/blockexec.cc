#include "parse.h"
#include "itq_gen.h"
#include <genf/thread.h>

void BlockExec::start( Context *ctx, Packet *packet, const wire_t *data )
{
	log_debug( DBG_BLOCK, "block exec start: setting from" );
	open = true;
	from = data;
	output->start( ctx );
}

void BlockExec::preExec( Context *ctx, Packet *packet, const wire_t *data )
{
	if ( open ) {
		log_debug( DBG_BLOCK, "block exec pre-exec: setting from" );
		from = data;
	}
}

void BlockExec::postExec( Context *ctx, Packet *packet, const wire_t *data )
{
	if ( open ) {
		log_debug( DBG_BLOCK, "block exec post-exec: shipping data" );
		output->receive( ctx, packet, from, data-from );
		from = 0;
	}
}

void BlockExec::pause( Context *ctx, Packet *packet, const wire_t *data )
{
	log_debug( DBG_BLOCK, "block exec pause: shipping" );
	output->receive( ctx, packet, from, data-from );
	open = false;
}

void BlockExec::resume( Context *ctx, Packet *packet, const wire_t *data )
{
	log_debug( DBG_BLOCK, "block exec resume: setting from" );
	open = true;
	from = data;
}

void BlockExec::finish( Context *ctx )
{
	output->finish( ctx );

	open = false;
	output = 0;
	from = 0;
}

void BlockExec::finish( Context *ctx, Packet *packet, const wire_t *data )
{
	log_debug( DBG_BLOCK, "block exec finish: " << ( open ? "shipping" : "not open" ) );

	if ( open )
		output->receive( ctx, packet, from, data-from );

	finish( ctx );
}



