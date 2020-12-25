/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <stdio.h>
#include "info.h"
#include "util.h"
#include "err.h"
#include "defs.h"

/* assumes we are positioned at the beginning of the info file */
int afdbver(FILE *f)
{
	int n;
	Afdbver_t v;

	if ((n = fread(&v, 1, sizeof v, f)) != sizeof v)
		return aferr(AFEBADDB);
	return (v == ETYMON_INDEX_MAGIC) ? (int) v : aferr(AFEVERSION);
}

int afreadinfo(FILE *f, Dbinfo *info)
{
	int n;
	
	if (afdbver(f) < 0)
		return -1;
	n = fread(info, 1, sizeof (Dbinfo), f);
	return (n == sizeof (Dbinfo)) ? 0 : aferr(AFEBADDB);
}

int afwriteinfo(FILE *f, Dbinfo *info)
{
	Uint4 magic;
	
	if (fseeko(f, 0, SEEK_SET) < 0)
		return aferr(AFEDBIO);
	magic = ETYMON_INDEX_MAGIC;
	if (fwrite(&magic, 1, sizeof magic, f) < sizeof magic)
		return aferr(AFEDBIO);
	if (fwrite(info, 1, sizeof (ETYMON_DB_INFO), f) < sizeof (ETYMON_DB_INFO))
		return aferr(AFEDBIO);

	return 0;
}
