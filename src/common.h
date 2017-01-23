#define KRING 25
#define NPAGES 2048
#define PGOFF_CTRL 1
#define PGOFF_DATA 2
#define KRING_PAGE_SIZE 4096

struct shared_desc
{
	int what;
};

struct user_page
{
	char d[4096];
};

