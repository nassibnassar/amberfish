/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <stdlib.h>
#include <stdio.h>
#include "index.h"
#include "lock.h"
#include "util.h"
#include "fdef.h"
#include "stem.h"
#include "info.h"
#include "linear.h"

#include "text.h"
#include "xml.h"
#include "xml_test.h"
#include "erc.h"

#include "open.h"
extern ETYMON_AF_STATE *etymon_af_state[];

/* assumes that the buffer, absolute_path, is of size
   ETYMON_MAX_PATH_SIZE, that relative_path contains a valid
   null-terminated string, that cwd has been filled in using getcwd(),
   and that relative_path != absolute_path */
void etymon_index_expand_path(char* relative_path, char* absolute_path, char* cwd) {
	/* check if it's already an absolute path */
	if (*relative_path == '/') {
		strncpy(absolute_path, relative_path, ETYMON_MAX_PATH_SIZE - 1);
		absolute_path[ETYMON_MAX_PATH_SIZE - 1] = '\0';
		return;
	} else {
		char* r_p;
		char* slash_p;
		char* p;
		int x, y, seg_len;
		
		/* otherwise combine cwd and relative_path to get the absolute path */

		/* start off with the cwd */
		strcpy(absolute_path, cwd);

		/* now take apart relative_path */
		r_p = relative_path;
		while ( (slash_p = strchr(r_p, '/')) != NULL ) {
			/* now r_p points to the beginning and slash_p to the next '/' */
			seg_len = slash_p - r_p;
			/* check if it's "../" */
			if ( (seg_len == 2) && (strncmp(r_p, "..", 2) == 0) ) {
				/* remove the last segment from absolute_path */
				p = strrchr(absolute_path, '/');
				if (p == NULL) {
					/* ERROR and return */
				}
				*p = '\0';
			}
			/* check if it's "./" */
			else if ( (seg_len == 1) && (*r_p == '.') ) {
				/* do nothing */
			}
			else {
				/* append the segment to the end of absolute_path */
				x = strlen(absolute_path);
				if ((ETYMON_MAX_PATH_SIZE - x) >= (seg_len + 2)) {
					absolute_path[x++] = '/';
					memcpy(absolute_path + x, r_p, seg_len);
					absolute_path[x + seg_len] = '\0';
				}
			}
			r_p = slash_p + 1;
		}
		x = strlen(absolute_path);
		y = strlen(r_p);
		if ((ETYMON_MAX_PATH_SIZE - x) >= (y + 2)) {
			absolute_path[x] = '/';
			memcpy(absolute_path + x + 1, r_p, y + 1);
		}
	}
}

#ifdef ZZZZZ

/*#define OPT_STDIO*/

#ifdef OPT_STDIO

/* this was written before the advent of ETYMON_INDEX_PAGE_L.post_n[],
   ETYMON_INDEX_UPOST.fields_n, and ETYMON_INDEX_UPOST.word_numbers_n
   in the first unoptimized pass; so it explicitly counts these values
   while building the optimized structures */
