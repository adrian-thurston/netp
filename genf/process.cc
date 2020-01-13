#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "thread.h"

int WriteBuffer::write( SelectFd *selectFd, char *data, int len )
{
	int res = ::write( selectFd->fd, data, len );
	if ( res < 0 ) {
		if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
			/* Cannot write anything now. */
			return 0;
		}
		else {
			/* error-based closure. */
			return -1;
		}
	}
	return res;
}

void WriteBuffer::send( SelectFd *selectFd, char *data, int blockLen )
{
	if ( selectFd->wantWriteGet() ) {
		log_debug( DBG_PACKET, "in want write mode "
				"queueing entire rope: " <<
				selectFd->tlsEstablished << " " <<
				selectFd->tlsWantWrite << " " <<
				selectFd->wantWrite );

		queue.append( data, blockLen );
	}
	else {
		log_debug( DBG_PACKET, "packet send, sending blocks" );

		int res = write( selectFd, data, blockLen );
		if ( res < 0 ) {
			log_debug( DBG_PACKET, "packet write: closed" );
			writeFuncs->bufClose( selectFd );
		}
		else {

			log_debug( DBG_PACKET, " -> sent " << res << " of " << ( blockLen ) << " bytes" );

			if ( res < blockLen ) {
				/* Didn't write a whole block. Put the remainder of the block
				 * and the message on the queue and go into wantWrite mode. */
				log_debug( DBG_PACKET, "failed to send full block, queueing " <<
						(blockLen-res) << " of " << blockLen );

				/* Copy data from blocks to output queue. */
				queue.append( data + res, blockLen - res);
				selectFd->wantWriteSet( true );
			}

			log_debug( DBG_PACKET, "packet send result: " << res );
		}
	}
}

void WriteBuffer::send( SelectFd *selectFd, Rope &blocks, bool canConsume )
{
	if ( selectFd->wantWriteGet() ) {
		log_debug( DBG_PACKET, "in want write mode "
				"queueing entire rope: " <<
				selectFd->tlsEstablished << " " <<
				selectFd->tlsWantWrite << " " <<
				selectFd->wantWrite );

		for ( RopeBlock *drb = blocks.hblk; drb != 0; drb = drb->next )
			log_debug( DBG_PACKET, "-> queueing " << blocks.length( drb ) << " bytes" );

		/* If in want write mode then there are queued blocks. Add to the queue
		 * and let the writeReady callback deal with the output. */
		if ( canConsume )
			queue.append( blocks );
		else {
			for ( RopeBlock *rb = blocks.hblk; rb != 0; rb = rb->next )
				queue.append( blocks.data( rb ), blocks.length( rb ) );
		}
	}
	else {
		log_debug( DBG_PACKET, "packet send, sending blocks" );

		/* Not blocked on writing, attempt to write the rope. */
		RopeBlock *rb = blocks.hblk;
		while ( true ) {

			if ( rb == 0 )
				break;

			char *data = blocks.data(rb);
			int blockLen = blocks.length(rb);
			int res = writeFuncs->bufWrite( selectFd, data, blockLen );
			if ( res < 0 ) {
				log_debug( DBG_PACKET, "packet write: closed" );
				writeFuncs->bufClose( selectFd );
				break;
			}

			log_debug( DBG_PACKET, " -> sent " << res << " of " << ( blockLen ) << " bytes" );

			if ( res < blockLen ) {
				/* Didn't write a whole block. Put the remainder of the block
				 * and the message on the queue and go into wantWrite mode. */
				log_debug( DBG_PACKET, "failed to send full block, queueing " <<
						(blockLen-res) << " of " << blockLen );

				for ( RopeBlock *drb = blocks.hblk; drb != 0; drb = drb->next )
					log_debug( DBG_PACKET, "-> queueing " << blocks.length( drb ) << " bytes" );

				if ( canConsume ) {
					/* Move data from blocks to the output queue. */
					rb->hoff += res;
					blocks.ropeLen -= res;


					queue.append( blocks );
				}
				else {
					/* Copy data from blocks to output queue. */
					queue.append( data + res, blockLen - res);
					rb = rb->next;
					for ( ; rb != 0; rb = rb->next )
						queue.append( blocks.data(rb), blocks.length(rb) );
				}
				selectFd->wantWriteSet( true );
				break;
			}

			if ( canConsume ) {
				/* Consume the head block. */
				blocks.ropeLen -= res;
				blocks.hblk = blocks.hblk->next;
				if ( blocks.hblk == 0 )
					blocks.tblk = 0;
				delete[] (char*)rb;
				rb = blocks.hblk;
			}
			else {
				rb = rb->next;
			}

			log_debug( DBG_PACKET, "packet send result: " << res );
		}
	}
}

