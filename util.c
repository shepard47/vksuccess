#include "util.h"
#include <stdio.h>
#include <stdlib.h>

void
exits(char *s)
{
	puts(s);
	exit(-1);
}
