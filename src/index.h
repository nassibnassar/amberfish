#ifndef _AF_INDEX_H
#define _AF_INDEX_H

/***** new *****/

#include "config.h"

typedef struct {
	Uint2 dbid;
	int memory;  /* maximum amount of memory to use during indexing (MB) */
	char **source;  /* list of files to index */
	int sourcen;
	char *doctype;
	Afchar *split;  /* delimiter of multiple documents (if any)
                           within a file */
	int dlevel;  /* maximum number of levels to descend (nested documents) */
	int verbose;  /* boolean: verbose output */
	int _stdin;  /* boolean: read files to index from standard input */
	int _longwords;
} Afindex;

typedef struct {
	int _tmp;
} Afindex_r;

/***** old *****/

#include "defs.h"
#include "docbuf.h"

#ifdef ZZZZZ
typedef struct {
	ETYMON_LOG log;
	char* dbname; /* database name */
	int memory; /* maximum amount of memory to use during indexing (MB) */
	int dlevel; /* maximum number of levels to descend (nested documents) */
	char* dclass; /* document class */
	char** files; /* list of files to index */
	int files_n;
	int files_stdin; /* boolean: read files to index from standard input */
/*	int phrase; // boolean: enable phrase search */
/*	int word_proximity; // boolean: enable word proximity operator */
	char* split; /* delimiter of multiple documents (if any)
                        within a file */
	int verbose; /* boolean: verbose output */
	char* dc_options; /* document class options */
	int long_words;
} ETYMON_INDEX_OPTIONS;
#endif

typedef struct {
	unsigned char* key; /* document key */
	char* filename; /* document source file name */
	etymon_af_off_t begin; /* starting offset of document within the file */
	etymon_af_off_t end; /* ending offset of document within the file (one byte past end) */
	Uint4 parent;  /* doc_id of parent document */
	Uint1 dclass_id; /* unique id associated with dclass */
	ETYMON_INDEX_INDEXING_STATE* state;
} ETYMON_AF_INDEX_ADD_DOC;

typedef struct {
	Uint4 doc_id; /* unique id associated with dclass */
	unsigned char* word; /* word to add to the index, must be unsigned char[ETYMON_MAX_WORD_SIZE] */
	Uint2* fields; /* array representing field location, must be Uint2[ETYMON_MAX_FIELD_NEST] */
	Uint4 word_number; /* word position, starting with 1 */
	ETYMON_INDEX_INDEXING_STATE* state;
} ETYMON_AF_INDEX_ADD_WORD;

typedef struct {
	int use_docbuf;  /* 1: read files via docbuf;
			    0: don't use docbuf; this will also
			       disable splitting files */
	void* dc_state;
} ETYMON_AF_DC_INIT;

typedef struct ETYMON_AF_DC_SPLIT_STRUCT {
	etymon_af_off_t end;  /* ending offset of document within the
				 file (one byte past end) */
	struct ETYMON_AF_DC_SPLIT_STRUCT* next;
} ETYMON_AF_DC_SPLIT;

typedef struct {
	ETYMON_DOCBUF* docbuf;
	char* filename;
	ETYMON_AF_DC_SPLIT* split_list;
	int dlevel;  /* maximum number of levels to descend (nested
			documents) */
	Uint1 dclass_id;
	ETYMON_INDEX_INDEXING_STATE* state;
	void* dc_state;
} ETYMON_AF_DC_INDEX;

/* Used by search.cc as well; should it be moved out of index.cc? */
int etymon_index_search_keys_nl(unsigned char* word, size_t word_len, ETYMON_INDEX_PAGE_NL* page);

/* Used by search.cc as well; should it be moved out of index.cc? */
int etymon_index_search_keys_l(unsigned char* word, size_t word_len, ETYMON_INDEX_PAGE_L* page, int* match);

int etymon_index_add_files(Afindex *opt);

#ifdef ZZZZZ
int etymon_index_optimize_old(ETYMON_INDEX_OPTIONS* opt);
#endif

Uint4 etymon_af_index_add_doc(ETYMON_AF_INDEX_ADD_DOC* opt);

int etymon_af_index_add_word(ETYMON_AF_INDEX_ADD_WORD* opt);

Uint4 etymon_index_dclass_get_next_doc_id(ETYMON_INDEX_INDEXING_STATE* state);

int etymon_index_valid_word(unsigned char* word);

#endif