void WriteBuffer::sendEos( SelectFd *selectFd )
{
	if ( selectFd->wantWriteGet() ) {
		closeOnFlushed = true;
	}
	else {
		log_debug( DBG_PACKET, "packet send, sending blocks" );
		::close( selectFd->fd );
		selectFd->closed = true;
	}
}

void WriteBuffer::writeReady( SelectFd *selectFd )
{
	if ( queue.length() == 0 ) {
		/* Queue is now empty. We can exit wantWrite mode. */
		log_debug( DBG_PACKET, "write ready, queue empty, turning off want write" );
		selectFd->wantWriteSet( false );
		queue.empty();
	}
	else {
		/* Have data to send out. */
		log_debug( DBG_PACKET, "write ready, sending blocks" );

		while ( true ) {
			RopeBlock *rb = queue.hblk;

			if ( rb == 0 ) {
				/* Nothing left to send. If close on flush is set, then
				 * finished. */
				if ( closeOnFlushed ) {
					::close( selectFd->fd );
					selectFd->closed = true;
				}
				break;
			}

			char *data = queue.data(rb);
			int blockLen = queue.length(rb);
			int res = writeFuncs->bufWrite( selectFd, data, blockLen );
			if ( res < 0 ) {
				log_debug( DBG_PACKET, "packet write: closed" );
				writeFuncs->bufClose( selectFd );
				break;
			}

			log_debug( DBG_PACKET, " -> sent " << res << " of " << ( blockLen ) << " bytes" );

			if ( res < blockLen ) {
				/* Didn't send the entire remainder of the block. Increqment
				 * the queue head offset and wait for another write ready. */
				rb->hoff += res;
				queue.ropeLen -= res;
				break;
			}

			/* Consume the head block. */
			queue.ropeLen -= res;
			queue.hblk = queue.hblk->next;
			if ( queue.hblk == 0 )
				queue.tblk = 0;
			delete[] (char*)rb;
		}
	}
}

int Thread::createProcess( Process *proc )
{
	pid_t p;
	int pipe_stdin[2], pipe_stdout[2];

	if ( pipe( pipe_stdin ) )
		return -1;

	if ( pipe( pipe_stdout ) ) {
		close( pipe_stdin[0] );
		close( pipe_stdin[1] );
		return -1;
	}

	p = fork();
	if ( p < 0 ) {
		/* Fork failed */
		close( pipe_stdin[0] );
		close( pipe_stdin[1] );
		close( pipe_stdout[0] );
		close( pipe_stdout[1] );
		return p;
	}

	if ( p == 0 ) {
		/* child */
		close( pipe_stdin[1] );
		dup2( pipe_stdin[0], 0 );
		close( pipe_stdout[0] );
		dup2( pipe_stdout[1], 1 );
		execl( "/bin/sh", "sh", "-c", proc->cmdline, NULL );
		perror( "execl" );
		exit( 99 );
	}

	/* Parent. Close child ends. */
	close( pipe_stdin[0] );
	close( pipe_stdout[1] );

	proc->pid = p;
	proc->to.fd = pipe_stdin[1];
	proc->from.fd = pipe_stdout[0];

	proc->to.type = SelectFd::Process;
	proc->from.type = SelectFd::Process;

	proc->to.local = proc;
	proc->from.local = proc;

	selectFdList.append( &proc->to );
	selectFdList.append( &proc->from );

	return 0; 
}
