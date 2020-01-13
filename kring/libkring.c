#include "kring.h"

#include <string.h>

void kring_copy_name( char *dest, const char *src )
{
	strncpy( dest, src, KRING_NLEN );
	dest[KRING_NLEN-1] = 0;
}

