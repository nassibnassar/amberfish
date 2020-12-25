/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <stdio.h>
#include "err.h"

int aferrno = 0;

const char *afstrerror(int errnum)
{
	if (errnum > 0 && errnum <= AFMAXERR)
		return aferrmsg[errnum];
	else
		return "Unknown error";
}

int afperror(const char *string)
{
	if (string && *string)
		fprintf(stderr, "%s: ", string);
	fprintf(stderr, "%s\n", afstrerror(aferrno));
	return -1;
}
