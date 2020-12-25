#ifndef _AF_ERR_H
#define _AF_ERR_H

#include <stdlib.h>
#include "config.h"

extern int aferrno;

/*
 *  The first group of errors are designated as "default errors" and
 *  can be returned by almost any public function.
 */
#define AFEUNKNOWN      (1)   /* Unknown internal error */
#define AFEMEM          (2)   /* Out of memory (failed malloc) */
#define AFEBUFOVER      (3)   /* Buffer overflow (usually char[]) */
#define AFEINVAL        (4)   /* Invalid argument */
#define	AFEDBIO         (5)   /* Database file I/O error */
#define	AFEBADDB        (6)   /* Corrupted database file */
/*
 *  The following errors are only returned by certain functions.
 */
#define	AFEDBLOCK       (7)   /* Database locked */
#define	AFEVERSION      (8)   /* Incompatible database version */
#define	AFELINEAR       (9)   /* Unsupported operation on linearized database */
#define	AFEOPENLIM     (10)   /* Too many open databases */
#define AFETERMLEN     (11)   /* Query term too long */
#define AFEQUERYNEST   (12)   /* Query too deeply nested */
#define AFEQUERYSYN    (13)   /* Syntax error in query */
#define AFENOSTEM      (14)   /* Database requires stemming support */

#define AFMAXCOREERR    (6)
#define AFMAXERR       (14)

static const char* aferrmsg[] = {
	"",
	"Unknown error",
	"Out of memory",
	"Buffer overflow",
	"Invalid argument",
	"Database file I/O error",
	"Corrupted database file",
	"Database locked",
	"Incompatible database version",
	"Unsupported operation on linearized database",
	"Too many open databases",
	"Query term too long",
	"Query too deeply nested",
	"Syntax error in query",
	"Database requires stemming support"
};

static inline int aferr(int err)
{
	aferrno = err;
	return -1;
}

static inline void *aferrn(int err)
{
	aferrno = err;
	return NULL;
}

const char *afstrerror(int errnum);
int afperror(const char *string);

#endif
