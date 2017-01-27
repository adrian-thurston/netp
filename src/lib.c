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

int kring_open( struct kring_user *u, enum KRING_TYPE type )
{
	void *r;

	memset( u, 0, sizeof(struct kring_user) );

	u->socket = socket( 25, SOCK_RAW, htons(ETH_P_ALL) );
	if ( u->socket < 0 )
		goto err_socket;

	long unsigned typeoff = ( type == KRING_DECRYPTED ? 0x10000 : 0 );

	r = mmap( 0, KRING_CTRL_SZ, PROT_READ | PROT_WRITE,
			MAP_SHARED, u->socket,
			( typeoff | PGOFF_CTRL ) * KRING_PAGE_SIZE );

	if ( r == MAP_FAILED )
		goto err_mmap;

	u->c = (struct shared_ctrl*)r;
	u->p = (struct shared_desc*)((char*)r + sizeof(struct shared_ctrl));

	r = mmap( 0, KRING_DATA_SZ, PROT_READ | PROT_WRITE,
			MAP_SHARED, u->socket,
			( typeoff | PGOFF_DATA ) * KRING_PAGE_SIZE );

	if ( r == MAP_FAILED )
		goto err_mmap;

	u->g = (struct kring_page*)r;

	u->rhead = u->c->whead;

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
	if ( (unsigned)len > (KRING_PAGE_SIZE - sizeof(int) - 64) )
		len = KRING_PAGE_SIZE - sizeof(int) - 64;

	int *plen = (int*)( u->g + u->c->whead );
	char *ptype = (char*)plen + sizeof(int);
	char *phost = ptype + 1;
	char *pdata = ptype + 64;

	*plen = len;
	*ptype = type;
	if ( remoteHost == 0 )
		phost[0] = 0;
	else {
		strcpy( phost, remoteHost );
		phost[62] = 0;
	}   

	memcpy( pdata, data, len );

	u->c->whead += 1;
	if ( u->c->whead >= NPAGES )
		u->c->whead = 0;




	return 0;
}   
