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
	}

	/* start with the prefix. Always there (see above). */
	len = strlen(prefix);

	/* Maybe add errnostring. */
	errnostr = strerror(u->_errno);
	if ( errnostr )
		len += 2 + strlen(errnostr);

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

int kring_open( struct kring_user *u, enum KRING_TYPE type )
{
	void *r;

	memset( u, 0, sizeof(struct kring_user) );

	u->socket = socket( KRING, SOCK_RAW, htons(ETH_P_ALL) );
	if ( u->socket < 0 )
		goto err_socket;

	long unsigned typeoff = ( type == KRING_DECRYPTED ? 0x10000 : 0 );

	r = mmap( 0, KRING_CTRL_SZ, PROT_READ | PROT_WRITE,
			MAP_SHARED, u->socket,
			( typeoff | PGOFF_CTRL ) * KRING_PAGE_SIZE );

	if ( r == MAP_FAILED )
		goto err_mmap;

	u->shared.control = (struct shared_ctrl*)r;
	u->shared.descriptors = (struct shared_desc*)((char*)r + sizeof(struct shared_ctrl));

	r = mmap( 0, KRING_DATA_SZ, PROT_READ | PROT_WRITE,
			MAP_SHARED, u->socket,
			( typeoff | PGOFF_DATA ) * KRING_PAGE_SIZE );

	if ( r == MAP_FAILED )
		goto err_mmap;

	u->g = (struct kring_page*)r;

	u->shared.control->rhead = u->shared.control->whead;

	int desc = u->shared.descriptors[u->shared.control->rhead].desc;
	if ( desc & DSC_WRITER_OWNED ) {
		/* Fatal error. Writer should not own from prev write head. */
	}
	else {
		int newval = desc | DSC_READER_OWNED;
		int before = __sync_val_compare_and_swap( &u->shared.descriptors[u->shared.control->rhead].desc, desc, newval );
		if ( before != desc ) {
			/* writer got in, retry. */
		}
		else {
			/* Okay good. */
		}
	}


	return 0;

err_mmap:
	u->_errno = errno;
	close( u->socket );
	return KRING_ERR_MMAP;

err_socket:
	u->_errno = errno;
	return KRING_ERR_SOCK;
	
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

	h = (struct kring_decrypted_header*)( u->g + whead );
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
