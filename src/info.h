#ifndef _AF_INFO_H
#define _AF_INFO_H

#include "config.h"

/* size of buffer to store version stamp in db info file */
#define Afverstampn (256)

/* located right after db version number in db info file */
typedef struct {
	char version_stamp[Afverstampn];
	int doc_n; /* total number of (non-deleted) documents in
		      database */
	Uint4 udict_root; /* root of the udict tree */
	int optimized; /* flag: if database is optimized */
	int phrase; /* flag: if database supports phrase searching */
	int word_proximity; /* flag: if database supports word proximity */
	int stemming; /* flag: if words are stemmed */
} Dbinfo;

int afdbver(FILE *info);
int afreadinfo(FILE *f, Dbinfo *info);
int afwriteinfo(FILE *f, Dbinfo *info);

/* old */

#define ETYMON_MAX_VSTAMP_SIZE Afverstampn

typedef Dbinfo ETYMON_DB_INFO;

#endif