int etymon_index_optimize_old_stdio(ETYMON_INDEX_OPTIONS* opt) {
	int dbinfo_fd, udict_fd, upost_fd, ufield_fd, uword_fd, lpost_fd, lfield_fd, lword_fd;
	FILE* dbinfo_f;
	FILE* udict_f;
	FILE* upost_f;
	FILE* ufield_f;
	FILE* uword_f;
	FILE* lpost_f;
	FILE* lfield_f;
	FILE* lword_f;
	int x;
	etymon_af_off_t udict_size, upost_isize, ufield_isize, uword_isize, lpost_isize, lfield_isize, lword_isize;
	Uint4 magic;
	ETYMON_DB_INFO dbinfo;
	char fn[ETYMON_MAX_PATH_SIZE];
	ETYMON_AF_STAT st;
	ssize_t nbytes;
	Uint4 udict_p, upost_p, lpost_p_save;
	Uint4 ufield_p, lfield_p_save, field_count;
	Uint4 uword_p, lword_p_save, word_count;
	ETYMON_INDEX_PAGE_L page_l;
	ETYMON_INDEX_PAGE_NL page_nl;
	Uint1 leaf_flag;
	ETYMON_INDEX_UPOST upost;
	ETYMON_INDEX_LPOST lpost;
	ETYMON_INDEX_UFIELD ufield;
	ETYMON_INDEX_LFIELD lfield;
	ETYMON_INDEX_UWORD uword;
	ETYMON_INDEX_LWORD lword;

	/* make sure database is ready */
	if (etymon_db_ready(opt->dbname, &(opt->log)) == 0) {
		int e;
		char s[ETYMON_MAX_MSG_SIZE];
		sprintf(s, "%s: Database not ready", opt->dbname);
		e = opt->log.error(s, 1);
		if (e != 0) {
			exit(e);
		}
		return -1;
	}
	
	/* lock the database */
	etymon_db_lock(opt->dbname, &(opt->log));
	
	/* open db info file for read/write */
	etymon_db_construct_path(ETYMON_DBF_INFO, opt->dbname, fn);
	dbinfo_fd = open(fn, O_RDWR | ETYMON_AF_O_LARGEFILE);
	if (dbinfo_fd == -1) {
		int e;
		char s[ETYMON_MAX_MSG_SIZE];
		sprintf(s, "%s: Unable to open database", opt->dbname);
		e = opt->log.error(s, 1);
		etymon_db_unlock(opt->dbname, &(opt->log));
		if (e != 0) {
			exit(e);
		}
		return -1;
	}
	nbytes = read(dbinfo_fd, &magic, sizeof(Uint4));
	if (nbytes != sizeof(Uint4)) {
		/* ERROR */
		printf("unable to read %s\n", fn);
		exit(1);
	}
	if (magic != ETYMON_INDEX_MAGIC) {
		int e;
		char s[ETYMON_MAX_MSG_SIZE];
		sprintf(s, "%s: Database created by incompatible version", opt->dbname);
		e = opt->log.error(s, 1);
		close(dbinfo_fd);
		etymon_db_unlock(opt->dbname, &(opt->log));
		if (e != 0) {
			exit(e);
		}
		return -1;
	}
	nbytes = read(dbinfo_fd, &dbinfo, sizeof(ETYMON_DB_INFO));
	if (nbytes != sizeof(ETYMON_DB_INFO)) {
		/* ERROR */
		printf("unable to read %s\n", fn);
		exit(1);
	}
	dbinfo_f = fdopen(dbinfo_fd, "r+b");
	
	/* make sure the database is not already optimized */
	if (dbinfo.optimized == 1) {
		int e;
		char s[ETYMON_MAX_MSG_SIZE];
		sprintf(s, "%s: Database is already linearized", opt->dbname);
		e = opt->log.error(s, 1);
		fclose(dbinfo_f);
		close(dbinfo_fd);
		etymon_db_unlock(opt->dbname, &(opt->log)); /* unlock the database */
		if (e != 0) {
			exit(e);
		}
		return -1;
	}

	/* open files */
	
	/* open udict for read/write */
	etymon_db_construct_path(ETYMON_DBF_UDICT, opt->dbname, fn);
	udict_fd = open(fn, O_RDWR | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (udict_fd == -1) {
		/* ERROR */
		printf("unable to open %s for read/write\n", fn);
		exit(1);
	}
	/* stat udict to get size */
	if (etymon_af_fstat(udict_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	udict_size = st.st_size;
	udict_f = fdopen(udict_fd, "r+b");
	
	/* open upost for read */
	etymon_db_construct_path(ETYMON_DBF_UPOST, opt->dbname, fn);
	upost_fd = open(fn, O_RDONLY | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (upost_fd == -1) {
		/* ERROR */
		printf("unable to open %s for read\n", fn);
		exit(1);
	}
	/* stat upost to get size */
	if (etymon_af_fstat(upost_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	upost_isize = st.st_size / sizeof(ETYMON_INDEX_UPOST);
	upost_f = fdopen(upost_fd, "rb");
	
	/* open ufield for read */
	etymon_db_construct_path(ETYMON_DBF_UFIELD, opt->dbname, fn);
	ufield_fd = open(fn, O_RDONLY | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (ufield_fd == -1) {
		/* ERROR */
		printf("unable to open %s for read\n", fn);
		exit(1);
	}
	/* stat ufield to get size */
	if (etymon_af_fstat(ufield_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	ufield_isize = st.st_size / sizeof(ETYMON_INDEX_UFIELD);
	ufield_f = fdopen(ufield_fd, "rb");
	
	/* open uword for read */
	etymon_db_construct_path(ETYMON_DBF_UWORD, opt->dbname, fn);
	uword_fd = open(fn, O_RDONLY | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (uword_fd == -1) {
		/* ERROR */
		printf("unable to open %s for read\n", fn);
		exit(1);
	}
	/* stat uword to get size */
	if (etymon_af_fstat(uword_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	uword_isize = st.st_size / sizeof(ETYMON_INDEX_UWORD);
	uword_f = fdopen(uword_fd, "rb");
	
	/* open lpost for append */
	etymon_db_construct_path(ETYMON_DBF_LPOST, opt->dbname, fn);
	lpost_fd = open(fn, O_WRONLY | O_APPEND | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (lpost_fd == -1) {
		/* ERROR */
		printf("unable to open %s for append\n", fn);
		exit(1);
	}
	/* stat lpost to get size */
	if (etymon_af_fstat(lpost_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	lpost_isize = st.st_size / sizeof(ETYMON_INDEX_LPOST);
	lpost_f = fdopen(lpost_fd, "ab");
	
	/* open lfield for append */
	etymon_db_construct_path(ETYMON_DBF_LFIELD, opt->dbname, fn);
	lfield_fd = open(fn, O_WRONLY | O_APPEND | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (lfield_fd == -1) {
		/* ERROR */
		printf("unable to open %s for append\n", fn);
		exit(1);
	}
	/* stat lfield to get size */
	if (etymon_af_fstat(lfield_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	lfield_isize = st.st_size / sizeof(ETYMON_INDEX_LFIELD);
	lfield_f = fdopen(lfield_fd, "ab");

	/* open lword for append */
	etymon_db_construct_path(ETYMON_DBF_LWORD, opt->dbname, fn);
	lword_fd = open(fn, O_WRONLY | O_APPEND | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (lword_fd == -1) {
		/* ERROR */
		printf("unable to open %s for append\n", fn);
		exit(1);
	}
	/* stat lword to get size */
	if (etymon_af_fstat(lword_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	lword_isize = st.st_size / sizeof(ETYMON_INDEX_LWORD);
	lword_f = fdopen(lword_fd, "ab");
	
	/* optimize! */
	
/*	if (opt->verbose >= 2) {*/
		printf("Linearizing (new)\n");
/*	}*/
	
	/* first descend to the left-most leaf page */
	udict_p = dbinfo.udict_root;
	do {
		if (fseeko(udict_f, (etymon_af_off_t)udict_p, SEEK_SET) == -1) {
			perror("index_optimize():fseeko()");
		}
		if (fread(&(leaf_flag), 1, 1, udict_f) < 1) {
			perror("index_optimize():fread()");
		}
		if (leaf_flag == 0) {
			if (fread(&page_nl, 1, sizeof(ETYMON_INDEX_PAGE_NL), udict_f) < sizeof(ETYMON_INDEX_PAGE_NL)) {
				perror("index_optimize():fread()");
			}
			udict_p = page_nl.p[0];
		}
	} while (leaf_flag == 0);

	/* now go through all leaf pages */

	do {

		if (fseeko(udict_f, ((etymon_af_off_t)(udict_p + 1)), SEEK_SET) == -1) {
			perror("index_optimize():fseeko()");
		}
		if (fread(&page_l, 1, sizeof(ETYMON_INDEX_PAGE_L), udict_f) < sizeof(ETYMON_INDEX_PAGE_L)) {
			perror("index_optimize():fread()");
		}

		/* examine each key and optimize associated posting, field data, and word number data */

		for (x = 0; x < page_l.n; x++) {

			page_l.post_n[x] = 0;
			
			/* run through postings, assume matching doc_id's are consecutive */
			lpost_p_save = lpost_isize + 1;
			upost_p = page_l.post[x];
			lpost.doc_id = 0;
			while (upost_p != 0) {
				/* read a upost node */
				if (fseeko(upost_f,
					  (etymon_af_off_t)( ((etymon_af_off_t)(upost_p - 1)) * ((etymon_af_off_t)(sizeof(ETYMON_INDEX_UPOST))) ),
					  SEEK_SET) == -1) {
					perror("index_optimize():fseeko()");
				}
				if (fread(&upost, 1, sizeof(ETYMON_INDEX_UPOST), upost_f) < sizeof(ETYMON_INDEX_UPOST)) {
					perror("index_optimize():fread()");
				}

				/* optimize fields */
				/* DO WE NEED TO LOOK FOR DUPLICATES? */
				lfield_p_save = lfield_isize + 1;
				field_count = 0;
				ufield_p = upost.fields;
				while (ufield_p != 0) {
					field_count++;
					if (fseeko(ufield_f,
						  (etymon_af_off_t)( ((etymon_af_off_t)(ufield_p - 1)) *
							   ((etymon_af_off_t)(sizeof(ETYMON_INDEX_UFIELD))) ),
						  SEEK_SET) == -1) {
						perror("index_optimize():fseeko()");
					}
					if (fread(&ufield, 1, sizeof(ETYMON_INDEX_UFIELD), ufield_f) <
					    sizeof(ETYMON_INDEX_UFIELD)) {
						perror("index_optimize():fread()");
					}
					memcpy(lfield.fields, ufield.fields, ETYMON_MAX_FIELD_NEST * 2);
					if (fwrite(&lfield, 1, sizeof(ETYMON_INDEX_LFIELD), lfield_f) <
					    sizeof(ETYMON_INDEX_LFIELD)) {
						perror("index_optimize():fwrite()");
					}
					lfield_isize++;
					ufield_p = ufield.next;
				}

				/* optimize word numbers */
				lword_p_save = lword_isize + 1;
				word_count = 0;
				uword_p = upost.word_numbers;
				while (uword_p != 0) {
					word_count++;
					if (fseeko(uword_f,
						  (etymon_af_off_t)( ((etymon_af_off_t)(uword_p - 1)) *
							   ((etymon_af_off_t)(sizeof(ETYMON_INDEX_UWORD))) ),
						  SEEK_SET) == -1) {
						perror("index_optimize():fseeko()");
					}
					if (fread(&uword, 1, sizeof(ETYMON_INDEX_UWORD), uword_f) <
					    sizeof(ETYMON_INDEX_UWORD)) {
						perror("index_optimize():fread()");
					}
					lword.wn = uword.wn;
					if (fwrite(&lword, 1, sizeof(ETYMON_INDEX_LWORD), lword_f) <
					    sizeof(ETYMON_INDEX_LWORD)) {
						perror("index_optimize():fwrite()");
					}
					lword_isize++;
					uword_p = uword.next;
				}

				/* compare the doc_id with our cached lpost */
				if (upost.doc_id == lpost.doc_id) {
					/* increment the frequency and field count */
					lpost.freq += upost.freq;
					lpost.fields_n += field_count;
					lpost.word_numbers_n += word_count;
				} else {
					/* flush lpost */
					if (lpost.doc_id != 0) { /* only flush if lpost contains something */
						if (fwrite(&lpost, 1, sizeof(ETYMON_INDEX_LPOST), lpost_f) <
						    sizeof(ETYMON_INDEX_LPOST)) {
							perror("index_optimize():fwrite()");
						}
						lpost_isize++;
						page_l.post_n[x]++;
					}
					/* replace lpost with upost */
					lpost.doc_id = upost.doc_id;
					lpost.freq = upost.freq;
					lpost.fields_n = field_count;
					lpost.word_numbers_n = word_count;
					/* set field pointer */
					lpost.fields = lfield_p_save;
					lpost.word_numbers = lword_p_save;
				}
				upost_p = upost.next;
			} /* while */
			/* flush lpost */
			if (lpost.doc_id != 0) { /* only flush if lpost contains something */
				if (fwrite(&lpost, 1, sizeof(ETYMON_INDEX_LPOST), lpost_f) < sizeof(ETYMON_INDEX_LPOST)) {
					perror("index_optimize():fwrite()");
				}
				lpost_isize++;
				page_l.post_n[x]++;
			}
			page_l.post[x] = lpost_p_save;

		} /* for */

		/* write out updated leaf page */
		if (fseeko(udict_f, ((etymon_af_off_t)(udict_p + 1)), SEEK_SET) == -1) {
			perror("index_optimize():fseeko()");
		}
		if (fwrite(&page_l, 1, sizeof(ETYMON_INDEX_PAGE_L), udict_f) < sizeof(ETYMON_INDEX_PAGE_L)) {
			perror("index_optimize():fwrite()");
		}
		
		udict_p = page_l.next;

	} while (udict_p != 0);

	/* update dbinfo */
	if (fseeko(dbinfo_f, (etymon_af_off_t)0, SEEK_SET) == -1) {
		perror("index_optimize():fseeko()");
	}
	magic = ETYMON_INDEX_MAGIC;
	nbytes = fwrite(&magic, 1, sizeof(Uint4), dbinfo_f);
	if (nbytes != sizeof(Uint4)) {
		/* ERROR */
		printf("unable to write MN\n");
		exit(1);
	}
	dbinfo.optimized = 1;
	nbytes = fwrite(&dbinfo, 1, sizeof(ETYMON_DB_INFO), dbinfo_f);
	if (nbytes != sizeof(ETYMON_DB_INFO)) {
		/* ERROR */
		printf("unable to write DBI\n");
		exit(1);
	}

	/* clean up */
	fclose(dbinfo_f);
	fclose(udict_f);
	fclose(upost_f);
	fclose(ufield_f);
	fclose(uword_f);
	fclose(lpost_f);
	fclose(lfield_f);
	fclose(lword_f);
	close(dbinfo_fd);
	close(udict_fd);
	close(upost_fd);
	close(ufield_fd);
	close(uword_fd);
	close(lpost_fd);
	close(lfield_fd);
	close(lword_fd);

	/* reopen and truncate upost */
	etymon_db_construct_path(ETYMON_DBF_UPOST, opt->dbname, fn);
	upost_fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	close(upost_fd);
	/* reopen and truncate ufield */
	etymon_db_construct_path(ETYMON_DBF_UFIELD, opt->dbname, fn);
	ufield_fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	close(ufield_fd);
	/* reopen and truncate uword */
	etymon_db_construct_path(ETYMON_DBF_UWORD, opt->dbname, fn);
	uword_fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	close(uword_fd);

	/* unlock the database */
	etymon_db_unlock(opt->dbname, &(opt->log));

	return 0;
	
} /* optimize_new() */

#endif

/* this was written before the advent of ETYMON_INDEX_PAGE_L.post_n[],
   ETYMON_INDEX_UPOST.fields_n, and ETYMON_INDEX_UPOST.word_numbers_n
   in the first unoptimized pass; so it explicitly counts these values
   while building the optimized structures */
int etymon_index_optimize_old(ETYMON_INDEX_OPTIONS* opt) {
	int dbinfo_fd, udict_fd, upost_fd, ufield_fd, uword_fd, lpost_fd, lfield_fd, lword_fd;
	int x;
	etymon_af_off_t udict_size, upost_isize, ufield_isize, uword_isize, lpost_isize, lfield_isize, lword_isize;
	Uint4 magic;
	ETYMON_DB_INFO dbinfo;
	char fn[ETYMON_MAX_PATH_SIZE];
	ETYMON_AF_STAT st;
	ssize_t nbytes;
	Uint4 udict_p, upost_p, lpost_p_save;
	Uint4 ufield_p, lfield_p_save, field_count;
	Uint4 uword_p, lword_p_save, word_count;
	ETYMON_INDEX_PAGE_L page_l;
	ETYMON_INDEX_PAGE_NL page_nl;
	Uint1 leaf_flag;
	ETYMON_INDEX_UPOST upost;
	ETYMON_INDEX_LPOST lpost;
	ETYMON_INDEX_UFIELD ufield;
	ETYMON_INDEX_LFIELD lfield;
	ETYMON_INDEX_UWORD uword;
	ETYMON_INDEX_LWORD lword;

	/* make sure database is ready */
	if (etymon_db_ready(opt->dbname) == 0) {
		int e;
		char s[ETYMON_MAX_MSG_SIZE];
		sprintf(s, "%s: Database not ready", opt->dbname);
		e = opt->log.error(s, 1);
		if (e != 0) {
			exit(e);
		}
		return -1;
	}
	
	/* lock the database */
	etymon_db_lock(opt->dbname, &(opt->log));
	
	/* open db info file for read/write */
	etymon_db_construct_path(ETYMON_DBF_INFO, opt->dbname, fn);
	dbinfo_fd = open(fn, O_RDWR | ETYMON_AF_O_LARGEFILE);
	if (dbinfo_fd == -1) {
		int e;
		char s[ETYMON_MAX_MSG_SIZE];
		sprintf(s, "%s: Unable to open database", opt->dbname);
		e = opt->log.error(s, 1);
		etymon_db_unlock(opt->dbname);
		if (e != 0) {
			exit(e);
		}
		return -1;
	}
	nbytes = read(dbinfo_fd, &magic, sizeof(Uint4));
	if (nbytes != sizeof(Uint4)) {
		/* ERROR */
		printf("unable to read %s\n", fn);
		exit(1);
	}
	if (magic != ETYMON_INDEX_MAGIC) {
		int e;
		char s[ETYMON_MAX_MSG_SIZE];
		sprintf(s, "%s: Database created by incompatible version", opt->dbname);
		e = opt->log.error(s, 1);
		close(dbinfo_fd);
		etymon_db_unlock(opt->dbname);
		if (e != 0) {
			exit(e);
		}
		return -1;
	}
	nbytes = read(dbinfo_fd, &dbinfo, sizeof(ETYMON_DB_INFO));
	if (nbytes != sizeof(ETYMON_DB_INFO)) {
		/* ERROR */
		printf("unable to read %s\n", fn);
		exit(1);
	}
	
	/* make sure the database is not already optimized */
	if (dbinfo.optimized == 1) {
		int e;
		char s[ETYMON_MAX_MSG_SIZE];
		sprintf(s, "%s: Database is already linearized", opt->dbname);
		e = opt->log.error(s, 1);
		close(dbinfo_fd);
		etymon_db_unlock(opt->dbname); /* unlock the database */
		if (e != 0) {
			exit(e);
		}
		return -1;
	}

	/* open files */
	
	/* open udict for read/write */
	etymon_db_construct_path(ETYMON_DBF_UDICT, opt->dbname, fn);
	udict_fd = open(fn, O_RDWR | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (udict_fd == -1) {
		/* ERROR */
		printf("unable to open %s for read/write\n", fn);
		exit(1);
	}
	/* stat udict to get size */
	if (etymon_af_fstat(udict_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	udict_size = st.st_size;
	
	/* open upost for read */
	etymon_db_construct_path(ETYMON_DBF_UPOST, opt->dbname, fn);
	upost_fd = open(fn, O_RDONLY | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (upost_fd == -1) {
		/* ERROR */
		printf("unable to open %s for read\n", fn);
		exit(1);
	}
	/* stat upost to get size */
	if (etymon_af_fstat(upost_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	upost_isize = st.st_size / sizeof(ETYMON_INDEX_UPOST);
	
	/* open ufield for read */
	etymon_db_construct_path(ETYMON_DBF_UFIELD, opt->dbname, fn);
	ufield_fd = open(fn, O_RDONLY | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (ufield_fd == -1) {
		/* ERROR */
		printf("unable to open %s for read\n", fn);
		exit(1);
	}
	/* stat ufield to get size */
	if (etymon_af_fstat(ufield_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	ufield_isize = st.st_size / sizeof(ETYMON_INDEX_UFIELD);
	
	/* open uword for read */
	etymon_db_construct_path(ETYMON_DBF_UWORD, opt->dbname, fn);
	uword_fd = open(fn, O_RDONLY | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (uword_fd == -1) {
		/* ERROR */
		printf("unable to open %s for read\n", fn);
		exit(1);
	}
	/* stat uword to get size */
	if (etymon_af_fstat(uword_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	uword_isize = st.st_size / sizeof(ETYMON_INDEX_UWORD);
	
	/* open lpost for append */
	etymon_db_construct_path(ETYMON_DBF_LPOST, opt->dbname, fn);
	lpost_fd = open(fn, O_WRONLY | O_APPEND | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (lpost_fd == -1) {
		/* ERROR */
		printf("unable to open %s for append\n", fn);
		exit(1);
	}
	/* stat lpost to get size */
	if (etymon_af_fstat(lpost_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	lpost_isize = st.st_size / sizeof(ETYMON_INDEX_LPOST);
	
	/* open lfield for append */
	etymon_db_construct_path(ETYMON_DBF_LFIELD, opt->dbname, fn);
	lfield_fd = open(fn, O_WRONLY | O_APPEND | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (lfield_fd == -1) {
		/* ERROR */
		printf("unable to open %s for append\n", fn);
		exit(1);
	}
	/* stat lfield to get size */
	if (etymon_af_fstat(lfield_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	lfield_isize = st.st_size / sizeof(ETYMON_INDEX_LFIELD);

	/* open lword for append */
	etymon_db_construct_path(ETYMON_DBF_LWORD, opt->dbname, fn);
	lword_fd = open(fn, O_WRONLY | O_APPEND | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (lword_fd == -1) {
		/* ERROR */
		printf("unable to open %s for append\n", fn);
		exit(1);
	}
	/* stat lword to get size */
	if (etymon_af_fstat(lword_fd, &st) == -1) {
		perror("index_optimize():fstat()");
	}
	lword_isize = st.st_size / sizeof(ETYMON_INDEX_LWORD);
	
	/* optimize! */

	if (opt->verbose >= 2) {
		printf("Linearizing (old)\n");
	}
	
	/* first descend to the left-most leaf page */
	udict_p = dbinfo.udict_root;
	do {
		if (etymon_af_lseek(udict_fd, (etymon_af_off_t)udict_p, SEEK_SET) == -1) {
			perror("index_optimize():lseek()");
		}
		if (read(udict_fd, &(leaf_flag), 1) == -1) {
			perror("index_optimize():read()");
		}
		if (leaf_flag == 0) {
			if (read(udict_fd, &page_nl, sizeof(ETYMON_INDEX_PAGE_NL)) == -1) {
				perror("index_optimize():read()");
			}
			udict_p = page_nl.p[0];
		}
	} while (leaf_flag == 0);

	/* now go through all leaf pages */

	do {

		if (etymon_af_lseek(udict_fd, ((etymon_af_off_t)(udict_p + 1)), SEEK_SET) == -1) {
			perror("index_optimize():lseek()");
		}
		if (read(udict_fd, &page_l, sizeof(ETYMON_INDEX_PAGE_L)) == -1) {
			perror("index_optimize():read()");
		}

		/* examine each key and optimize associated posting, field data, and word number data */

		for (x = 0; x < page_l.n; x++) {

			page_l.post_n[x] = 0;
			
			/* run through postings, assume matching doc_id's are consecutive */
			lpost_p_save = lpost_isize + 1;
			upost_p = page_l.post[x];
			lpost.doc_id = 0;
			while (upost_p != 0) {
				/* read a upost node */
				if (etymon_af_lseek(upost_fd,
					  (etymon_af_off_t)( ((etymon_af_off_t)(upost_p - 1)) * ((etymon_af_off_t)(sizeof(ETYMON_INDEX_UPOST))) ),
					  SEEK_SET) == -1) {
					perror("index_optimize():lseek()");
				}
				if (read(upost_fd, &upost, sizeof(ETYMON_INDEX_UPOST)) == -1) {
					perror("index_optimize():read()");
				}

				/* optimize fields */
				/* DO WE NEED TO LOOK FOR DUPLICATES? */
				lfield_p_save = lfield_isize + 1;
				field_count = 0;
				ufield_p = upost.fields;
				while (ufield_p != 0) {
					field_count++;
					if (etymon_af_lseek(ufield_fd,
						  (etymon_af_off_t)( ((etymon_af_off_t)(ufield_p - 1)) *
							   ((etymon_af_off_t)(sizeof(ETYMON_INDEX_UFIELD))) ),
						  SEEK_SET) == -1) {
						perror("index_optimize():lseek()");
					}
					if (read(ufield_fd, &ufield, sizeof(ETYMON_INDEX_UFIELD)) == -1) {
						perror("index_optimize():read()");
					}
					memcpy(lfield.fields, ufield.fields, ETYMON_MAX_FIELD_NEST * 2);
					if (write(lfield_fd, &lfield, sizeof(ETYMON_INDEX_LFIELD)) == -1) {
						perror("index_optimize():write()");
					}
					lfield_isize++;
					ufield_p = ufield.next;
				}

				/* optimize word numbers */
				lword_p_save = lword_isize + 1;
				word_count = 0;
				uword_p = upost.word_numbers;
				while (uword_p != 0) {
					word_count++;
					if (etymon_af_lseek(uword_fd,
						  (etymon_af_off_t)( ((etymon_af_off_t)(uword_p - 1)) *
							   ((etymon_af_off_t)(sizeof(ETYMON_INDEX_UWORD))) ),
						  SEEK_SET) == -1) {
						perror("index_optimize():lseek()");
					}
					if (read(uword_fd, &uword, sizeof(ETYMON_INDEX_UWORD)) == -1) {
						perror("index_optimize():read()");
					}
					lword.wn = uword.wn;
					if (write(lword_fd, &lword, sizeof(ETYMON_INDEX_LWORD)) == -1) {
						perror("index_optimize():write()");
					}
					lword_isize++;
					uword_p = uword.next;
				}

				/* compare the doc_id with our cached lpost */
				if (upost.doc_id == lpost.doc_id) {
					/* increment the frequency and field count */
					lpost.freq += upost.freq;
					lpost.fields_n += field_count;
					lpost.word_numbers_n += word_count;
				} else {
					/* flush lpost */
					if (lpost.doc_id != 0) { /* only flush if lpost contains something */
						if (write(lpost_fd, &lpost, sizeof(ETYMON_INDEX_LPOST)) == -1) {
							perror("index_optimize():write()");
						}
						lpost_isize++;
						page_l.post_n[x]++;
					}
					/* replace lpost with upost */
					lpost.doc_id = upost.doc_id;
					lpost.freq = upost.freq;
					lpost.fields_n = field_count;
					lpost.word_numbers_n = word_count;
					/* set field pointer */
					lpost.fields = lfield_p_save;
					lpost.word_numbers = lword_p_save;
				}
				upost_p = upost.next;
			} /* while */
			/* flush lpost */
			if (lpost.doc_id != 0) { /* only flush if lpost contains something */
				if (write(lpost_fd, &lpost, sizeof(ETYMON_INDEX_LPOST)) == -1) {
					perror("index_optimize():write()");
				}
				lpost_isize++;
				page_l.post_n[x]++;
			}
			page_l.post[x] = lpost_p_save;

		} /* for */

		/* write out updated leaf page */
		if (etymon_af_lseek(udict_fd, ((etymon_af_off_t)(udict_p + 1)), SEEK_SET) == -1) {
			perror("index_optimize():lseek()");
		}
		if (write(udict_fd, &page_l, sizeof(ETYMON_INDEX_PAGE_L)) == -1) {
			perror("index_optimize():write()");
		}
		
		udict_p = page_l.next;

	} while (udict_p != 0);

	/* update dbinfo */
	if (etymon_af_lseek(dbinfo_fd, (etymon_af_off_t)0, SEEK_SET) == -1) {
		perror("index_optimize():lseek()");
	}
	magic = ETYMON_INDEX_MAGIC;
	nbytes = write(dbinfo_fd, &magic, sizeof(Uint4));
	if (nbytes != sizeof(Uint4)) {
		/* ERROR */
		printf("unable to write MN\n");
		exit(1);
	}
	dbinfo.optimized = 1;
	nbytes = write(dbinfo_fd, &dbinfo, sizeof(ETYMON_DB_INFO));
	if (nbytes != sizeof(ETYMON_DB_INFO)) {
		/* ERROR */
		printf("unable to write DBI\n");
		exit(1);
	}
	close(dbinfo_fd);

	/* clean up */
	close(udict_fd);
	close(upost_fd);
	close(ufield_fd);
	close(uword_fd);
	close(lpost_fd);
	close(lfield_fd);
	close(lword_fd);

	/* reopen and truncate upost */
	etymon_db_construct_path(ETYMON_DBF_UPOST, opt->dbname, fn);
	upost_fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	close(upost_fd);
	/* reopen and truncate ufield */
	etymon_db_construct_path(ETYMON_DBF_UFIELD, opt->dbname, fn);
	ufield_fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	close(ufield_fd);
	/* reopen and truncate uword */
	etymon_db_construct_path(ETYMON_DBF_UWORD, opt->dbname, fn);
	uword_fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	close(uword_fd);

	/* unlock the database */
	etymon_db_unlock(opt->dbname);

	return 0;
}

#endif

void etymon_index_write_nl(int filedes, etymon_af_off_t offset, ETYMON_INDEX_PAGE_NL* page) {
	static Uint1 leaf_flag = 0;
	if (etymon_af_lseek(filedes, (etymon_af_off_t)offset, SEEK_SET) == -1) {
		perror("index_write_nl():lseek()");
	}
	if (write(filedes, &(leaf_flag), 1) == -1) {
		perror("index_write_nl():write()");
	}
	if (write(filedes, page, sizeof(ETYMON_INDEX_PAGE_NL)) == -1) {
		perror("index_write_nl():write()");
	}
}


void etymon_index_write_l(int filedes, etymon_af_off_t offset, ETYMON_INDEX_PAGE_L* page) {
	static Uint1 leaf_flag = 1;
	if (etymon_af_lseek(filedes, (etymon_af_off_t)offset, SEEK_SET) == -1) {
		perror("index_write_l():lseek()");
	}
	if (write(filedes, &(leaf_flag), 1) == -1) {
		perror("index_write_l():write()");
	}
	if (write(filedes, page, sizeof(ETYMON_INDEX_PAGE_L)) == -1) {
		perror("index_write_l():write()");
	}
}


/* returns 1 if flush was performed */
int etymon_index_flush_l(ETYMON_INDEX_INDEXING_STATE* state) {
	if (state->pcache_l_write != 0) {
		etymon_index_write_l(state->udict_fd, state->pcache_l_write, &(state->pcache_l));
		state->pcache_l_write = 0;
		return 1;
	} else {
		return 0;
	}
}


int etymon_index_search_keys_nl(unsigned char* word, size_t word_len, ETYMON_INDEX_PAGE_NL* page) {
	int j, k, m, len, comp;
	if (page->n == 0) {
		return 0;
	}
	j = 0;
	k = page->n - 1;
	while (j <= k) {
		m = (j + k) / 2;
		len = page->offset[m + 1] - page->offset[m];
		comp = strncmp( (char*)word,
				(char*)(page->keys + page->offset[m]),
				len );
		if (comp == 0) {
			comp = word_len - len;
		}
		if (comp < 0) {
			k = m - 1;
		}
		else if (comp > 0) {
			j = m + 1;
		}
		else {
			return m + 1; /* different from a leaf search - match gives right side pointer */
		}
	}
	return j;
}


int etymon_index_search_keys_l(unsigned char* word, size_t word_len, ETYMON_INDEX_PAGE_L* page, int* match) {
	int j, k, m, len, comp;
	if (page->n == 0) {
		*match = 0;
		return 0;
	}
	j = 0;
	k = page->n - 1;
	while (j <= k) {
		m = (j + k) / 2;
		len = page->offset[m + 1] - page->offset[m];
/*		printf("COMPARING:\n[%s]\n[%s]\nfor %i\n\n", (char*)word, (char*)(page->keys + page->offset[m]), len); */
		comp = strncmp( (char*)word,
				(char*)(page->keys + page->offset[m]),
				len );
		if (comp == 0) {
			comp = word_len - len;
		}
		if (comp < 0) {
			k = m - 1;
		}
		else if (comp > 0) {
			j = m + 1;
		}
		else {
			*match = 1;
			return m; /* different from non-leaf search - match gives left side pointer */
		}
	}
	*match = 0;
	return j;
}


void etymon_index_write_upost(ETYMON_INDEX_INDEXING_STATE* state, int wcache_p, ETYMON_INDEX_PAGE_L* page_l, int ins) {
	int p;
	Uint4 ufield_p;
	Uint4 uword_p;
	state->upost.doc_id = state->wcache[wcache_p].doc_id;
	state->upost.freq = state->wcache[wcache_p].freq;
	state->upost.next = page_l->post[ins];

	/* write out fields if there are any */
	if (state->wcache[wcache_p].fields == -1) {
		state->upost.fields = 0;
		state->upost.fields_n = 0;
	} else {
		/* write out fields */
		p = state->wcache[wcache_p].fields;
		ufield_p = 0;
		state->upost.fields_n = 0;
		while (p != -1) {
			memcpy(state->ufield.fields, state->fcache[p].f, ETYMON_MAX_FIELD_NEST * 2);
			state->ufield.next = ufield_p;
			if (write(state->ufield_fd, &(state->ufield), sizeof(ETYMON_INDEX_UFIELD)) == -1) {
				perror("index_write_upost():write()");
			}
			p = state->fcache[p].next;
			state->ufield_isize++;
			ufield_p = state->ufield_isize;
			state->upost.fields_n++;
		}
		state->upost.fields = ufield_p;
	}

	/* write out word number data if any */
	if (state->number_words) {
		/* write out word numbers */
		p = state->wcache[wcache_p].word_numbers_head;
		uword_p = 0;
		state->upost.word_numbers_n = 0;
		while (p != -1) {
			state->uword.wn = state->wncache[p].wn;
			state->uword.next = uword_p;
			if (write(state->uword_fd, &(state->uword), sizeof(ETYMON_INDEX_UWORD)) == -1) {
				perror("index_write_upost():write()");
			}
			p = state->wncache[p].next;
			state->uword_isize++;
			uword_p = state->uword_isize;
			state->upost.word_numbers_n++;
		}
		state->upost.word_numbers = uword_p;
	} else {
		state->upost.word_numbers = 0;
		state->upost.word_numbers_n = 0;
	}

	page_l->post[ins] = state->upost_isize + 1;
	page_l->post_n[ins]++;
	
	/* now write out the new upost */
	if (write(state->upost_fd, &(state->upost), sizeof(ETYMON_INDEX_UPOST)) == -1) {
		perror("index_write_upost():write()");
	}
	state->upost_isize++;
}


void etymon_index_insert_key_l(ETYMON_INDEX_PAGE_L* page, int ins, unsigned char* word, size_t word_len) {
	int x;
	/* first scoot the keys over and insert the new word */
	if (ins < page->n) { /* don't need to if we're at the end of the key buffer */
		memmove(page->keys + page->offset[ins] + word_len,
			page->keys + page->offset[ins],
			page->offset[page->n] - page->offset[ins]);
	}
	memcpy(page->keys + page->offset[ins], word, word_len);
	/* move post data over */
	memmove(page->post + ins + 1, page->post + ins, (page->n - ins) * sizeof(Uint4));
	memmove(page->post_n + ins + 1, page->post_n + ins, (page->n - ins) * sizeof(Uint4));
	/* next scoot the offsets directory over (add word_len to offsets) */
	page->n++;
	for (x = page->n; x > ins; x--) {
		page->offset[x] = page->offset[x - 1] + word_len;
	}
}


void etymon_index_insert_key_nl(ETYMON_INDEX_PAGE_NL* page, int ins, unsigned char* word, size_t word_len) {
	int x;
	/* first scoot the keys over and insert the new word */
	if (ins < page->n) { /* don't need to if we're at the end of the key buffer */
		memmove(page->keys + page->offset[ins] + word_len,
			page->keys + page->offset[ins],
			page->offset[page->n] - page->offset[ins]);
	}
	memcpy(page->keys + page->offset[ins], word, word_len);
	/* move the page pointers */
	memmove(page->p + ins + 2, page->p + ins + 1, (page->n - ins) * sizeof(Uint4));
	/* next scoot the offsets directory over (add word_len to offsets) */
	page->n++;
	for (x = page->n; x > ins; x--) {
		page->offset[x] = page->offset[x - 1] + word_len;
	}
}


/* fills in word with the shortest separator between the two pages, and returns the word length */
int etymon_index_shortest_sep_l(ETYMON_INDEX_PAGE_L* left, ETYMON_INDEX_PAGE_L* right, unsigned char* word) {
	static int p;
	static int max;
	static unsigned char* left_word;
	max = right->offset[1];
	left_word = left->keys + left->offset[left->n - 1];
	p = 0;
	do {
		word[p] = right->keys[p];
		p++;
	} while ( (p < max) && (left_word[p - 1] >= word[p - 1]) );
	word[p] = '\0';
	return p;
}


/* fills in word with the shortest separator between the two pages, and returns the word length */
int etymon_index_shortest_sep_nl(ETYMON_INDEX_PAGE_NL* left, ETYMON_INDEX_PAGE_NL* right, unsigned char* word) {
	static int p;
	static int max;
	static unsigned char* left_word;
	max = right->offset[1];
	left_word = left->keys + left->offset[left->n - 1];
	p = 0;
	do {
		word[p] = right->keys[p];
		p++;
	} while ( (p < max) && (left_word[p - 1] >= word[p - 1]) );
	word[p] = '\0';
	return p;
}


void etymon_index_parent_add_key(ETYMON_INDEX_INDEXING_STATE* state, int level, unsigned char* word, size_t word_len,
				 Uint4 child) {
	int x, y, ins;
	unsigned char new_word[ETYMON_MAX_WORD_SIZE];
	size_t new_word_len;
	Uint4 overflow_pos;

	/* first check if we have ascended above the root of the tree */
	if (level < 0) {
		/* if so, we create a new root page */
		/* we can do it place of the old root position in the pcache, and invalidate the rest of the pcache */
		state->pcache_nl[0].nl.n = 1;
		state->pcache_nl[0].nl.p[0] = state->pcache_nl[0].pos; /* grab the pos from the now former root page */
		state->pcache_nl[0].nl.p[1] = child;
		state->pcache_nl[0].nl.offset[0] = 0;
		state->pcache_nl[0].nl.offset[1] = word_len;
		memcpy(state->pcache_nl[0].nl.keys, word, word_len);
		state->pcache_nl[0].pos = state->udict_size;
		state->pcache_nl[0].is_nl = 1;
		state->pcache_count = 1;
		/* now write out the new root page */
		etymon_index_write_nl(state->udict_fd, state->pcache_nl[0].pos, &(state->pcache_nl[0].nl));
		state->udict_size += sizeof(Uint1) + sizeof(ETYMON_INDEX_PAGE_NL);
		/* set new root pointer */
		state->udict_root = state->pcache_nl[0].pos;
		return;
	}

	/* check if page is full */
	if ( (state->pcache_nl[level].nl.n >= ETYMON_MAX_KEYS_NL) ||
	     ((ETYMON_MAX_KEY_AREA_NL - state->pcache_nl[level].nl.offset[state->pcache_nl[level].nl.n]) /* empty key space */
	      < (int)word_len) ) {

		/* split the page */
		
		/* allocate new non-leaf page for split overflow */
		/* we move half of the keys into the overflow leaf page */
		state->overflow_nl.n = state->pcache_nl[level].nl.n / 2;
		/* move the offsets - by hand */
		y = state->pcache_nl[level].nl.offset[state->pcache_nl[level].nl.n - state->overflow_nl.n];
		for (x = 0; x <= state->overflow_nl.n; x++) {
			state->overflow_nl.offset[x] =
				state->pcache_nl[level].nl.offset[x + state->pcache_nl[level].nl.n -
								 state->overflow_nl.n] - y;
		}

		/* move the keys */
		memcpy(state->overflow_nl.keys,
		       state->pcache_nl[level].nl.keys +
		       state->pcache_nl[level].nl.offset[state->pcache_nl[level].nl.n - state->overflow_nl.n],
		       state->pcache_nl[level].nl.offset[state->pcache_nl[level].nl.n] -
		       state->pcache_nl[level].nl.offset[state->pcache_nl[level].nl.n - state->overflow_nl.n]);

		/* move the page pointers */
		memcpy(state->overflow_nl.p,
		       state->pcache_nl[level].nl.p +
		       state->pcache_nl[level].nl.n - state->overflow_nl.n,
		       (state->overflow_nl.n + 1) * sizeof(Uint4));

		state->pcache_nl[level].nl.n -= state->overflow_nl.n;

		/* remove the median key (now at the end of the old page),
		   which we remember and will insert into the parent */
		new_word_len = state->pcache_nl[level].nl.offset[state->pcache_nl[level].nl.n] -
			state->pcache_nl[level].nl.offset[state->pcache_nl[level].nl.n - 1];
		memcpy(new_word, state->pcache_nl[level].nl.keys +
		       state->pcache_nl[level].nl.offset[state->pcache_nl[level].nl.n - 1], new_word_len);
		new_word[new_word_len] = '\0';
		state->pcache_nl[level].nl.n--;
				
		/* insert the new key into either the old or new nl page */
		/* search in the new page */
		ins = etymon_index_search_keys_nl(word, word_len, &(state->overflow_nl));
		/* if it's at 0 then put it in the old */
		if (ins == 0) {
			/* it goes in the old page, in the last key position */
			etymon_index_insert_key_nl(&(state->pcache_nl[level].nl), state->pcache_nl[level].nl.n, word,
						word_len);
			
			state->pcache_nl[level].nl.p[ins + 1] = child;
		} else {
			/* it goes in the new page */
			etymon_index_insert_key_nl(&(state->overflow_nl), ins, word, word_len);
			
			state->overflow_nl.p[ins + 1] = child;
		}

		/* write out old nl page */
		etymon_index_write_nl(state->udict_fd, state->pcache_nl[level].pos, &(state->pcache_nl[level].nl));
		/* write out new overflow page */
		overflow_pos = state->udict_size;
		etymon_index_write_nl(state->udict_fd, overflow_pos, &(state->overflow_nl));
		/* update size of dictionary file */
		state->udict_size += sizeof(Uint1) + sizeof(ETYMON_INDEX_PAGE_NL);

		/* recursively update parent page, splitting if necessary */
		etymon_index_parent_add_key(state, level - 1, new_word, new_word_len, overflow_pos);

	} else {
		/* if not full, simply insert the new key */
		ins = etymon_index_search_keys_nl(word, word_len, &(state->pcache_nl[level].nl));
		etymon_index_insert_key_nl(&(state->pcache_nl[level].nl), ins, word, word_len);
		state->pcache_nl[level].nl.p[ins + 1] = child;
		/* write out the updated page */
		etymon_index_write_nl(state->udict_fd, state->pcache_nl[level].pos, &(state->pcache_nl[level].nl));
	}
	
}


/* returns 0 if everything was OK */
int etymon_index_index_word(ETYMON_INDEX_INDEXING_STATE* state, int wcache_p) {
	static int x, y, match, ins;
	static Uint4 p;
	static ssize_t nbytes;
	static Uint1 leaf_flag;
	static unsigned char* word;
	static unsigned char new_word[ETYMON_MAX_WORD_SIZE];
	static size_t word_len, new_word_len;
	static int level;

	word = state->wcache[wcache_p].word;

	/* search for the right page */

	/* start at the root page */
	p = state->udict_root;
	level = 0;
	
	/* search the tree by descent */
	do {
		
		/* get the page from the cache if available */
		
		if ( (level < state->pcache_count) &&
		     (state->pcache_nl[level].pos == p) ) {
			/* yes, it is in the cache */
			/* check whether it is a leaf page */
			if (state->pcache_nl[level].is_nl == 1) {
				leaf_flag = 0;
			} else {
				leaf_flag = 1;
			}
		} else {
			/* not in the cache, so read it from disk */
			if (etymon_af_lseek(state->udict_fd, (etymon_af_off_t)p, SEEK_SET) == -1) {
				perror("index_index_word():lseek()");
			}
			/* check whether it is a leaf page */
			nbytes = read(state->udict_fd, &(leaf_flag), sizeof(Uint1));
			if (nbytes != sizeof(Uint1)) {
				/* ERROR */
				printf("error reading from index (LP)\n");
				exit(1);
			}
			if (leaf_flag == 0) {
				/* read the non-leaf page into the right cache position */
				state->pcache_nl[level].pos = p;
				state->pcache_nl[level].is_nl = 1;
				state->pcache_count = level + 1;
				nbytes = read(state->udict_fd, &(state->pcache_nl[level].nl),
					      sizeof(ETYMON_INDEX_PAGE_NL));
				if (nbytes != sizeof(ETYMON_INDEX_PAGE_NL)) {
					/* ERROR */
					printf("error reading from index (NL)\n");
					exit(1);
				}
			} else {

				/* read the leaf page into the leaf page cache */
				
				if (etymon_index_flush_l(state) == 1) { /* flush leaf write cache first */
					if (etymon_af_lseek(state->udict_fd, (etymon_af_off_t)(p + 1), SEEK_SET) == -1) {
						perror("index_index_word():lseek()");
					}
				}
				
				state->pcache_nl[level].pos = p;
				state->pcache_nl[level].is_nl = 0;
				state->pcache_count = level + 1;
				nbytes = read(state->udict_fd, &(state->pcache_l), sizeof(ETYMON_INDEX_PAGE_L));
				if (nbytes != sizeof(ETYMON_INDEX_PAGE_L)) {
					/* ERROR */
					printf("error reading from index (L)\n");
					exit(1);
				}
			}
		}
		
		/* if it is not a leaf page, determine next seek */
		if (leaf_flag == 0) {
			ins = etymon_index_search_keys_nl(word, word_len,
							  &(state->pcache_nl[level].nl));
			p = state->pcache_nl[level].nl.p[ins];
			level++;
			/* Internal overflow (level) */
			if (level >= ETYMON_MAX_PAGE_DEPTH)
				return aferr(AFEUNKNOWN);
		}
		
	} while (leaf_flag == 0);
	/* we have reached a leaf page */

	word_len = strlen((char*)word);
	
	/* determine position in key list to insert new key */
	ins = etymon_index_search_keys_l(word, word_len,
					 &(state->pcache_l), &match);
	
	/* if we found a perfect match, then no need to insert the key; just add it to the postings */
	if (match) {
		
		etymon_index_write_upost(state, wcache_p, &(state->pcache_l), ins);
		
		/* tag leaf for write cache */
		state->pcache_l_write = p;
		
	} else {
		/* insert the key */
		/* check if page is full */
		if ( (state->pcache_l.n >= ETYMON_MAX_KEYS_L) ||
		     ((ETYMON_MAX_KEY_AREA_L - state->pcache_l.offset[state->pcache_l.n]) /* empty key space */
		      < (int)word_len) ) {
			
			/* if so, we split the page */

			/* allocate new leaf page for split overflow */
			/* we move half of the keys into the overflow leaf page */
			state->overflow_l.n = state->pcache_l.n / 2;
			state->overflow_l.prev = p;
			state->overflow_l.next = state->pcache_l.next;
			/* move the posting data */
			memcpy(state->overflow_l.post,
			       state->pcache_l.post +
			       state->pcache_l.n - state->overflow_l.n,
			       state->overflow_l.n * sizeof(Uint4));
			memcpy(state->overflow_l.post_n,
			       state->pcache_l.post_n +
			       state->pcache_l.n - state->overflow_l.n,
			       state->overflow_l.n * sizeof(Uint4));
			/* move the offsets - by hand */
			y = state->pcache_l.offset[state->pcache_l.n - state->overflow_l.n];
			for (x = 0; x <= state->overflow_l.n; x++) {
				state->overflow_l.offset[x] =
					state->pcache_l.offset[x + state->pcache_l.n - state->overflow_l.n] - y;
			}
			/* move the keys */
			memcpy(state->overflow_l.keys,
			       state->pcache_l.keys +
			       state->pcache_l.offset[state->pcache_l.n - state->overflow_l.n],
			       state->pcache_l.offset[state->pcache_l.n] -
			       state->pcache_l.offset[state->pcache_l.n - state->overflow_l.n]);
			state->pcache_l.n -= state->overflow_l.n;
			state->pcache_l.next = state->udict_size;
			
			/* insert the new key into either the old or new leaf page */
			if (ins <= state->pcache_l.n) {
				/* it goes in the old page */
				etymon_index_insert_key_l(&(state->pcache_l), ins, word, word_len);
				
				state->pcache_l.post[ins] = 0;
				state->pcache_l.post_n[ins] = 0;
				etymon_index_write_upost(state, wcache_p, &(state->pcache_l), ins);
			} else {
				/* it goes in the new page */
				
				x = etymon_index_search_keys_l(word, word_len,
							       &(state->overflow_l), &match);
				
				etymon_index_insert_key_l(&(state->overflow_l), x, word, word_len);
				
				state->overflow_l.post[x] = 0;
				state->overflow_l.post_n[x] = 0;
				etymon_index_write_upost(state, wcache_p, &(state->overflow_l), x);
			}
			
			/* tag old leaf for write caching */
			state->pcache_l_write = p;
			/* write out new overflow page */
			etymon_index_write_l(state->udict_fd, state->udict_size, &(state->overflow_l));
			y = state->udict_size; /**/
			/* update size of dictionary file */
			state->udict_size += sizeof(Uint1) + sizeof(ETYMON_INDEX_PAGE_L);

			/* update prev pointer in far-right leaf to point to overflow_l */
			if (state->overflow_l.next != 0) {
				if (etymon_af_lseek(state->udict_fd,
						    (etymon_af_off_t)(state->overflow_l.next + 1), SEEK_SET) == -1) {
					perror("index_index_word():lseek()");
				}
				nbytes = read(state->udict_fd, &(state->extra_l), sizeof(ETYMON_INDEX_PAGE_L));
				if (nbytes == -1) {
					perror("index_index_word():read()");
				}
				state->extra_l.prev = state->pcache_l.next;
				if (etymon_af_lseek(state->udict_fd, (etymon_af_off_t)(state->overflow_l.next + 1), SEEK_SET) == -1) {
					perror("index_index_word():lseek()");
				}
				nbytes = write(state->udict_fd, &(state->extra_l), sizeof(ETYMON_INDEX_PAGE_L));
				if (nbytes == -1) {
					perror("index_index_word():write()");
				}
			}
			
			/* we want to insert a new key in the parent to fork the split */

			new_word_len = etymon_index_shortest_sep_l(&(state->pcache_l), &(state->overflow_l),
								   new_word);
			
			/* recursively update parent page, splitting if necessary */
			etymon_index_parent_add_key(state, level - 1, new_word, new_word_len,
						    state->pcache_l.next);
				
		} else {
			
			/* if not, simply scoot keys over and insert new key */
			etymon_index_insert_key_l(&(state->pcache_l), ins, word, word_len);
			state->pcache_l.post[ins] = 0;
			state->pcache_l.post_n[ins] = 0;

			/* next add postings for the new key */
			
			etymon_index_write_upost(state, wcache_p, &(state->pcache_l), ins);
			
			/* tag leaf for write cache */
			state->pcache_l_write = p;
		}
		
	}
	
	return 0;
}


/* returns 0 if everything was OK */
int etymon_index_traverse_wcache(ETYMON_INDEX_INDEXING_STATE* state, int p) {
	int c;
	int start;
	if (state->wcache[p].left != -1) {
		if (etymon_index_traverse_wcache(state, state->wcache[p].left) == -1) {
			return -1;
		}
	}
	start = state->wcache[p].next;
	c = start;
	do {
		if (etymon_index_index_word(state, c) == -1) {
			return -1;
		}
		c = state->wcache[c].next;
	} while (c != start);
	if (state->wcache[p].right != -1) {
		if (etymon_index_traverse_wcache(state, state->wcache[p].right) == -1) {
			return -1;
		}
	}
	return 0;
}


/* returns 0 if everything was OK */
int etymon_index_dclass_index(ETYMON_INDEX_INDEXING_STATE* state) {
	static Uint1 leaf_flag;
	static ssize_t nbytes;

	if (!state->flushmsg) {
		state->flushmsg = 1;
		afprintv(state->verbose, 2, "Flushing index buffers");
	}
	/* make sure there is at least one page (root) */
	if (state->udict_root == 0) {
		/* seek to offset 0 and write one zero byte (unused) */
		if (etymon_af_lseek(state->udict_fd, (etymon_af_off_t)0, SEEK_SET) == -1) {
			perror("index_dclass_index():lseek()");
		}
		leaf_flag = 0; /* we'll use the leaf_flag variable, but this isn't really a leaf flag byte */
		nbytes = write(state->udict_fd, &(leaf_flag), 1);
		if (nbytes == -1) {
			perror("index_dclass_index():write()");
		}
		/* write an empty root page now at offset 1 */
		state->pcache_l.n = 0;
		state->pcache_l.prev = 0;
		state->pcache_l.next = 0;
		state->pcache_l.offset[0] = 0;
		leaf_flag = 1;
		nbytes = write(state->udict_fd, &(leaf_flag), sizeof(Uint1));
		if (nbytes != sizeof(Uint1)) {
			/* ERROR */
			printf("error writing to index\n");
			exit(1);
		}
		nbytes = write(state->udict_fd, &(state->pcache_l), sizeof(ETYMON_INDEX_PAGE_L));
		if (nbytes != sizeof(ETYMON_INDEX_PAGE_L)) {
			/* ERROR */
			printf("error writing to index\n");
			exit(1);
		}
		/* update size of dictionary file */
		state->udict_size += 2 + sizeof(ETYMON_INDEX_PAGE_L);
		/* set root pointer */
		state->udict_root = 1;
		/* root page is now cached and it is a leaf */
		state->pcache_nl[0].pos = 1;
		state->pcache_nl[0].is_nl = 0;
		state->pcache_count = 1;
	}

	if (state->wcache_count > 0) {
		if (etymon_index_traverse_wcache(state, state->wcache_root) == -1) {
			return -1;
		}
	}
	return 0;
}


/* return 0 if everything went well */
int etymon_index_dclass_finish(ETYMON_INDEX_INDEXING_STATE* state) {
	/* perform last indexing pass */
	if (etymon_index_dclass_index(state) == -1) {
		return -1;
	}
	/* flush write cached leaf node */
	etymon_index_flush_l(state);

	return 0;
}


ETYMON_AF_DC_SPLIT* etymon_af_index_get_split_list(ETYMON_AF_DC_INDEX*
						   dc_index, char* split) {
	ETYMON_AF_DC_SPLIT* split_list;
	ETYMON_AF_DC_SPLIT* split_p;
	ETYMON_DOCBUF* docbuf = dc_index->docbuf;
	int split_len = strlen(split);
	int split_match = 0;  /* number of characters matched with
				 delimiter string */
	unsigned char ch;
	Uint4 offset = 0;

	/* initialize split list */
	split_list =
		(ETYMON_AF_DC_SPLIT*)(malloc(
			sizeof(ETYMON_AF_DC_SPLIT)));
	split_p = split_list;
	
	/* return if the document size is 0 */
	if (docbuf->data_len == 0) {
		split_list->end = 0;
		split_list->next = NULL;
		return split_list;
	}

	/* skip first character to avoid a 0 length document resulting
	   from an immediate match */
	etymon_docbuf_next_char(docbuf);
	offset++;

	/* find matches to the delimiter string */
	while ( ! docbuf->eof ) {
		ch = etymon_docbuf_next_char(docbuf);
		offset++;
		if (ch == split[split_match]) {
			split_match++;
			if (split_match == split_len) {
				split_match = 0;
				split_p->end = offset - split_len;
				split_p->next = (ETYMON_AF_DC_SPLIT*)(malloc(
					sizeof(ETYMON_AF_DC_SPLIT)));
				split_p = split_p->next;
			}
		} else {
			if (split_match != 0) {
				split_match = 0;
			}
		}
	}

	split_p->end = docbuf->st.st_size;
	split_p->next = NULL;

	return split_list;
}


/* returns 0 if everything went OK */
int etymon_index_add_files(Afindex *opt) {
	ETYMON_DOCBUF* docbuf;
	ETYMON_INDEX_INDEXING_STATE* state;
	char s_file[ETYMON_MAX_PATH_SIZE];
	char* source_file;
	char fn[ETYMON_MAX_PATH_SIZE];
	char cwd[ETYMON_MAX_PATH_SIZE];
	ETYMON_AF_STAT st;
	ssize_t nbytes;
	size_t maxmem, memleft;
/*	int dbinfo_fd; */
	int x_file;
	Uint4 magic;
	ETYMON_DB_INFO *dbinfo;
	int dclass_id;
	int result;
	int fdef_fd;
	int done_files;
	int file_good;
	int x;
	size_t wcache_alloc, fcache_alloc, wncache_alloc;
	ETYMON_AF_DC_INDEX dc_index;
	ETYMON_AF_DC_INIT dc_init;
	ETYMON_AF_DC_SPLIT* split_list = NULL;
	ETYMON_AF_DC_SPLIT* split_p;
	int use_docbuf; /* 1: use docbuf; 0: don't use it */
	char *dbname;

	dbname = etymon_af_state[opt->dbid]->dbname;
	
	maxmem = ((size_t) opt->memory) * 1048576 - 1315000;

	/* make sure database is ready */
	if (etymon_db_ready(dbname) == 0)
		return aferr(AFEDBLOCK);
	
	/* lock the database */
	etymon_db_lock(dbname, NULL);
	
	/* open db info file for read/write */
/*
	etymon_db_construct_path(ETYMON_DBF_INFO, dbname, fn);
	dbinfo_fd = open(fn, O_RDWR | ETYMON_AF_O_LARGEFILE);
	if (dbinfo_fd == -1) {
		etymon_db_unlock(dbname);
		return aferr(AFEDBIO);
	}
	nbytes = read(dbinfo_fd, &magic, sizeof(Uint4));
	if (nbytes != sizeof(Uint4)) {
		printf("unable to read %s\n", fn);
		exit(1);
	}
	if (magic != ETYMON_INDEX_MAGIC) {
		close(dbinfo_fd);
		etymon_db_unlock(dbname);
		return aferr(AFEVERSION);
	}
	nbytes = read(dbinfo_fd, &dbinfo, sizeof(ETYMON_DB_INFO));
	if (nbytes != sizeof(ETYMON_DB_INFO)) {
		printf("unable to read %s\n", fn);
		exit(1);
	}
*/
	dbinfo = &(etymon_af_state[opt->dbid]->info);

	if (dbinfo->stemming && !af_stem_available()) {
		etymon_db_unlock(dbname);
		return aferr(AFENOSTEM);
	}
	
	/* we can only add files if the database is not optimized */
	if (dbinfo->optimized == 1) {
		etymon_db_unlock(dbname);
		return aferr(AFELINEAR);
	}
	
	/* set up state information for indexing */
	
	state = (ETYMON_INDEX_INDEXING_STATE*)(malloc(sizeof(ETYMON_INDEX_INDEXING_STATE)));

	state->udict_root = dbinfo->udict_root;
	state->doc_n = dbinfo->doc_n;
	state->dbname = dbname;
	state->verbose = opt->verbose;
	state->long_words = opt->_longwords;

	/* open doctable for append */
	etymon_db_construct_path(ETYMON_DBF_DOCTABLE, dbname, fn);
	state->doctable_fd = open(fn, O_WRONLY | O_APPEND | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (state->doctable_fd == -1) {
		/* ERROR */
		printf("unable to open %s for append\n", fn);
		exit(1);
	}
	/* stat doctable to get size */
	if (etymon_af_fstat(state->doctable_fd, &st) == -1) {
		perror("index_add_files():fstat()");
	}
	state->doctable_next_id = st.st_size / sizeof(ETYMON_DOCTABLE) + 1;

	/* open udict for read/write */
	etymon_db_construct_path(ETYMON_DBF_UDICT, dbname, fn);
	state->udict_fd = open(fn, O_RDWR | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (state->udict_fd == -1) {
		/* ERROR */
		printf("unable to open %s for read/write\n", fn);
		exit(1);
	}
	/* stat udict to get size */
	if (etymon_af_fstat(state->udict_fd, &st) == -1) {
		perror("index_add_files():fstat()");
	}
	state->udict_size = st.st_size;
	
	/* open upost for append */
	etymon_db_construct_path(ETYMON_DBF_UPOST, dbname, fn);
	state->upost_fd = open(fn, O_WRONLY | O_APPEND | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (state->upost_fd == -1) {
		/* ERROR */
		printf("unable to open %s for append\n", fn);
		exit(1);
	}
	/* stat upost to get size */
	if (etymon_af_fstat(state->upost_fd, &st) == -1) {
		perror("index_add_files():fstat()");
	}
	state->upost_isize = st.st_size / sizeof(ETYMON_INDEX_UPOST);
	
	/* open ufield for append */
	etymon_db_construct_path(ETYMON_DBF_UFIELD, dbname, fn);
	state->ufield_fd = open(fn, O_WRONLY | O_APPEND | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (state->ufield_fd == -1) {
		/* ERROR */
		printf("unable to open %s for append\n", fn);
		exit(1);
	}
	/* stat ufield to get size */
	if (etymon_af_fstat(state->ufield_fd, &st) == -1) {
		perror("index_add_files():fstat()");
	}
	state->ufield_isize = st.st_size / sizeof(ETYMON_INDEX_UFIELD);
	
	/* open uword for append */
	etymon_db_construct_path(ETYMON_DBF_UWORD, dbname, fn);
	state->uword_fd = open(fn, O_WRONLY | O_APPEND | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (state->uword_fd == -1) {
		/* ERROR */
		printf("unable to open %s for append\n", fn);
		exit(1);
	}
	/* stat uword to get size */
	if (etymon_af_fstat(state->uword_fd, &st) == -1) {
		perror("index_add_files():fstat()");
	}
	state->uword_isize = st.st_size / sizeof(ETYMON_INDEX_UWORD);
	
	/* allocate memory for page cache */
	state->pcache_nl_size = ETYMON_MAX_PAGE_DEPTH;
	memleft = maxmem - ((size_t) (sizeof(ETYMON_INDEX_PCACHE_NODE) * ETYMON_MAX_PAGE_DEPTH));
	if (memleft < 1048576) {
		memleft = 1048576;
	}
	state->pcache_nl = (ETYMON_INDEX_PCACHE_NODE*)(malloc(sizeof(ETYMON_INDEX_PCACHE_NODE) * ETYMON_MAX_PAGE_DEPTH));
	if (state->pcache_nl == NULL) {
		/* ERROR */
		printf("unable to allocate memory for cache\n");
		exit(1);
	}
	state->pcache_count = 0;
	state->pcache_nl[0].pos = 0;

	/* initialize the write cached leaf page */
	state->pcache_l_write = 0;

	/* turn on word numbering */
	state->phrase = dbinfo->phrase;
	state->word_proximity = dbinfo->word_proximity;
	state->stemming = dbinfo->stemming;
	if ( (dbinfo->phrase) || (dbinfo->word_proximity) ) {
		state->number_words = 1;
	} else {
		state->number_words = 0;
	}
	
	/* calculate cache memory allocation based on memleft */
	if (state->number_words) {
		wcache_alloc = (size_t) (memleft * .4);
		fcache_alloc = (size_t) (memleft * .3);
		wncache_alloc = (size_t) (memleft * .3);
	} else {
		wcache_alloc = (size_t) (memleft * .5);
		fcache_alloc = (size_t) (memleft * .5);
		wncache_alloc = 0;
	}
	/*
	if (wcache_alloc > 2000000000)
		wcache_alloc = 2000000000;
	if (fcache_alloc > 2000000000)
		fcache_alloc = 2000000000;
	if (wncache_alloc > 2000000000)
		wncache_alloc = 2000000000;
	*/
	
	/* allocate memory for word cache */
	state->wcache_size = wcache_alloc / ((size_t) sizeof(ETYMON_INDEX_WCACHE_NODE));
	state->wcache = (ETYMON_INDEX_WCACHE_NODE*)(malloc(sizeof(ETYMON_INDEX_WCACHE_NODE) * state->wcache_size));
	if (state->wcache == NULL) {
		/* ERROR */
		printf("unable to allocate memory for cache\n");
		exit(1);
	}
	state->wcache_count = 0;
	state->wcache_root = -1;

	/* allocate memory for field cache */
	state->fcache_size = fcache_alloc / ((size_t) sizeof(ETYMON_INDEX_FCACHE_NODE));
	state->fcache = (ETYMON_INDEX_FCACHE_NODE*)(malloc(sizeof(ETYMON_INDEX_FCACHE_NODE) * state->fcache_size));
	if (state->fcache == NULL) {
		/* ERROR */
		printf("unable to allocate memory for cache\n");
		exit(1);
	}
	state->fcache_count = 0;

	/* allocate memory for word number cache */
	if (state->number_words) {
		state->wncache_size = wncache_alloc / ((size_t) sizeof(ETYMON_INDEX_WNCACHE_NODE));
		state->wncache = (ETYMON_INDEX_WNCACHE_NODE*)(malloc(sizeof(ETYMON_INDEX_WNCACHE_NODE) *
								     state->wncache_size));
		if (state->wncache == NULL) {
			/* ERROR */
			printf("unable to allocate memory for cache\n");
			exit(1);
		}
		state->wncache_count = 0;
	}

	/* load field definitions into a binary tree */
	/* open fdef for read/write */
	etymon_db_construct_path(ETYMON_DBF_FDEF, dbname, fn);
	fdef_fd = open(fn, O_RDWR | O_CREAT | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (fdef_fd == -1) {
		/* ERROR */
		printf("unable to open %s for read/write\n", fn);
		exit(1);
	}
	state->fdef_count = etymon_af_fdef_read_mem(fdef_fd, &(state->fdef_root), &(state->fdef_tail));
	
	/* set dclass_id based on dclass string */
	if (strcmp(opt->doctype, "xml") == 0) {
		dclass_id = 1;
	}
	else if (strcmp(opt->doctype, "xml_test") == 0) {
		dclass_id = 2;
	}
	else if (strcmp(opt->doctype, "erc") == 0) {
		dclass_id = 100;
	} else {
		/* need to print an error here if the input is unknown
                   - right now it's being handled in af.cc which
                   is the wrong place */
		dclass_id = 0;
	}

	/* set up parameters to pass in to document class init
	   function */
	dc_init.use_docbuf = 1;
	dc_init.dc_state = NULL;

	/* for now we hard code calls */
	switch (dclass_id) {
	case 2:
		dc_index.dclass_id = 2;
		result = dc_xml_test_init(&dc_init);
		break;
	case 100:
		dc_index.dclass_id = 100;
		result = dc_erc_init(&dc_init);
		break;
	case 1:
#ifdef ETYMON_AF_XML
		dc_index.dclass_id = 1;
		result = dc_xml_init(&dc_init);
		break;
#endif
	default:
		dc_index.dclass_id = 0;
		result = dc_text_init(&dc_init);
	}
	if (result == -1) {
		free(state->wcache);
		free(state->fcache);
		if (state->number_words) {
			free(state->wncache);
		}
		free(state->pcache_nl);
		close(state->doctable_fd);
		close(state->udict_fd);
		close(state->upost_fd);
		close(state->ufield_fd);
		close(fdef_fd);
		etymon_af_fdef_free_mem(state->fdef_root);
		free(state);
		return -1;
	}

	use_docbuf = dc_init.use_docbuf;
	
	if (use_docbuf) {
		/* set up buffering for input documents */
		docbuf = (ETYMON_DOCBUF*)(malloc(sizeof(ETYMON_DOCBUF)));
		docbuf->buf = NULL; /* first time it will be NULL,
				       because we need to get
				       st_blksize from stat */
	} else {
		docbuf = NULL;
	}
		
	/* set up parameters to pass in to document class index function */
	dc_index.docbuf = docbuf;
	dc_index.filename = fn;
	dc_index.split_list = NULL;
	dc_index.dlevel = opt->dlevel;
	dc_index.state = state;
	dc_index.dc_state = dc_init.dc_state;

	/* get cwd */
	getcwd(cwd, ETYMON_MAX_PATH_SIZE);
	
	/* loop through and index each file */
	x_file = 0;
	done_files = 0;
	do {

		file_good = 1;
		
		/* load file into buffer */
		if (opt->_stdin) {
			if (fgets(s_file, ETYMON_MAX_PATH_SIZE, stdin) == NULL) {
				/* need to check fgets more correctly for errors */
				done_files = 1;
				break;
			} else {
				/* remove '\n' at end */
				x = strlen(s_file);
				if ( (x > 1) && (s_file[x - 1] == '\n') ) {
					s_file[x - 1] = '\0';
				}
				source_file = s_file;
			}
		} else {
			if (x_file == opt->sourcen) {
				done_files = 1;
				break;
			} else {
				source_file = opt->source[x_file];
			}
		}

		if (done_files) {
			break;
		}
		
		etymon_index_expand_path(source_file, fn, cwd);
			
		if (use_docbuf) {
			docbuf->fn = fn;
			docbuf->filedes = open(docbuf->fn, O_RDONLY | ETYMON_AF_O_LARGEFILE);
			if (docbuf->filedes == -1) {
				/*
				int e;
				char s[ETYMON_MAX_MSG_SIZE];
				sprintf(s, "%s: No such file or directory", docbuf->fn);
				file_good = 0;
				e = opt->log.error(s, 0);
				if (e != 0) {
					exit(e);
				}
				*/
			} else {

				/* stat the file */
				if (etymon_af_fstat(docbuf->filedes,
						    &(docbuf->st)) == -1) {
					perror("index_add_files():fstat()");
				}
				/* make sure it is a regular file */
				if (S_ISREG(docbuf->st.st_mode) == 0) {
					int e;
					char s[ETYMON_MAX_MSG_SIZE];
					sprintf(s,
						"%s: file not recognized: File format not recognized",
						docbuf->fn);
					file_good = 0;
					close(docbuf->filedes);
					/*
					e = opt->log.error(s, 0);
					if (e != 0) {
						exit(e);
					}
					*/
				}
				
			}
		}

		if (file_good) {
		
			state->flushmsg = 0;
			
			if (opt->verbose >= 1) {
				if (opt->verbose >= 2) {
					printf("Indexing ");
				}
				printf("%s\n", fn);
			}

			if (use_docbuf) {
				/* initialize the buffer if it hasn't been done */
				if (docbuf->buf == NULL) {
					docbuf->buf_size = docbuf->st.st_blksize;
					docbuf->buf = (unsigned char*)(malloc(docbuf->buf_size));
				}
				/* continue setting up to load the file */
				docbuf->eof = 0;
				/* read the first page from disk */
				etymon_docbuf_load_page(docbuf);
				
				/* ok, the docbuf page has been
				   prepared */
				/* check if we need to split the file
				   into multiple documents */
				if (*(opt->split) != '\0') {
					split_list =
						etymon_af_index_get_split_list(
							&dc_index, 
							(char *) opt->split);
					/* reset the docbuf page */
					if (etymon_af_lseek(docbuf->filedes,
							    (etymon_af_off_t)0, SEEK_SET) == -1) {
						perror("index_add_files():lseek()");
						exit(-1);
					}
					docbuf->eof = 0;
					etymon_docbuf_load_page(docbuf);
				} else {
					/* otherwise set up a single
					   node split list, marking
					   the end of the file */
					split_list =
						(ETYMON_AF_DC_SPLIT*)(malloc(
									      sizeof(ETYMON_AF_DC_SPLIT)));
					split_list->end = docbuf->st.st_size;
					split_list->next = NULL;
				}
				dc_index.split_list = split_list;
			}
			
			/* here we must call the indexing function in the doctype,
			   handing it a pointer to a struct of call back functions */
			/* for now we hard code calls */
			switch (dclass_id) {
			case 2:
				dc_index.dclass_id = 2;
				result = dc_xml_test_index(&dc_index);
				break;
			case 100:
				dc_index.dclass_id = 100;
				result = dc_erc_index(&dc_index);
				break;
			case 1:
#ifdef ETYMON_AF_XML
				dc_index.dclass_id = 1;
				result = dc_xml_index(&dc_index);
				break;
#endif
			default:
				dc_index.dclass_id = 0;
				result = dc_text_index(&dc_index);
			}
			/* free split list */
			while (split_list) {
				split_p = split_list;
				split_list = split_list->next;
				free(split_p);
			}
			/* check result from document class */
			if (result == -1) {
				free(state->wcache);
				free(state->fcache);
				if (state->number_words) {
					free(state->wncache);
				}
				free(state->pcache_nl);
				if (use_docbuf) {
					if (docbuf->buf != NULL) {
						free(docbuf->buf);
					}
					free(docbuf);
				}
				close(state->doctable_fd);
				close(state->udict_fd);
				close(state->upost_fd);
				close(state->ufield_fd);
				close(fdef_fd);
				etymon_af_fdef_free_mem(state->fdef_root);
				free(state);
				return -1;
			}
			
			if (use_docbuf) {
				/* close the document file */
				close(docbuf->filedes);
			}

		} /* if (file_good) */

		x_file++;

	} while (done_files == 0);
	
	if (etymon_index_dclass_finish(state) == -1) {
		free(state->wcache);
		free(state->fcache);
		if (state->number_words) {
			free(state->wncache);
		}
		free(state->pcache_nl);
		if (use_docbuf) {
			if (docbuf->buf != NULL) {
				free(docbuf->buf);
			}
			free(docbuf);
		}
		close(state->doctable_fd);
		close(state->udict_fd);
		close(state->upost_fd);
		close(state->ufield_fd);
		close(fdef_fd);
		etymon_af_fdef_free_mem(state->fdef_root);
		free(state);
		return -1;
	}

	/* write out fdef file */
	/* re-open fdef and overwrite */
	close(fdef_fd);
	etymon_db_construct_path(ETYMON_DBF_FDEF, dbname, fn);
	fdef_fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
	if (fdef_fd == -1) {
		/* ERROR */
		printf("unable to open %s for read/write\n", fn);
		exit(1);
	}
	etymon_af_fdef_write_mem(fdef_fd, state->fdef_root);
	
	/* update dbinfo */
/*
	if (etymon_af_lseek(dbinfo_fd, (etymon_af_off_t)0, SEEK_SET) == -1) {
		perror("index_add_files():lseek()");
	}
	magic = ETYMON_INDEX_MAGIC;
	nbytes = write(dbinfo_fd, &magic, sizeof(Uint4));
	if (nbytes != sizeof(Uint4)) {
		printf("unable to write MN\n");
		exit(1);
	}
	dbinfo.udict_root = state->udict_root;
	dbinfo.doc_n = state->doc_n;
	nbytes = write(dbinfo_fd, &dbinfo, sizeof(ETYMON_DB_INFO));
	if (nbytes != sizeof(ETYMON_DB_INFO)) {
		printf("unable to write DBI\n");
		exit(1);
	}
	close(dbinfo_fd);
*/
	dbinfo->udict_root = state->udict_root;
	dbinfo->doc_n = state->doc_n;
	
	/* clean up */
	free(state->wcache);
	free(state->fcache);
	if (state->number_words) {
		free(state->wncache);
	}
	free(state->pcache_nl);
	if (use_docbuf) {
		if (docbuf->buf != NULL) {
			free(docbuf->buf);
		}
		free(docbuf);
	}
	close(state->doctable_fd);
	close(state->udict_fd);
	close(state->upost_fd);
	close(state->ufield_fd);
	close(fdef_fd);
	etymon_af_fdef_free_mem(state->fdef_root);
	free(state);

	/* unlock the database */
	etymon_db_unlock(dbname);

	return 0;
}


int etymon_af_index_add_word(ETYMON_AF_INDEX_ADD_WORD* opt) {
	ETYMON_INDEX_INDEXING_STATE* state = opt->state;
	int tree_p, comp, field_p;
	int* tree_link;
	int done;
	int full;

	if (state->verbose >= 5) {
		afprintvp(state->verbose, 5);
		printf("Adding word: \"%s\"\n", (const char *) opt->word);
	}
	
	/*
	size_t tmp_len = strlen((const char*)opt->word);
	char tmp_word[1024];
	strcpy(tmp_word, (const char*)opt->word);
	af_stem(opt->word);
	if (strlen((const char*)opt->word) != tmp_len) {
		printf("%s -> ", tmp_word);
		printf("%s\n", (const char*)opt->word);
	}
	*/
	if (state->stemming)
		af_stem(opt->word);
	
	/* if any caches are full, then index this block and clear the caches */
	if (state->number_words) {
		full = (state->wcache_count == state->wcache_size) ||
			(state->fcache_count == state->fcache_size) ||
			(state->wncache_count == state->wncache_size);
	} else {
		full = (state->wcache_count == state->wcache_size) ||
			(state->fcache_count == state->fcache_size);
	}
	if (full) {
		if (etymon_index_dclass_index(state) == -1) {
			return -1;
		}
		state->wcache_count = 0;
		state->wcache_root = -1;
		state->fcache_count = 0;
		state->wncache_count = 0;
	}

	/* add the new word to the cache */

	/* search the binary tree for a matching word
	   - tree_p will end up the index of the matching node, or -1 if no match exists
	   - tree_link will end up pointing to the parent link (or the root pointer) */
	
	tree_link = &(state->wcache_root);
	tree_p = state->wcache_root;
	comp = -1;
	while ( (comp != 0) && (tree_p != -1) ) {
		comp = strcmp((char*)(opt->word), (char*)(state->wcache[tree_p].word));
		if (comp < 0) {
			tree_link = &(state->wcache[tree_p].left);
			tree_p = *tree_link;
		}
		else if (comp > 0) {
			tree_link = &(state->wcache[tree_p].right);
			tree_p = *tree_link;
		}
	}

	if (tree_p == -1) {
		/* if there was no match, we create a new node */
		memcpy(state->wcache[state->wcache_count].word, opt->word, ETYMON_MAX_WORD_SIZE);
		state->wcache[state->wcache_count].left = -1;
		state->wcache[state->wcache_count].right = -1;
		state->wcache[state->wcache_count].next = state->wcache_count; /* here next points to the tail */
		state->wcache[state->wcache_count].freq = 1;
		state->wcache[state->wcache_count].doc_id = opt->doc_id;
		/* add new node to field cache */
		if (opt->fields[0] != 0) {
			memcpy(state->fcache[state->fcache_count].f, opt->fields, ETYMON_MAX_FIELD_NEST * 2);
			state->fcache[state->fcache_count].next = -1;
			state->wcache[state->wcache_count].fields = state->fcache_count;
			state->fcache_count++;
		} else {
			state->wcache[state->wcache_count].fields = -1;
		}
		/* add new node to word number cache */
		if (state->number_words) {
			state->wncache[state->wncache_count].wn = opt->word_number;
			state->wncache[state->wncache_count].next = -1;
			state->wcache[state->wcache_count].word_numbers_head = state->wncache_count;
			state->wcache[state->wcache_count].word_numbers_tail = state->wncache_count;
			state->wncache_count++;
		} else {
			state->wcache[state->wcache_count].word_numbers_head = -1;
			state->wcache[state->wcache_count].word_numbers_tail = -1;
		}
		/* update parent node in binary tree */
		if (tree_link != NULL) {
			*tree_link = state->wcache_count;
		}
		state->wcache_count++;
	} else {
		/* there was a word match, so now we check if the doc_id's match */
		if (opt->doc_id == state->wcache[tree_p].doc_id) {
			/* doc_id's match, so we simply increment the frequency */
			state->wcache[tree_p].freq++;
			/* now add new node to field cache if there is no matching field */
			if (opt->fields[0] != 0) {
				/* search for a matching field */
				field_p = state->wcache[tree_p].fields;
				done = 0;
				while ( (done == 0) && (field_p != -1) ) {
					if (memcmp(state->fcache[field_p].f, opt->fields,
						   ETYMON_MAX_FIELD_NEST * 2) != 0) {
						field_p = state->fcache[field_p].next;
					} else {
						done = 1;
					}
				}
				if (field_p == -1) {
					/* no match, so add a new field node */
					memcpy(state->fcache[state->fcache_count].f, opt->fields,
					       ETYMON_MAX_FIELD_NEST * 2);
					state->fcache[state->fcache_count].next = state->wcache[tree_p].fields;
					state->wcache[tree_p].fields = state->fcache_count;
					state->fcache_count++;
				}
			}
			/* now add new node to word number cache at end of list */
			if (state->number_words) {
				/* add a new word number node */
				state->wncache[state->wncache_count].wn = opt->word_number;
				state->wncache[state->wcache[tree_p].word_numbers_tail].next = state->wncache_count;
				state->wncache[state->wncache_count].next = -1;
				state->wcache[tree_p].word_numbers_tail = state->wncache_count;
				state->wncache_count++;
			}
		} else {
			/* doc_id's don't match, so we create a new node in the binary tree */
			memcpy(state->wcache[state->wcache_count].word, opt->word, ETYMON_MAX_WORD_SIZE);
			state->wcache[state->wcache_count].left = state->wcache[tree_p].left;
			state->wcache[state->wcache_count].right = state->wcache[tree_p].right;
			state->wcache[state->wcache_count].next = state->wcache[tree_p].next; /* tail */
			state->wcache[tree_p].next = state->wcache_count; /* to point back to the new node */
			state->wcache[state->wcache_count].freq = 1;
			state->wcache[state->wcache_count].doc_id = opt->doc_id;
			/* add new node to field cache */
			if (opt->fields[0] != 0) {
				memcpy(state->fcache[state->fcache_count].f, opt->fields, ETYMON_MAX_FIELD_NEST * 2);
				state->fcache[state->fcache_count].next = -1;
				state->wcache[state->wcache_count].fields = state->fcache_count;
				state->fcache_count++;
			} else {
				state->wcache[state->wcache_count].fields = -1;
			}
			/* add new node to word number cache */
			if (state->number_words) {
				state->wncache[state->wncache_count].wn = opt->word_number;
				state->wncache[state->wncache_count].next = -1;
				state->wcache[state->wcache_count].word_numbers_head = state->wncache_count;
				state->wcache[state->wcache_count].word_numbers_tail = state->wncache_count;
				state->wncache_count++;
			} else {
				state->wcache[state->wcache_count].word_numbers_head = -1;
				state->wcache[state->wcache_count].word_numbers_head = -1;
			}
			/* update parent node in binary tree */
			if (tree_link != NULL) {
				*tree_link = state->wcache_count;
			}
			state->wcache_count++;
		}
	}

	return 0;
}


Uint4 etymon_af_index_add_doc(ETYMON_AF_INDEX_ADD_DOC* opt) {
	ETYMON_INDEX_INDEXING_STATE* state = opt->state;
	ssize_t nbytes;

	/* fill in doctable entry with new data */
	if (opt->key == NULL) {
		/* fill in default key, based on doctable id */
		/*
		snprintf((char*)(state->doctable.key), ETYMON_MAX_KEY_SIZE, "%ld",
			(unsigned long)(state->doctable_next_id));
		*/
		state->doctable.key[0] = '\0';
	} else {
		strncpy((char*)(state->doctable.key), (char*)(opt->key), ETYMON_MAX_KEY_SIZE - 1);
		state->doctable.key[ETYMON_MAX_KEY_SIZE - 1] = '\0';
	}
	strncpy(state->doctable.filename, opt->filename, ETYMON_MAX_PATH_SIZE - 1);
	state->doctable.filename[ETYMON_MAX_PATH_SIZE - 1] = '\0';
	state->doctable.begin = opt->begin;
	state->doctable.end = opt->end;
	state->doctable.parent = opt->parent;
	state->doctable.dclass_id = opt->dclass_id;
	state->doctable.deleted = 0;
	/* write out doctable entry */
	nbytes = write(state->doctable_fd, &(state->doctable), sizeof(ETYMON_DOCTABLE));
	if (nbytes != sizeof(ETYMON_DOCTABLE)) {
		/* ERROR */
		printf("error writing to file in etymon_index_dclass_add_doc\n");
		exit(1);
	}
	/* increment count of total number of (non-deleted) documents in database */
	state->doc_n++;
	/* increment next counter */
	return state->doctable_next_id++;
}


/* need to change this function prototype to conform to other document
   class call-backs */
Uint4 etymon_index_dclass_get_next_doc_id(ETYMON_INDEX_INDEXING_STATE* state) {
	return state->doctable_next_id;
}
