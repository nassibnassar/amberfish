#ifndef _AF_SEARCH_H
#define _AF_SEARCH_H

/***** new *****/

#include <fcntl.h>
#include "config.h"
#include "defs.h"

#define AFQUERYBOOLEAN (1)
#define AFQUERYVECTOR  (2)

#define AFNOSCORE      (0)
#define AFSCOREBOOLEAN (1)
#define AFSCOREVECTOR  (2)

typedef struct {
        Uint4 docid;
        Uint4 score;
        Uint2 dbid;
} Afresult;

typedef struct {
	Uint4 parent;  /* docid of parent document */
        char docpath[AFPATHSIZE];  /* document source file name */
	off_t begin;  /* starting offset of document within the file */
        off_t end;  /* ending offset of document within the file (one byte past end) */
	Uint1 deleted;
} Afresultmd;

typedef struct {
	Uint2 *dbid;  /* databases to search */
	int dbidn;
	Afchar *query;
	int qtype;  /* AFQUERYBOOLEAN | AFQUERYVECTOR */
	int score;  /* AFNOSCORE | AFSCOREBOOLEAN | AFSCOREVECTOR */
	int score_normalize;  /* if scoring, scale scores from 0 to 10000 */
} Afsearch;

typedef struct {
        Afresult *result;
	int resultn;
} Afsearch_r;

int afsearch(const Afsearch *r, Afsearch_r *rr);

int afsortscore(Afresult *result, int resultn);

int afsortdocid(Afresult *result, int resultn);

int afgetresultmd(const Afresult *result, int resultn, Afresultmd *resultmd);

/***** old *****/

#include "defs.h"
#include "log.h"
#include "info.h"

typedef struct {
	ETYMON_LOG* log;
	ETYMON_DB_INFO dbinfo;
	int doctable_fd;
	int udict_fd;
	int fdef_fd;
	int upost_fd;
	int ufield_fd;
	int lpost_fd;
	int lfield_fd;
	ETYMON_AF_FDEF_DISK* fdef_disk;
} ETYMON_SEARCH_SEARCHING_STATE;

/*
typedef struct {
	int db_id;
	Uint4 doc_id;
	Uint2 score;
} ETYMON_AF_RESULT;
*/

/*
enum etymon_af_scoring_methods { ETYMON_AF_UNSCORED = 0,
				 ETYMON_AF_SCORE_DEFAULT };

enum etymon_af_sorting_methods { ETYMON_AF_UNSORTED = 0,
				 ETYMON_AF_SORT_SCORE };
*/

/*
typedef struct {
	int* db_id;
	unsigned char* query;
	int score_results;
	int sort_results;
	ETYMON_AF_LOG* log;
	ETYMON_AF_RESULT* results;
	int results_n;
} ETYMON_AF_SEARCH;
*/

/*
typedef struct {
	Uint4 parent;  / doc_id of parent document /
        char* filename; / document source file name /
	etymon_af_off_t begin; / starting offset of document within the file /
        etymon_af_off_t end; / ending offset of document within the file (one byte past end) /
} ETYMON_AF_ERESULT;
*/

typedef struct {
	Uint2 dbid;
	Afsearch* opt;
	Afsearch_r* optr;
	Uint4 corpus_doc_n;
} ETYMON_AF_SEARCH_STATE;

/*int etymon_af_search(ETYMON_AF_SEARCH* opt);*/

/*int etymon_af_search_term_compare(const void* a, const void* b);*/

/*
int etymon_af_resolve_results(Afresult* results, int results_n, ETYMON_AF_ERESULT* resolved_results);
*/

#endif
