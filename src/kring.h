#ifndef __KRING_H
#define __KRING_H

#if defined(__cplusplus)
extern "C" {
#endif

#define KRING 25
#define NPAGES 2048
#define PGOFF_CTRL 1
#define PGOFF_DATA 2
#define KRING_PAGE_SIZE 4096

#define DSC_WRITER_OWNED    0x1
#define DSC_READER_OWNED    0x2
#define DSC_EITHER_OWNED    0x3

#define DSC_READER_SHIFT    2

#define KRING_ERR_SOCK -1
#define KRING_ERR_MMAP -2

/* Direction: from client, or from server. */
#define KRING_DIR_CLIENT 1
#define KRING_DIR_SERVER 2

#define KRING_DIR_INSIDE  1
#define KRING_DIR_OUTSIDE 2

enum KRING_TYPE
{
	KRING_PACKETS = 1,
	KRING_DECRYPTED
};

struct shared_ctrl
{
	unsigned long whead;
};

struct shared_desc
{
	unsigned short desc;
};

struct page_desc
{
	struct page *p;
	void *m;
};

struct kring_page
{
	char d[KRING_PAGE_SIZE];
};

#define KRING_CTRL_SZ sizeof(struct shared_ctrl) + sizeof(struct shared_desc) * NPAGES

#define KRING_DATA_SZ KRING_PAGE_SIZE * NPAGES

void kring_write( int rid, int dir, void *d, int len );

struct kring_user
{
	int socket;

	struct shared_ctrl *c;
	struct shared_desc *p;
	struct kring_page *g;
	unsigned long rhead;
	int _errno;
	char *errstr;
};

int kring_open( struct kring_user *u, enum KRING_TYPE type );

int kring_write_decrypted( struct kring_user *u, int type, const char *remoteHost, char *data, int len );
char *kring_error( struct kring_user *u, int err );

struct kring_packet
{
	char dir;
	int len;
	int caplen;
	unsigned char *bytes;
};

struct kring_decrypted
{
	 unsigned char type;
	 char *host;
	 int len;
	 unsigned char *bytes;
};

inline int kring_avail( struct kring_user *u )
{
	return ( u->rhead != u->c->whead );
}

inline void kring_next( struct kring_user *u )
{
	/* Next. */
	u->rhead = u->rhead + 1;
	if ( u->rhead >= NPAGES )
		u->rhead = 0;
}

inline void kring_next_packet( struct kring_user *u, struct kring_packet *packet )
{
	int *plen;
	char *pdir;
	unsigned char *bytes;

	kring_next( u );

	plen = (int*)( u->g + u->rhead );
	pdir = (char*)plen + sizeof(int);
	bytes = (unsigned char*)plen + sizeof(int) + 1;

	packet->dir = *pdir;
	packet->len = *plen;
	packet->caplen = *plen;
	packet->bytes = bytes;
}

inline void kring_next_decrypted( struct kring_user *u, struct kring_decrypted *decrypted )
{
	int *plen;
	unsigned char *ptype;
	unsigned char *phost;
	unsigned char *bytes;

	kring_next( u );

	plen = (int*)( u->g + u->rhead );
	ptype = (unsigned char*)( plen + 1 );
	phost = ptype + 1;
	bytes = ptype + 64;

	decrypted->type = *ptype;
	decrypted->host = (char*)phost;
	decrypted->len = *plen;
	decrypted->bytes = bytes;
}


inline unsigned long kring_one_back( unsigned long pos )
{
	return pos == 0 ? NPAGES - 1 : pos - 1;
}

inline unsigned long kring_one_forward( unsigned long pos )
{
	pos += 1;
	return pos == NPAGES ? 0 : pos;
}

#if defined(__cplusplus)
}
#endif

#endif
