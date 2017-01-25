#define KRING 25
#define NPAGES 2048
#define PGOFF_CTRL 1
#define PGOFF_DATA 2
#define KRING_PAGE_SIZE 4096

#define DSC_WRITER_OWNED    0x1
#define DSC_READER_OWNED    0x2
#define DSC_READER_SHIFT    2

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

#define KRING_CTRL_SZ sizeof(struct shared_ctrl) + sizeof(struct shared_desc) * NPAGES

#define KRING_DATA_SZ KRING_PAGE_SIZE * NPAGES

void kring_write( int rid, void *d, int len );
