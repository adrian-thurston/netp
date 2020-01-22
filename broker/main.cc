#include "main.h"
#include "itq_gen.h"
#include "packet_gen.h"
#include "genf.h"

#include <unistd.h>
#include <signal.h>
#include <fstream>
#include <sstream>
#include <math.h>
#include <parse/module.h>

#include <curl/curl.h>

Last::Last()
:
	retain(1),
	dest(0),
	have(0)
{
	msg = new Rope[retain];
}

Last::~Last()
{
	delete[] msg;
}

ClientConnection::ClientConnection( MainThread *thread )
:
	PacketConnection( thread ),
	mainThread( thread ),
	brokerConnClosed( false )
{}

void ClientConnection::packetClosed()
{
	if ( mainThread->replay != 0 )
		mainThread->breakLoop();

	//log_message( "broker: packet connection closed" );

	/* Don't delete, mark. */
	brokerConnClosed = true;
}

BrokerListener::BrokerListener( MainThread *thread )
:
	PacketListener( thread ),
	mainThread(thread)
{}

void ClientConnection::failure( FailType failType )
{
	log_ERROR( "broker connection failure: " << failType );
}

void ClientConnection::notifyAccept()
{
	//log_message( "broker: incoming connection fd: " << selectFd->fd );

	selectFd->wantReadSet( true );
}

ClientConnection *BrokerListener::connectionFactory( int fd )
{
	ClientConnection *bc = new ClientConnection( mainThread );
	mainThread->connList.append( bc );
	return bc;
}


void MainThread::recvWantId( SelectFd *fd, Record::WantId *pkt )
{
	//log_message( "recording want id: " << pkt->wantId );

	Connection *conn = static_cast<Connection*>( fd->local );
	ClientConnection *bc = dynamic_cast<ClientConnection*>( conn );
	
	bc->wantIds.insert( pkt->wantId );

	PacketWriter writer( bc );

	for ( LastList::Iter last = lastList; last.lte(); last++ ) {
		if ( last->msgId == pkt->wantId ) {
			int m = ( last->dest - last->have + last->retain ) % last->retain;
			for ( int i = 0; i < last->have; i++ ) {
				PacketBase::send( &writer, last->msg[m], false );
				m = ( m + 1 ) % last->retain;
			}
		}
	}
}

void MainThread::recvPing( SelectFd *fd, Record::Ping *pkt )
{
}

void MainThread::readStruct( Struct *s, Record::PacketField &fieldIter, int numFields  )
{
	while ( numFields > 0 && fieldIter.valid() ) {
		/* Collect the field. */
		Field *field = new Field;
		field->name = fieldIter.name;
		field->tag = fieldIter._tag;
		field->type = fieldIter.type;
		field->size = fieldIter.size;
		field->offset = fieldIter._offset;

		s->fieldList.append( field );

		numFields -= 1;
		fieldIter.advance();

		/* If the field type is list, the fields of the list's structure appear
		 * immediately after the field descriptor. The size tells us how many
		 * fields. */
		if ( field->type == FieldTypeList ) {
			field->listOf = new Struct;
			readStruct( field->listOf, fieldIter, field->size );
			s->hasList = field;
		}
	}
}

void MainThread::recvPacketType( SelectFd *fd, Record::PacketType *pkt )
{
	/* Create the struct representation of the object. */
	Struct *s = new Struct;
	s->name = pkt->_name;
	s->ID = pkt->numId;

	Record::PacketField fieldIter(pkt->rope, pkt->head_fields);
	readStruct( s, fieldIter, pkt->numFields );

	/* Insert in the map. If it exists already, we replace it. */
	StructMapEl *structMapEl = 0;
	structMap.insert( pkt->numId, 0, &structMapEl );
	if ( structMapEl->value != 0 )
		delete structMapEl->value;
	structMapEl->value = s;
}

void MainThread::recvSetRetain( SelectFd *fd, Record::SetRetain *pkt )
{
	Last *found = 0;
	for ( LastList::Iter last = lastList; last.lte(); last++ ) {
		if ( last->msgId == pkt->id ) {
			found = last;
			break;
		}
	}

	if ( found == 0 ) {
		found = new Last;
		lastList.append( found );
	}

	found->retain = pkt->retain;
}

