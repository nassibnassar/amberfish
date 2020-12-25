#ifndef _AF_UTIL_H
#define _AF_UTIL_H

/* new */

#include "defs.h"

typedef Uint4 Afdbver_t;

FILE *afopendbf(const char *db, int type, const char *mode);
int afclosedbf(Affile *f);
int afmakefn(int type, const char *db, char *buf);
int afgetfsize(FILE *f, off_t *size);
void afprintvp(int verbose, int minimum);
void afprintv(int verbose, int minimum, const char *msg);

/* old */

void etymon_db_construct_path(int ftype, const char* dbname, char* buf);
void etymon_tolower(char* s);
char*** etymon_split_options(int argc, char *argv[]);

#endif
