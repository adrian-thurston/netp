#include "main.h"
#include "itq_gen.h"
#include "packet_gen.h"
#include "genf.h"

#include <unistd.h>
#include <signal.h>
#include <fstream>
#include <sstream>
#include <math.h>
#include <netp/module.h>

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

void MainThread::recvPacketType( SelectFd *fd, Record::PacketType *pkt )
{
	/* Create the struct representation of the object. */
	Struct *s = new Struct;
	s->ID = pkt->numId;
	for ( Record::PacketField f(pkt->rope, pkt->head_fields); f.valid(); f.advance() ) {
		Field *field = new Field;
		field->name = f.name;
		field->type = f.type;
		field->size = f.size;
		field->offset = f.foffset;

		s->fieldList.append( field );
	}

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

void MainThread::dispatchPacket( SelectFd *fd, Recv &recv )
{
	StructMapEl *db = structMap.find( recv.head->msgId );
	if ( db != 0 ) {
		Struct *s = db->value;
		for ( Field *f = s->fieldList.head; f != 0; f = f->next ) {
			if ( f->type == 6 ) {
				char *overlay = Thread::pktFind( &recv.buf,
					sizeof(PacketBlockHeader) + sizeof(PacketHeader) );
				char *str = Thread::pktFind( &recv.buf,
					*((uint32_t*)(overlay + f->offset )) );
	
				log_message( f->name << ": " << str );
			}
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

