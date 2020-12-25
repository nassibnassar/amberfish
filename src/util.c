/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

/* new */

#include <string.h>
#include "util.h"

FILE *afopendbf(const char *db, int type, const char *mode)
{
	char fn[ETYMON_MAX_PATH_SIZE];
	FILE *f;

	if (afmakefn(type, db, fn) < 0)
		return NULL;
	f = fopen(fn, mode);
	return f ? f : aferrn(AFEDBIO);
}

static inline int closefile(FILE *f)
{
	if (f) {
		if (fclose(f) == EOF)
			return aferr(AFEDBIO);
	}
	return 0;
}

int afclosedbf(Affile *f)
{
	if (closefile(f->info) < 0)
		return -1;
	if (closefile(f->doctab) < 0)
		return -1;
	if (closefile(f->udict) < 0)
		return -1;
	if (closefile(f->upost) < 0)
		return -1;
	if (closefile(f->ufield) < 0)
		return -1;
	if (closefile(f->lpost) < 0)
		return -1;
	if (closefile(f->lfield) < 0)
		return -1;
	if (closefile(f->fdef) < 0)
		return -1;
	if (closefile(f->uword) < 0)
		return -1;
	if (closefile(f->lword) < 0)
		return -1;
	if (closefile(f->lock) < 0)
		return -1;
	return 0;
}

static inline const char *getftfn(int type)
{
	if (type < sizeof affntab)
		return affntab[type];
	else
		return aferrn(AFEINVAL);
}

/* buf is assumed to have capacity ETYMON_MAX_PATH_SIZE */
int afmakefn(int type, const char *db, char *buf)
{
	int dblen, extlen;
	const char *ext;

	dblen = strlen(db);
	memcpy(buf, db, dblen);
	buf += dblen;
	ext = getftfn(type);
	if (!ext)
		return -1;
	extlen = strlen(ext);
	if ((dblen + extlen) >= ETYMON_MAX_PATH_SIZE)
		return aferr(AFEBUFOVER);
	memcpy(buf, ext, extlen + 1);
	return 0;
}

/*
int afgetfsize_nonansi(FILE *f, off_t *size)
{
	struct stat st;
	
	if (fstat(fileno(f), &st) < 0)
		return aferr(AFEDBIO);
	*size = st.st_size;
	return 0;
}
*/

int afgetfsize(FILE *f, off_t *size)
{
	off_t save;

	if ((save = ftello(f)) < 0)
		return aferr(AFEDBIO);
	if (fseeko(f, 0, SEEK_END) < 0)
		return aferr(AFEDBIO);
	if ((*size = ftello(f)) < 0)
		return aferr(AFEDBIO);
	if (fseeko(f, save, SEEK_SET) < 0)
		return aferr(AFEDBIO);
	return 0;
}

void afprintvp(int verbose, int minimum)
{
	int x;
	
	for (x = 2; x < minimum; x++)
		printf("+");
	if (minimum > 2)
		printf(" ");
}

void afprintv(int verbose, int minimum, const char *msg)
{
	if (verbose >= minimum) {
		afprintvp(verbose, minimum);
		printf("%s\n", msg);
	}
}

/* old */
	
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ETYMON_UTIL_MAX_ARG_SIZE 4096

