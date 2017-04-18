#include "kring.h"
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "common.c"

char *kring_error( struct kring_user *u, int err )
{
	int len;
	const char *prefix, *errnostr;

	prefix = "<unknown>";
	switch ( err ) {
		case KRING_ERR_SOCK:
			prefix = "socket call failed";
			break;
		case KRING_ERR_MMAP:
			prefix = "mmap call failed";
			break;
		case KRING_ERR_BIND:
			prefix = "bind call failed";
			break;
		case KRING_ERR_READER_ID:
			prefix = "getsockopt(reader_id) call failed";
			break;
		case KRING_ERR_RING_N:
			prefix = "getsockopt(ring_n) call failed";
			break;
		case KRING_ERR_ENTER:
			prefix = "exception in ring entry";
			break;
	}

	/* start with the prefix. Always there (see above). */
	len = strlen(prefix);

	/* Maybe add errnostring. */
	if ( u->_errno != 0 ) {
		errnostr = strerror(u->_errno);
		if ( errnostr )
			len += 2 + strlen(errnostr);
	}

	/* Null. */
	len += 1;

	u->errstr = malloc( len );
	strcpy( u->errstr, prefix );

	if ( errnostr != 0 ) {
		strcat( u->errstr, ": " );
		strcat( u->errstr, errnostr );
	}

	return u->errstr;
}

static unsigned long cons_pgoff( unsigned long ring_id, unsigned long region )
{
	return (
		( ( ring_id << PGOFF_ID_SHIFT )    & PGOFF_ID_MASK ) |
		( ( region << PGOFF_REGION_SHIFT ) & PGOFF_REGION_MASK )
	) * KRING_PAGE_SIZE;
}

static int kring_map_enter( struct kring_user *u, int ring_id, int ctrl )
{
	int res;
	void *r;

	r = mmap( 0, KRING_CTRL_SZ, PROT_READ | PROT_WRITE,
			MAP_SHARED, u->socket,
			cons_pgoff( ring_id, PGOFF_CTRL ) );

	if ( r == MAP_FAILED ) {
		kring_func_error( KRING_ERR_MMAP, errno );
		return -1;
	}

	u->control[ctrl].writer = (struct shared_writer*)r;
	u->control[ctrl].reader = (struct shared_reader*)( (char*)r + sizeof(struct shared_writer) );
	u->control[ctrl].descriptor = (struct shared_desc*)(
			(char*)r + sizeof(struct shared_writer) + sizeof(struct shared_reader) * KRING_READERS );

	r = mmap( 0, KRING_DATA_SZ, PROT_READ | PROT_WRITE,
			MAP_SHARED, u->socket,
			cons_pgoff( ring_id, PGOFF_DATA ) );

	if ( r == MAP_FAILED ) {
		kring_func_error( KRING_ERR_MMAP, errno );
		return -1;
	}

	u->data[ctrl].page = (struct kring_page*)r;

	if ( u->mode == KRING_READ ) {
		res = kring_prep_enter( u, ctrl );
		if ( res < 0 ) {
			kring_func_error( KRING_ERR_ENTER, 0 );
			return -1;
		}
	}

	return 0;
}

int kring_open( struct kring_user *u, enum KRING_TYPE type, const char *ringset, int ring_id, enum KRING_MODE mode )
{
	int ctrl, to_alloc, res, ring_N, reader_id;
	socklen_t nlen = sizeof(ring_N);
	socklen_t idlen = sizeof(reader_id);
	struct kring_addr addr;

	memset( u, 0, sizeof(struct kring_user) );

	u->socket = socket( KRING, SOCK_RAW, htons(ETH_P_ALL) );
	if ( u->socket < 0 ) {
		kring_func_error( KRING_ERR_SOCK, errno );
		goto err_return;
	}

	u->ring_id = ring_id;
	u->mode = mode;

	copy_name( addr.name, ringset );
	addr.ring_id = ring_id;
	addr.mode = mode;
	addr.writer_id = 0;

	res = bind( u->socket, (struct sockaddr*)&addr, sizeof(addr) );
	if ( res < 0 ) {
		kring_func_error( KRING_ERR_BIND, errno );
		goto err_close;
	}

	/* Get the number of rings in the ringset. */
	res = getsockopt( u->socket, SOL_PACKET, KR_OPT_RING_N, &ring_N, &nlen );
	if ( res < 0 ) {
		kring_func_error( KRING_ERR_RING_N, errno );
		goto err_close;
	}
	u->nrings = ring_N;

	/* Get the reader id we were assigned. */
	res = getsockopt( u->socket, SOL_PACKET, KR_OPT_READER_ID, &reader_id, &idlen );
	if ( res < 0 ) {
		kring_func_error( KRING_ERR_READER_ID, errno );
		goto err_close;
	}
	u->reader_id = reader_id;

	/*
	 * Allocate ring-specific structs. May not use them all.
	 */
	to_alloc = 1;
	if ( ring_id == KR_RING_ID_ALL )
		to_alloc = ring_N;

	u->control = (struct kring_control*)malloc( sizeof( struct kring_control ) * to_alloc );
	memset( u->control, 0, sizeof( struct kring_control ) * to_alloc );

	u->data = (struct kring_data*)malloc( sizeof( struct kring_data ) * to_alloc );
	memset( u->data, 0, sizeof( struct kring_data ) * to_alloc );

	/* Which rings to map. */
	if ( ring_id != KR_RING_ID_ALL ) {
		res = kring_map_enter( u, ring_id, 0 );
		if ( res < 0 )
			goto err_close;
	}
	else {
		for ( ctrl = 0; ctrl < ring_N; ctrl++ ) {
			res = kring_map_enter( u, ctrl, ctrl );
			if ( res < 0 )
				goto err_close;
		}
	}

	return 0;

err_close:
	close( u->socket );
err_return:
	return u->krerr;
}

