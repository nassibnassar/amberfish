/*
 *  This is a temporary holding place for definitions that don't yet
 *  have a home.
 */

#ifndef _AF_DEFS_H
#define _AF_DEFS_H

/* new */

#include <stdio.h>
#include "err.h"

/* database file type */

#define AFFTINFO         (0)
#define AFFTDOCTAB       (1)
#define AFFTUDICT        (2)
#define AFFTUPOST        (3)
#define AFFTUFIELD       (4)
#define AFFTLPOST        (5)
#define AFFTLFIELD       (6)
#define AFFTFDEF         (7)
#define AFFTUWORD        (8)
#define AFFTLWORD        (9)
#define AFFTLOCK        (10)

static const char *affntab[] = {
	".db",   /* AFFTINFO */
	".dt",   /* AFFTDOCTAB */
	".x0",   /* AFFTUDICT */
	".y0",   /* AFFTUPOST */
	".z0",   /* AFFTUFIELD */
	".y1",   /* AFFTLPOST */
	".z1",   /* AFFTLFIELD */
	".fd",   /* AFFTFDEF */
	".w0",   /* AFFTUWORD */
	".w1",   /* AFFTLWORD */
	".lk",   /* AFFTLOCK */
};
#ifdef NOTHING
static const char *ftfn[] = {
	ETYMON_DBF_INFO_EXT,       /* AFFTINFO */
	ETYMON_DBF_DOCTAB_EXT,   /* AFFTINFO */
	ETYMON_DBF_UDICT_EXT,      /* AFFTUDICT */
	ETYMON_DBF_UPOST_EXT,      /* AFFTUPOST */
	ETYMON_DBF_UFIELD_EXT,     /* AFFTUFIELD */
	ETYMON_DBF_LPOST_EXT,      /* AFFTLPOST */
	ETYMON_DBF_LFIELD_EXT,     /* AFFTLFIELD */
	ETYMON_DBF_FDEF_EXT,       /* AFFTFDEF */
	ETYMON_DBF_UWORD_EXT,      /* AFFTUWORD */
	ETYMON_DBF_LWORD_EXT,      /* AFFTLWORD */
	ETYMON_DBF_LOCK_EXT,       /* AFFTLOCK */
};
#endif

typedef struct {
	FILE *info;
	FILE *doctab;
	FILE *udict;
	FILE *upost;
	FILE *ufield;
	FILE *lpost;
	FILE *lfield;
	FILE *fdef;
	FILE *uword;
	FILE *lword;
	FILE *lock;
} Affile;

/* old */

#include <stdlib.h>
#include <fcntl.h>
#include "config.h"

/* I'd like to move this to where it is used and reduce the number of
   times it is used. */
#define ETYMON_AF_BANNER "Amberfish, Version " AF_VERSION
#define ETYMON_AF_COPYRIGHT "Copyright (C) 1999-2004 Etymon Systems, Inc.  All Rights Reserved."
#define ETYMON_AF_BANNER_STAMP ETYMON_AF_BANNER ".  " ETYMON_AF_COPYRIGHT

/* Uint4 at beginning of db info file to indicate index version, used
 * to determine compatibility; increment it here if the index format
 * changes */
#define ETYMON_INDEX_MAGIC (5)

/* maximum char[] size for an absolute path */
#define AFPATHSIZE (1024)
#define ETYMON_MAX_PATH_SIZE AFPATHSIZE

/* maximum char[] key size */
/* 11 is big enough to hold the default Uint4 keys */
#define ETYMON_MAX_KEY_SIZE (11)

/* maximum char[] size for a field name (not an entire field path) */
#define ETYMON_AF_MAX_FIELDNAME_SIZE (32)

/* maximum char[] size for an error message */
#define ETYMON_MAX_MSG_SIZE (1024)

#define ETYMON_DBF_INFO AFFTINFO
#define ETYMON_DBF_INFO_EXT getftfn(AFFTINFO)
#define ETYMON_DBF_DOCTABLE AFFTDOCTAB
#define ETYMON_DBF_DOCTABLE_EXT getftfn(AFFTDOCTAB)
#define ETYMON_DBF_UDICT AFFTUDICT
#define ETYMON_DBF_UDICT_EXT getftfn(AFFTUDICT)
#define ETYMON_DBF_UPOST AFFTUPOST
#define ETYMON_DBF_UPOST_EXT getftfn(AFFTUPOST)
#define ETYMON_DBF_UFIELD AFFTUFIELD
#define ETYMON_DBF_UFIELD_EXT getftfn(AFFTUFIELD)
#define ETYMON_DBF_LPOST AFFTLPOST
#define ETYMON_DBF_LPOST_EXT getftfn(AFFTLPOST)
#define ETYMON_DBF_LFIELD AFFTLFIELD
#define ETYMON_DBF_LFIELD_EXT getftfn(AFFTLFIELD)
#define ETYMON_DBF_FDEF AFFTFDEF
#define ETYMON_DBF_FDEF_EXT getftfn(AFFTFDEF)
#define ETYMON_DBF_UWORD AFFTUWORD
#define ETYMON_DBF_UWORD_EXT getftfn(AFFTUWORD)
#define ETYMON_DBF_LWORD AFFTLWORD
#define ETYMON_DBF_LWORD_EXT getftfn(AFFTLWORD)
#define ETYMON_DBF_LOCK AFFTLOCK
#define ETYMON_DBF_LOCK_EXT getftfn(AFFTLOCK)