void MainThread::resendPacket( SelectFd *fd, Recv &recv )
{
	for ( BrokerConnectionList::Iter out = connList; out.lte(); out++ ) {
		if ( !out->brokerConnClosed && out->isEstablished() ) {
			if ( out->wantIds.find( recv.head->msgId ) ) {
				//log_message( "-> resending (dispatch) from " << fd->fd <<
				//		" to " << out->selectFd->fd );

				PacketWriter writer( out );
				PacketBase::send( &writer, recv.buf, false );
			}
		}
	}

	for ( BrokerConnectionList::Iter out = connList; out.lte(); ) {
		BrokerConnectionList::Iter next = out.next();
		if ( out->brokerConnClosed ) {
			//log_message( "detaching broker connection" );
			connList.detach( out );
		}
		out = next;
	}

	/*
	 * Stash most recent.
	 */
	Rope *rope = 0;
	Last *last = 0;
	for ( LastList::Iter li = lastList; li.lte(); li++ ) {
		if ( li->msgId == recv.head->msgId ) {
			last = li;
			break;
		}
	}

	if ( last == 0 ) {
		last = new Last;
		last->msgId = recv.head->msgId;
		lastList.append( last );
	}

	rope = &last->msg[last->dest];
	rope->transfer( recv.buf );
	last->dest = ( last->dest + 1 ) % last->retain;
	if ( last->have < last->retain )
		last->have += 1;

	/*
	 * Log last.
	 */
	std::stringstream fileName;
	fileName << PKGSTATEDIR "/packet-" << packetLowerName(last->msgId) << ".log";

	std::ofstream feed( fileName.str().c_str(), std::ios::app );

	for ( RopeBlock *rb = rope->hblk; rb != 0; rb = rb->next ) {
		char *data = rope->data(rb);
		int blockLen = rope->length(rb);
		feed.write( data, blockLen );
	}

	feed.close();
}

void MainThread::stashBool( std::ostream &post, char &sep, uint32_t base, Field *f, Recv &recv )
{
	char *overlay = Thread::pktFind( &recv.buf, base );

	// log_message( "fetching bool at " << f->offset );
	bool b = *((bool*)(overlay + f->offset));

	post << sep << f->name << "=" << ( b ? 't' : 'f' ) << "";

	sep = ',';
}

void MainThread::stashInt( std::ostream &post, char &sep, uint32_t base, Field *f, Recv &recv )
{
	char *overlay = Thread::pktFind( &recv.buf, base );

	// log_message( "fetching int at " << f->offset );
	int i = *((int*)(overlay + f->offset));

	post << sep << f->name << "=" << i;

	sep = ',';
}

void MainThread::stashUnsignedInt( std::ostream &post, char &sep, uint32_t base, Field *f, Recv &recv )
{
	char *overlay = Thread::pktFind( &recv.buf, base );

	// log_message( "fetching unsigned int at " << f->offset );
	unsigned int ui = *((unsigned int*)(overlay + f->offset));

	post << sep << f->name << "=" << ui;

	sep = ',';
}

void MainThread::stashLong( std::ostream &post, char &sep, uint32_t base, Field *f, Recv &recv )
{
	char *overlay = Thread::pktFind( &recv.buf, base );

	// log_message( "fetching long at " << f->offset );
	long l = *((long*)(overlay + f->offset));

	post << sep << f->name << "=" << l;

	sep = ',';
}

void MainThread::stashUnsignedLong( std::ostream &post, char &sep, uint32_t base, Field *f, Recv &recv )
{
	char *overlay = Thread::pktFind( &recv.buf, base );

	// log_message( "fetching unsgiend long at " << f->offset );
	unsigned long ul = *((unsigned long*)(overlay + f->offset));

	post << sep << f->name << "=" << ul;

	sep = ',';
}

void MainThread::lineProtocolStrEscape( std::ostream &post, char *str, int len )
{
	for ( char *p = str; len > 0; p++, len-- ) {
		if ( *p == '"' )
			post << '\\' << '"';
		else
			post << *p;
	}
}	

void MainThread::stashString( std::ostream &post, char &sep, uint32_t base,
		bool tags, Field *f, Recv &recv )
{
	char *overlay = Thread::pktFind( &recv.buf, base );

	char *str = Thread::pktFind( &recv.buf,
			*((uint32_t*)(overlay + f->offset)) );

	post << sep << f->name << "=";

	if ( !tags )
		post << "\"";

	lineProtocolStrEscape( post, str, strlen( str ) );

	if ( !tags )
		post << "\"";

	sep = ',';
}

