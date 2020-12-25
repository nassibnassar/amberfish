#ifndef _AF_SCAN_H
#define _AF_SCAN_H

#include "defs.h"

typedef struct {
	Uint2 dbid;
	int verbose;
} Afscan;

int afscan(const Afscan *rq);

#endif
