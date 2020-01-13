#ifndef __KRDEP_H
#define __KRDEP_H

/* MUST match system page size. */
#define KRING_PAGE_SIZE 4096

#define KRING_RING_ID_ALL -1

struct kring_page_desc
{
	struct page *p;
	void *m;
};

struct kring_page
{
	char d[KRING_PAGE_SIZE];
};

struct kring_data
{
	struct kring_page *page;
};

struct kring_control
{
	void *head;
	void *writer;
	void *reader;
	void *descriptor;
};

enum KRING_PROTO
{
	KRING_PACKETS = 1,
	KRING_DECRYPTED,
	KRING_PLAIN
};

enum KRING_TYPE
{
	KRING_DATA = 1,
	KRING_CTRL
};

enum KRING_MODE
{
	KRING_READ = 1,
	KRING_WRITE
};

struct kring_user
{
	int socket;
	int ring_id;
	int nrings;
	int writer_id;
	int reader_id;
	enum KRING_MODE mode;

	/* If reading from multiple rings then this can be an array. */
	struct kring_control *control;

	/* When used in user space we use the data pointer, which points to the
	 * mmapped region. In the kernel we use pd, which points to the array of
	 * (pages + memory pointers. Must be interpreted according to socket value. */
	struct kring_data *data;

	struct kring_page_desc *pd;

	int krerr;
	int _errno;
	char *errstr;
};


#endif