/**
   Constructs a path name using a dbname as the stem and appending an
   extension depending on the type of file requested.

   @param ftype the type of file (e.g. ETYMON_DB_INFO).
   @param dbname the database name to use as a stem.
   @param buf the buffer to construct the path in (assumed to have
   capacity ETYMON_MAX_PATH_SIZE)
*/
void etymon_db_construct_path(int ftype, const char* dbname, char* buf) {
	int leftover;  /* extra space in buf after dbname is copied in */
	/* start with dbname as the stem */
	strncpy(buf, dbname, ETYMON_MAX_PATH_SIZE - 1);
	buf[ETYMON_MAX_PATH_SIZE - 1] = '\0';
	leftover = ETYMON_MAX_PATH_SIZE - strlen(buf) - 1;
	/* now add the proper extension */
	switch (ftype) {
	case ETYMON_DBF_INFO:
		strncat(buf, ETYMON_DBF_INFO_EXT, leftover);
		break;
	case ETYMON_DBF_DOCTABLE:
		strncat(buf, ETYMON_DBF_DOCTABLE_EXT, leftover);
		break;
	case ETYMON_DBF_UDICT:
		strncat(buf, ETYMON_DBF_UDICT_EXT, leftover);
		break;
	case ETYMON_DBF_UPOST:
		strncat(buf, ETYMON_DBF_UPOST_EXT, leftover);
		break;
	case ETYMON_DBF_UFIELD:
		strncat(buf, ETYMON_DBF_UFIELD_EXT, leftover);
		break;
	case ETYMON_DBF_LPOST:
		strncat(buf, ETYMON_DBF_LPOST_EXT, leftover);
		break;
	case ETYMON_DBF_LFIELD:
		strncat(buf, ETYMON_DBF_LFIELD_EXT, leftover);
		break;
	case ETYMON_DBF_LOCK:
		strncat(buf, ETYMON_DBF_LOCK_EXT, leftover);
		break;
	case ETYMON_DBF_FDEF:
		strncat(buf, ETYMON_DBF_FDEF_EXT, leftover);
		break;
	case ETYMON_DBF_UWORD:
		strncat(buf, ETYMON_DBF_UWORD_EXT, leftover);
		break;
	case ETYMON_DBF_LWORD:
		strncat(buf, ETYMON_DBF_LWORD_EXT, leftover);
		break;
	default:
		break;
	}
}


void etymon_tolower(char* s) {
	char* p = s;

	while (*p != '\0') {
		*p = tolower(*p);
		p++;
	}
}


char*** etymon_split_options(int argc, char *argv[]) {
	/*
	  we're going to create a 2-dimensional array that looks something like this:
	  { { "-d", "dbname" },
	    { "-f", "file1", "file2", "file3" } }
	  to do that we need to first figure out which argv[] are options flags.
	  the starts array keeps track of the start of each option group
	*/
	int* starts;
	int starts_count;
	int r;
	char*** options;
	starts = (int*)(malloc(sizeof(int) * argc));
	starts_count = 0;
	for (r = 1; r < argc; r++) {
		/* check if it's an option flag */
		if (argv[r][0] == '-') {
			if ( (argv[r][1] == '\0') || (argv[r][1] != '-') ) {
				/* yes, it's an option flag */
				starts[starts_count++] = r;
			}
		}
	}
	/* in the last element of starts we store the next (imaginary) start number */
	starts[starts_count] = argc;
	/* now the starts array contains a list of the indicies of argv[] which contain options flags
	   so we we next build the options array based on this information */
	options = (char***)(malloc(sizeof(char**) * (starts_count + 1)));
	for (r = 0; r < starts_count; r++) {
		/* size is the number of columns in this row of the array */
		int size = starts[r+1] - starts[r];
		int c;
		options[r] = (char**)(malloc(sizeof(char*) * (size + 1)));
		for (c = 0; c < size; c++) {
			char* s = argv[starts[r] + c];
			/* convert leading "--" to "-" in option arguments */
			if ( (c > 0) && (strncmp(s, "--", 2) == 0) ) {
				options[r][c] = (char*)(malloc(strlen(s)));
				strcpy(options[r][c], s + 1);
			} else {
				options[r][c] = (char*)(malloc(strlen(s) + 1));
				strcpy(options[r][c], s);
			}
		}
		options[r][size] = NULL;
	}
	options[starts_count] = NULL;
	free(starts);
	return options;
}


void etymon_doctable_getinfo(char* dbname, ETYMON_DOCTABLE* doctable) {
	char fn[ETYMON_MAX_PATH_SIZE];
	int fd;

	/* need to check for buffer overrun */
	etymon_db_construct_path(ETYMON_DBF_DOCTABLE, dbname, fn);
	fd = open(fn, O_RDONLY | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	/* need to check for missing database and transient I/O failures */
	if (read(fd, doctable, sizeof(ETYMON_DOCTABLE)) == -1) {
		perror("doctable_getinfo():read()");
	}
	close(fd);
}