/*
 * NOTE: when open for writing we always are writing to a specific ring id. No
 * need to iterate over control and data or dereference control/data pointers.
 */
int kring_write_decrypted( struct kring_user *u, long id, int type, const char *remoteHost, char *data, int len )
{
	struct kring_decrypted_header *h;
	unsigned char *bytes;
	shr_off_t whead;
	char buf[1];

	if ( len > kring_decrypted_max_data()  )
		len = kring_decrypted_max_data();

	/* Find the place to write to, skipping ahead as necessary. */
	whead = find_write_loc( u->control );

	/* Reserve the space. */
	u->control->writer->wresv = whead;

	h = (struct kring_decrypted_header*)( u->data->page + whead );
	bytes = (unsigned char*)( h + 1 );

	h->len = len;
	h->id = id;
	h->type = type;
	if ( remoteHost == 0 )
		h->host[0] = 0;
	else {
		strncpy( h->host, remoteHost, sizeof(h->host) );
		h->host[sizeof(h->host)-1] = 0;
	}   

	memcpy( bytes, data, len );

	/* Clear the writer owned bit from the buffer. */
	writer_release( u->control, whead );

	/* Write back the write head, thereby releasing the buffer to writer. */
	u->control->writer->whead = whead;

	/* Wake up here. */
	send( u->socket, buf, 1, 0 );

	return 0;
}   

void kring_lock( int *mutex, unsigned long long *spins )
{
	while ( __sync_lock_test_and_set( mutex, 1 ) ) {
		/* If builtin returns 1 we did not flip it and therefore did not acquire the lock. */
		__sync_add_and_fetch( spins, 1 );
	}
}

void kring_unlock( int *mutex )
{
	__sync_lock_release( mutex, 0 );
}

/*
 * NOTE: when open for writing we always are writing to a specific ring id. No
 * need to iterate over control and data or dereference control/data pointers.
 */
int kring_write_plain( struct kring_user *u, char *data, int len )
{
	struct kring_plain_header *h;
	unsigned char *bytes;
	shr_off_t whead;
	char buf[1];

	if ( len > kring_plain_max_data()  )
		len = kring_plain_max_data();

	kring_lock( &u->control->writer->write_mutex, &u->control->writer->spins );

	/* Find the place to write to, skipping ahead as necessary. */
	whead = find_write_loc( u->control );

	/* Reserve the space. */
	u->control->writer->wresv = whead;

	h = (struct kring_plain_header*)( u->data->page + whead );
	bytes = (unsigned char*)( h + 1 );

	h->len = len;
	memcpy( bytes, data, len );

	/* Clear the writer owned bit from the buffer. */
	writer_release( u->control, whead );

	/* Write back the write head, thereby releasing the buffer to writer. */
	u->control->writer->whead = whead;

	kring_unlock( &u->control->writer->write_mutex );

	/* Wake up here. */
	send( u->socket, buf, 1, 0 );

	return 0;
}   

int kring_read_wait( struct kring_user *u )
{
	char buf[1];
	int ret = recv( u->socket, buf, 1, 1 ); 
	if ( ret == -1 && errno == EINTR )
		ret = 0;
	return ret;
}
