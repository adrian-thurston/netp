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
		case KRING_ERR_GETID:
			prefix = "getsockopt(id) call failed";
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

static unsigned long cons_pgoff( unsigned long rid, unsigned long region )
{
	return (
		( ( rid    << PGOFF_ID_SHIFT )     & PGOFF_ID_MASK ) |
		( ( region << PGOFF_REGION_SHIFT ) & PGOFF_REGION_MASK )
	) * KRING_PAGE_SIZE;
}

int kring_open( struct kring_user *u, enum KRING_TYPE type, const char *ringset, int rid, enum KRING_MODE mode )
{
	int res, id;
	socklen_t idlen = sizeof(id);
	void *r;
	struct kring_addr addr;

	memset( u, 0, sizeof(struct kring_user) );

	u->socket = socket( KRING, SOCK_RAW, htons(ETH_P_ALL) );
	if ( u->socket < 0 ) {
		kring_func_error( KRING_ERR_SOCK, errno );
		goto err_return;
	}

	copy_name( addr.name, ringset );
	addr.mode = mode;

	res = bind( u->socket, (struct sockaddr*)&addr, sizeof(addr) );
	if ( res < 0 ) 
		goto err_close;

	res = getsockopt( u->socket, SOL_PACKET, 1, &id, &idlen );
	if ( res < 0 ) {
		kring_func_error( KRING_ERR_GETID, errno );
		goto err_close;
	}

	u->id = id;

	r = mmap( 0, KRING_CTRL_SZ, PROT_READ | PROT_WRITE,
			MAP_SHARED, u->socket,
			cons_pgoff( rid, PGOFF_CTRL ) );

	if ( r == MAP_FAILED ) {
		kring_func_error( KRING_ERR_MMAP, errno );
		goto err_close;
	}

	u->shared.control = (struct shared_ctrl*)r;
	u->shared.reader = (struct shared_reader*)( (char*)r + sizeof(struct shared_ctrl) );
	u->shared.descriptor = (struct shared_desc*)( (char*)r + sizeof(struct shared_ctrl) + sizeof(struct shared_reader) * NRING_READERS );

	r = mmap( 0, KRING_DATA_SZ, PROT_READ | PROT_WRITE,
			MAP_SHARED, u->socket,
			cons_pgoff( rid, PGOFF_DATA ) );

	if ( r == MAP_FAILED ) {
		kring_func_error( KRING_ERR_MMAP, errno );
		goto err_close;
	}

	u->data = (struct kring_page*)r;

	res = kring_enter( u );
	if ( res < 0 ) {
		kring_func_error( KRING_ERR_ENTER, 0 );
		goto err_close;
	}

	return 0;

err_close:
	close( u->socket );
err_return:
	return u->krerr;
}

int kring_write_decrypted( struct kring_user *u, int type, const char *remoteHost, char *data, int len )
{
	struct kring_decrypted_header *h;
	unsigned char *bytes;
	shr_off_t whead;
	char buf[1];

	if ( (unsigned)len > (KRING_PAGE_SIZE - sizeof(struct kring_decrypted_header) ) )
		len = KRING_PAGE_SIZE - sizeof(struct kring_decrypted_header);

	/* Find the place to write to, skipping ahead as necessary. */
	whead = find_write_loc( &u->shared );

	/* Reserve the space. */
	u->shared.control->wresv = whead;

	h = (struct kring_decrypted_header*)( u->data + whead );
	bytes = (unsigned char*)( h + 1 );

	h->len = len;
	h->type = type;
	if ( remoteHost == 0 )
		h->host[0] = 0;
	else {
		strncpy( h->host, remoteHost, sizeof(h->host) );
		h->host[sizeof(h->host)-1] = 0;
	}   

	memcpy( bytes, data, len );

	/* Clear the writer owned bit from the buffer. */
	writer_release( &u->shared, whead );

	/* Write back the write head, thereby releasing the buffer to writer. */
	u->shared.control->whead = whead;

	/* Wake up here. */
	send( u->socket, buf, 1, 0 );

	return 0;
}   