void MainThread::stashChar( std::ostream &post, char &sep, uint32_t base,
		bool tags, Field *f, Recv &recv )
{
	char *overlay = Thread::pktFind( &recv.buf, base );

	char *str = ((char*)(overlay + f->offset));

	int size = f->size;
	char *null = (char*)memchr( str, 0, f->size );
	if ( null != 0 )
		size = ( null - str );

	post << sep << f->name << "=";
	
	if ( !tags )
		post << "\"";

	lineProtocolStrEscape( post, str, size );

	if ( !tags )
		post << "\"";

	sep = ',';
}

void MainThread::stashFieldList( std::ostream &post, char &sep,
		uint32_t base, bool tags, const FieldList &fieldList, Recv &recv )
{
	for ( Field *f = fieldList.head; f != 0; f = f->next ) {
		if ( ! ( tags xor f->tag ) ) {
			switch ( f->type ) {
				case FieldTypeBool:
					stashBool( post, sep, base, f, recv );
					break;
				case FieldTypeInt:
					stashInt( post, sep, base, f, recv );
					break;
				case FieldTypeUnsignedInt:
					stashUnsignedInt( post, sep, base, f, recv );
					break;
				case FieldTypeLong:
					stashLong( post, sep, base, f, recv );
					break;
				case FieldTypeUnsignedLong:
					stashUnsignedLong( post, sep, base, f, recv );
					break;
				case FieldTypeString:
					stashString( post, sep, base, tags, f, recv );
					break;
				case FieldTypeChar:
					stashChar( post, sep, base, tags, f, recv );
					break;
				case FieldTypeList:
					/* Not including these explicitly. */
					break;
			}
		}
	}
}

void MainThread::stashStruct( std::ostream &post, Struct *strct, Recv &recv )
{
	char sep;
	const uint32_t headerSize = sizeof(PacketBlockHeader) + sizeof(PacketHeader);

	if ( strct->hasList != 0 ) {

		/* Locate offset of the head by reading the field. */
		char *overlay = Thread::pktFind( &recv.buf, headerSize );
		uint32_t itemOffset = *((uint32_t*)( overlay + strct->hasList->offset ));

		while ( itemOffset != 0 ) {
			// log_message( "stash item from " << itemOffset );

			post << strct->name;

			sep = ',';

			stashFieldList( post, sep, headerSize,
					true, strct->fieldList, recv );

			stashFieldList( post, sep, itemOffset,
					true, strct->hasList->listOf->fieldList, recv );

			sep = ' ';

			stashFieldList( post, sep, headerSize,
					false, strct->fieldList, recv );

			stashFieldList( post, sep, itemOffset,
					false, strct->hasList->listOf->fieldList, recv );

			post << "\n";

			/* Next pointer is offset 0 from the offset of the item. */
			char *overlay = Thread::pktFind( &recv.buf, itemOffset );
			itemOffset = *((uint32_t*)( overlay + 0 ));
		}
	}
	else {
		post << strct->name;
		sep = ',';

		stashFieldList( post, sep, headerSize,
				true, strct->fieldList, recv );

		sep = ' ';

		stashFieldList( post, sep, headerSize,
				false, strct->fieldList, recv );
	}
}

void MainThread::writeInflux( std::string post )
{
	// log_message( "posting:\n" << post() );

	std::string writeUrl = "http://127.0.0.1:9999/api/v2/write"
		"?org=thurston&bucket=curltest&precision=s";

	CURL *writeHandle = curl_easy_init();
	curl_easy_setopt( writeHandle, CURLOPT_URL, writeUrl.c_str() );
	curl_easy_setopt( writeHandle, CURLOPT_CONNECTTIMEOUT, 10 );
	curl_easy_setopt( writeHandle, CURLOPT_TIMEOUT, 10 );
	curl_easy_setopt( writeHandle, CURLOPT_POST, 1 );
	curl_easy_setopt( writeHandle, CURLOPT_TCP_KEEPIDLE, 120L );
	curl_easy_setopt( writeHandle, CURLOPT_TCP_KEEPINTVL, 60L );
	FILE *devnull = fopen( "/dev/null", "w+" );
	curl_easy_setopt( writeHandle, CURLOPT_WRITEDATA, devnull );

	std::string auth = 
		std::string("Authorization: Token ") + influxToken;

	struct curl_slist *chunk = NULL;
	chunk = curl_slist_append( chunk, auth.c_str() );
								 
	curl_easy_setopt( writeHandle, CURLOPT_HTTPHEADER, chunk );

	CURLcode response;
	long responseCode;

	curl_easy_setopt( writeHandle, CURLOPT_POSTFIELDS, post.c_str() );
	curl_easy_setopt( writeHandle, CURLOPT_POSTFIELDSIZE, (long) post.size() );
	response = curl_easy_perform( writeHandle );
	curl_easy_getinfo(writeHandle, CURLINFO_RESPONSE_CODE, &responseCode);
	if ( response != CURLE_OK ) {
		log_ERROR( "curl perform failed: " << response );
	}
	if ( responseCode < 200 || responseCode > 206 ) {
		log_ERROR( "curl perform response code: " << responseCode );
	}

	curl_easy_cleanup( writeHandle );
}

