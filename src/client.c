#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>

#include "kring.h"

struct user_page
{
	char d[KRING_PAGE_SIZE];
};

int main()
{
	int s = socket( 25, SOCK_RAW, htons(ETH_P_ALL) );
	if ( s < 0 ) 
		printf( "socket failed: %d %s\n", s, strerror(errno) );

	{
		char *r = mmap( 0, KRING_CTRL_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, s, PGOFF_CTRL * KRING_PAGE_SIZE );
		printf( "ctrl %p %s\n", r, strerror(errno) );

		if ( r != MAP_FAILED ) {
			struct shared_ctrl *c = (struct shared_ctrl*)r;
			struct shared_desc *p = (struct shared_desc*)(r + sizeof(struct shared_ctrl));
			printf( "next-w: %lu\n", c->whead );

			printf( "c1: %hu\n", p[3].desc );
			printf( "c2: %hu\n", p[8].desc );
		}
	}

	{
		char *r = mmap( 0, KRING_DATA_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, s, PGOFF_DATA * KRING_PAGE_SIZE );
		printf( "data %p %s\n", r, strerror(errno) );

		if ( r != MAP_FAILED ) {
			struct user_page *p = (struct user_page*)r;

			printf( "d1: %d\n", (int)p[3].d[10] );
			printf( "d2: %d\n", (int)p[8].d[12] );
		}
	}

	return 0;
}
