#include "thread.h"
#include <stdlib.h>

std::ostream &operator <<( std::ostream &out, const Thread::endp & )
{
	exit( 1 );
}