#define ETYMON_DB_PERM (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

typedef struct {
        unsigned char key[ETYMON_MAX_KEY_SIZE]; /* document key */
        char filename[ETYMON_MAX_PATH_SIZE]; /* document source file name */
	etymon_af_off_t begin; /* starting offset of document within the file */
        etymon_af_off_t end; /* ending offset of document within the file (one byte past end) */
	Uint4 parent;  /* doc_id of parent document */
        Uint1 dclass_id; /* unique id associated with dclass */
	Uint1 deleted; /* 1 = marked as deleted; 0 = not deleted */
} ETYMON_DOCTABLE;

typedef struct {
	unsigned char name[ETYMON_AF_MAX_FIELDNAME_SIZE];
	Uint2 left;
	Uint2 right;
} ETYMON_AF_FDEF_DISK;

typedef struct {
	int (*error)(char*, int);
} ETYMON_LOG;

/* maximum number of keys in a page */
#define ETYMON_MAX_KEYS_L (5000)
#define ETYMON_MAX_KEYS_NL (8000)

#ifdef UMLS
#define ETYMON_MAX_TOKEN_SIZE (256)
#define ETYMON_MAX_WORD_SIZE (256)
#else

/* maximum char[] size for a parsed token */
/* this must be set to the larger of ETYMON_MAX_WORD_SIZE and ETYMON_MAX_FIELDNAME_SIZE */
#define ETYMON_MAX_TOKEN_SIZE (32)

/* maximum char[] size for an indexable word */
#define ETYMON_MAX_WORD_SIZE (32)

#endif  /* UMLS */

/* maximum char[] size for a single term within a query */
#define ETYMON_MAX_QUERY_TERM_SIZE (1024)

/* maximum level of nesting in structured fields */
#define ETYMON_MAX_FIELD_NEST (64)

/* maxmium depth (levels) of tree allowed */
#define ETYMON_MAX_PAGE_DEPTH (4)

/* average key length */
#define ETYMON_MEAN_KEY_LEN_L (8)
#define ETYMON_MEAN_KEY_LEN_NL (5)

/* size of key buffer in pages */
#define ETYMON_MAX_KEY_AREA_L (ETYMON_MAX_KEYS_L * ETYMON_MEAN_KEY_LEN_L)
#define ETYMON_MAX_KEY_AREA_NL (ETYMON_MAX_KEYS_NL * ETYMON_MEAN_KEY_LEN_NL)

#define ETYMON_AF_MAX_OP_STACK_DEPTH (256)
#define ETYMON_AF_MAX_R_STACK_DEPTH (256)

#define ETYMON_AF_OP_OR (1)
#define ETYMON_AF_OP_AND (2)
#define ETYMON_AF_OP_GROUP_OPEN (3)
#define ETYMON_AF_OP_GROUP_CLOSE (4)

typedef struct {
	Uint2 n; /* number of keys */
	Uint4 p[ETYMON_MAX_KEYS_NL + 1]; /* pointers to other pages (offset) */
	Uint2 offset[ETYMON_MAX_KEYS_NL + 1]; /* offsets to keys */
	unsigned char keys[ETYMON_MAX_KEY_AREA_NL]; /* key buffer */
} ETYMON_INDEX_PAGE_NL;

typedef struct {
	Uint2 n; /* number of keys */
	Uint4 prev; /* previous leaf node (offset) */
	Uint4 next; /* next left node (offset) */
	Uint4 post[ETYMON_MAX_KEYS_L]; /* postings for each key */
	Uint4 post_n[ETYMON_MAX_KEYS_L]; /* number of postings for each key */
	Uint2 offset[ETYMON_MAX_KEYS_L + 1]; /* offsets to keys */
	unsigned char keys[ETYMON_MAX_KEY_AREA_L]; /* key buffer */
} ETYMON_INDEX_PAGE_L;

typedef struct {
	Uint4 pos; /* position of page on disk or 0 if empty slot */
	Uint1 is_nl; /* is it a non-leaf (here) or leaf (in the leaf cache) */
	ETYMON_INDEX_PAGE_NL nl;
} ETYMON_INDEX_PCACHE_NODE;

typedef struct {
	Uint2 f[ETYMON_MAX_FIELD_NEST];
	int next;
} ETYMON_INDEX_FCACHE_NODE;

