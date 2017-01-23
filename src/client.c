#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>

#include "common.h"

int main()
{
	int s, sz;
	char *r;
	
	s = socket( 25, SOCK_RAW, htons(ETH_P_ALL) );
	if ( s < 0 ) 
		printf( "socket failed: %d %s\n", s, strerror(errno) );

	{
		sz = sizeof( struct shared_desc ) * NPAGES;
		printf( "ctrl size: %d\n", sz );
		r = mmap( 0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, s, PGOFF_CTRL * KRING_PAGE_SIZE );
		printf( "ctrl %p %s\n", r, strerror(errno) );

		if ( r != MAP_FAILED ) {
			struct shared_desc *p = (struct shared_desc*)r;
			printf( "c1: %d\n", (int)p[3].what );
			printf( "c2: %d\n", (int)p[8].what );
		}
	}

	{
		sz = sizeof(struct user_page) * NPAGES;
		printf( "data size: %d\n", sz );
		r = mmap( 0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, s, PGOFF_DATA * KRING_PAGE_SIZE );
		printf( "data %p %s\n", r, strerror(errno) );

		if ( r != MAP_FAILED ) {
			struct user_page *p = (struct user_page*)r;
			printf( "d1: %d\n", (int)p[3].d[10] );
			printf( "d2: %d\n", (int)p[8].d[12] );
			p[3].d[10] = 3 * 10;
			p[8].d[12] = 8 * 12;
		}
	}

	return 0;
}
