#ifndef __KRDEP_H
#define __KRDEP_H

/* MUST match system page size. */
#define KRING_PAGE_SIZE 4096

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

#endif