void MainThread::stashInflux( Struct *strct, Recv &recv )
{
	std::ostringstream post;

	stashStruct( post, strct, recv );
	writeInflux( post.str() );
}

void MainThread::dispatchPacket( SelectFd *fd, Recv &recv )
{
	StructMapEl *db = structMap.find( recv.head->msgId );
	if ( db != 0 ) {
		if ( influxToken != 0 ) {
			stashInflux( db->value, recv );
		}
		else {
			log_ERROR( "received packet for influx, but --influx-token specied" );
		}
	}

	if ( replay != 0 ) {
		//log_message( "dispatching packet" );
		attached->selectFd->wantReadSet( false );

		PacketWriter writer( client );
		PacketBase::send( &writer, recv.buf, false );
	}
	else {
		if ( recv.head->msgId == Record::Ping::ID ||
				recv.head->msgId == Record::WantId::ID ||
				recv.head->msgId == Record::PacketType::ID )
		{
			MainGen::dispatchPacket( fd, recv );
		}
		else {
			moduleList.brokerDispatchPacket( fd, recv );

			/* Warning: this consumes the rope! */
			resendPacket( fd, recv );
		}
	}

}

void MainThread::handleTimer()
{
	if ( replay != 0 ) {
		attached->selectFd->wantReadSet( true );
	}
		
	// log_message( "timer" );
}

int MainThread::service()
{
	log_message( "main starting" );

	for ( OptStringEl *opt = moduleArgs.head; opt != 0; opt = opt->next )
		moduleList.loadModule( opt->data );
			
	moduleList.initModules();

	tlsStartup();
	SSL_CTX *sslCtx = sslCtxServerInternal();

	PacketListener *listener = new BrokerListener( this );

	if ( external ) {
		ClientConnection *cc = new ClientConnection( this );

		cc->initiate( external, 4830, true, sslCtx, false );

		connList.append( cc );
	}

	/* Initiate listen. */
	listener->startListen( 4830, true, sslCtx, false );

	struct timeval t;
	t.tv_sec = 1;
	t.tv_usec = 0;

	selectLoop( &t );

	tlsShutdown();

	log_message( "main exiting" );

	return 0;
}

void MainThread::checkOptions()
{
	if ( replay != 0  )
		usePid = false;
}

ClientConnection *MainThread::attachToFile( int fd )
{
	bool nb = makeNonBlocking( fd );
	if ( !nb )
		log_ERROR( "pkt-listen, post-accept: non-blocking IO not available" );

	ClientConnection *pc = new ClientConnection( this );
	connList.append( pc );
	SelectFd *selectFd = new SelectFd( this, fd, 0 );
	selectFd->local = static_cast<Connection*>(pc);
	pc->selectFd = selectFd;

	pc->tlsConnect = false;
	selectFd->type = SelectFd::Connection;
	selectFd->state = SelectFd::Established;
	selectFd->wantRead = true;
	selectFdList.append( selectFd );
	pc->notifyAccept();

	return pc;
}

void MainThread::runReplay()
{
	log_message( "replay starting" );

	tlsStartup();
	SSL_CTX *sslCtx = sslCtxClientInternal();

	/* Connection to broker. */
	ClientConnection *cc = new ClientConnection( this );
	cc->initiate( "localhost", 4830, true, sslCtx, false );
	connList.append( cc );

	client = cc;

	/* Input log file */
	FILE *file = fopen( replay, "rb" );
	int fd = fileno( file );

	attached = attachToFile( fd );

	struct timeval t;
	t.tv_sec = 5;
	t.tv_usec = 0;

	selectLoop( &t );

	tlsShutdown();

	fclose( file );

	log_message( "replay exiting" );
}

int MainThread::main()
{
	if ( replay != 0 ) {
		runReplay();
		return 0;
	}
	else {
		return service();
	}
}