typedef struct {
	Uint4 wn;
	int next;
} ETYMON_INDEX_WNCACHE_NODE;

typedef struct {
	unsigned char word[ETYMON_MAX_WORD_SIZE];
	int left;
	int right;
	int next; /* circular linked list */
	Uint2 freq;
	Uint4 doc_id;
	int fields;
	int word_numbers_head;
	int word_numbers_tail;
} ETYMON_INDEX_WCACHE_NODE;

typedef struct {
	Uint4 doc_id; /* document id */
	Uint2 freq; /* frequency */
	Uint4 fields; /* field pointer */
	Uint4 fields_n; /* number of fields */
	Uint4 word_numbers; /* word numbers pointer */
	Uint4 word_numbers_n; /* number of word numbers */
	Uint4 next; /* next posting (index) or 0 */
} ETYMON_INDEX_UPOST;

typedef struct {
	Uint4 doc_id; /* document id */
	Uint2 freq; /* frequency */
	Uint4 fields; /* field pointer */
	Uint4 fields_n; /* number of fields */
	Uint4 word_numbers; /* word numbers pointer */
	Uint4 word_numbers_n; /* number of word numbers */
} ETYMON_INDEX_LPOST;

typedef struct {
	Uint2 fields[ETYMON_MAX_FIELD_NEST];
	Uint4 next; /* next field (index) or 0 */
} ETYMON_INDEX_UFIELD;

typedef struct {
	Uint4 wn;
	Uint4 next; /* next word number (index) or 0 */
} ETYMON_INDEX_UWORD;

typedef struct {
	Uint2 fields[ETYMON_MAX_FIELD_NEST];
} ETYMON_INDEX_LFIELD;

typedef struct {
	Uint4 wn;
} ETYMON_INDEX_LWORD;

typedef struct ETYMON_AF_FDEF_MEM_STRUCT {
	Uint2 n;
	unsigned char name[ETYMON_AF_MAX_FIELDNAME_SIZE];
	struct ETYMON_AF_FDEF_MEM_STRUCT* left;
	struct ETYMON_AF_FDEF_MEM_STRUCT* right;
	struct ETYMON_AF_FDEF_MEM_STRUCT* next;
} ETYMON_AF_FDEF_MEM;

typedef struct {
	char* dbname; /* database name */
	int doctable_fd; /* doctable file descriptor */
	etymon_af_off_t doctable_next_id; /* next available doctable number */
	ETYMON_DOCTABLE doctable; /* doctable buffer to use repeatedly */
	ETYMON_INDEX_WCACHE_NODE* wcache; /* binary tree (array) of word cache nodes */
	size_t wcache_size;
	size_t wcache_count;
	int wcache_root;
	ETYMON_INDEX_FCACHE_NODE* fcache; /* linked list (array) of field cache nodes */
	size_t fcache_size;
	size_t fcache_count;
	ETYMON_INDEX_WNCACHE_NODE* wncache; /* linked list (array) of word number cache nodes */
	size_t wncache_size;
	size_t wncache_count;
	int udict_fd; /* udict file descriptor */
	etymon_af_off_t udict_size; /* current size of udict */
	int upost_fd; /* upost file descriptor */
	etymon_af_off_t upost_isize; /* current size of upost */
	int ufield_fd; /* ufield file descriptor */
	etymon_af_off_t ufield_isize; /* current size of ufield */
	int uword_fd; /* uword file descriptor */
	etymon_af_off_t uword_isize; /* current size of uword */
	Uint4 udict_root; /* root of the udict tree (offset) */
	ETYMON_INDEX_PCACHE_NODE* pcache_nl; /* non-leaf page cache */
	ETYMON_INDEX_PAGE_L pcache_l; /* leaf page cache */
	Uint4 pcache_l_write; /* offset position for write caching, or 0 if pcache_l has been flushed */
	int pcache_nl_size;
	int pcache_count;
	ETYMON_INDEX_PAGE_NL overflow_nl; /* overflow non-leaf page */
	ETYMON_INDEX_PAGE_L overflow_l; /* overflow leaf page */
	ETYMON_INDEX_PAGE_L extra_l; /* extra leaf page */
	ETYMON_INDEX_UPOST upost;
	ETYMON_INDEX_UFIELD ufield;
	ETYMON_INDEX_UWORD uword;
	ETYMON_AF_FDEF_MEM* fdef_root; /* pointer to root node of fdef binary tree */
	ETYMON_AF_FDEF_MEM* fdef_tail; /* pointer to tail node of fdef threaded list */
	Uint2 fdef_count;
	int phrase; /* enable phrase searching */
	int word_proximity; /* enable word proximity operator */
	int stemming; /* enable stemming */
	int number_words; /* enable recordings of word number data */
	int doc_n; /* total number of (non-deleted) documents in
		     database */
	int verbose;
	int long_words;
	int flushmsg;
} ETYMON_INDEX_INDEXING_STATE;

#endif
