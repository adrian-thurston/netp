
/* Requires appropriate header includes in the file that includes this file. */

static void kdata_copy_name( char *dest, const char *src )
{
	strncpy( dest, src, KRING_NLEN );
	dest[KRING_NLEN-1] = 0;
}
