#include "fetch.h"

void FetchConnection::readReady()
{
	while ( true ) {
		char bytes[8192*2];
		int nbytes = read( bytes, sizeof(bytes) );

		if ( nbytes < 0 ) {
			failure( Connection::SslReadFailure );
			close();
			//if ( state == Sent )
			//	requestList.detachFirst();
			state = Disconnected;
			break;
		}
		else if ( nbytes > 0 ) {
			dataAvail( bytes, nbytes );
			if ( closed )
				break;
		}
		else if ( nbytes == 0 ) {
			break;
		}
	}
}

void FetchConnection::failure( FailType failType )
{
	//log_message( "common connection failure: " << failType );
}

void FetchConnection::connectComplete()
{
	//log_message( "common connection: complete" );
	requestParser.start( &requestContext );
	responseParser.start( &responseContext );

	requestParser.responseParser = &responseParser;
	responseParser.requestParser = &requestParser;

	responseContext.vpt.other = &requestContext.vpt;
	requestContext.vpt.other = &responseContext.vpt;

	responseContext.vpt.push( Node::ConnTls );
	responseContext.vpt.push( Node::ConnHost );
	responseContext.vpt.setText( fetchHost );
	responseContext.vpt.pop( Node::ConnHost );

	requestContext.vpt.push( Node::ConnTls );
	requestContext.vpt.push( Node::ConnHost );
	requestContext.vpt.setText( fetchHost );
	requestContext.vpt.pop( Node::ConnHost );

	state = Idle;
	lastSend = 0;

	maybeSend();
}

void FetchConnection::getGoing( const char *host )
{
	fetchHost = host;
	if ( requestList.length() > 0 ) {
		if ( state == FetchConnection::Disconnected ) {
			//log_message( "common connection: connecting" );
			initiate( host, 443, true, sslCtx, true );
			state = FetchConnection::Connecting;
		}
	}
}

void FetchConnection::dataAvail( char *bytes, int nbytes )
{
	responseParser.receive( &responseContext, 0, (const unsigned char*) bytes, nbytes );
}

Consumer *FetchConnection::nextConsumer()
{
	return requestList.head->consumer;
}

void FetchConnection::preFinish()
{
	requestList.detachFirst();
	state = FetchConnection::Idle;
}

void FetchConnection::responseComplete()
{
	maybeSend();
}

void FetchConnection::maybeSend()
{
	if ( state == Idle && requestList.length() > 0 && time(0) - lastSend > throttleDelay ) {
		Request *request = requestList.head;

		requestParser.receive( &requestContext, 0, (const wire_t*)request->content.c_str(),
				(int)request->content.size() );

		int nbytes = write( (char*)request->content.c_str(), request->content.size() );
		if ( nbytes != (int)request->content.size() ) {
			/* FIXME: need to handle delays. */
			//log_message( "not able to send full request" );
			close();
			state = Disconnected;
		}
		state = Sent;
		lastSend = time(0);

		//log_message( "common connection: sent: " << request );
	}
}

void FetchConnection::timer()
{
	maybeSend();
}

void FetchConnection::queueRequest( Request *request )
{
	requestList.append( request );
	//log_message( "common connection: queued: " << request );
	maybeSend();
}

void FetchConnection::disconnect()
{
	//log_message( "common connection: disconnecting" );
	close();
	if ( state == Sent )
		requestList.detachFirst();
	state = Disconnected;
}
