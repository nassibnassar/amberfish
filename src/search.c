/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include "search.h"
#include "open.h"
#include "fdef.h"
#include "index.h"
#include "stem.h"

char *afstrdup(const char *s)
{
	int n;
	char *t;

	n = strlen(s) + 1;
	t = (char *) malloc(n);
	if (t)
		memcpy(t, s, n);
	return t;
}

extern ETYMON_AF_STATE* etymon_af_state[];

typedef struct {
	int doc_id;
	float w_d;  /* w{qj} d{ij} */
	float d2;  /* (d{ij})^2 */
} ETYMON_AF_IRESULT;

typedef struct {
	ETYMON_INDEX_LWORD* wlist;
	Uint4 wn_n;
	Uint1* new_word;
} ETYMON_AF_R0_WN;


typedef struct {
	Uint4 doc_id;
	Uint1 new_doc; /* only used during intersect phases (word_counter > 0) */
	ETYMON_AF_R0_WN* wn;
	Uint2 freq;
} ETYMON_AF_R0;


void etymon_af_search_free_r0(ETYMON_AF_R0* r0, int r0_size, int r0_wn_size) {
	int x, y;
	
	if (r0) {
		if (r0[0].wn) {
			for (x = 0; x < r0_size; x++) {
				for (y = 0; y < r0_wn_size; y++) {
					if (r0[x].wn[y].wlist) {
						free(r0[x].wn[y].wlist);
					}
					if (r0[x].wn[y].new_word) {
						free(r0[x].wn[y].new_word);
					}
				}
				free(r0[x].wn);
			}
		}
		free(r0);
	}
}


int etymon_af_search_term_compare_docid(const void* a, const void* b) {
	int x;
	
	x = ((Afresult*)a)->dbid - ((Afresult*)b)->dbid;
	if (x != 0) {
		return x;
	}
	return ((Afresult*)a)->docid - ((Afresult*)b)->docid;
}


int etymon_af_search_term_compare_score(const void* a, const void* b) {
	return ((Afresult*)b)->score - ((Afresult*)a)->score;
}


int etymon_af_search_iresult_compare_reverse(const void* a, const void* b) {
	return ((ETYMON_AF_IRESULT*)b)->doc_id - ((ETYMON_AF_IRESULT*)a)->doc_id;
}


int etymon_af_search_r0_compare_reverse(const void* a, const void* b) {
	return ((ETYMON_AF_R0*)b)->doc_id - ((ETYMON_AF_R0*)a)->doc_id;
}


int etymon_af_search_term_lword_compare(const void* a, const void* b) {
	return *((Uint4*)a) - ((ETYMON_INDEX_LWORD*)b)->wn;
}


/* searches in fields[] using field_mask[] as the query */
int etymon_af_search_fields(Uint2* field_mask, int field_mask_len, int rooted, Uint2* fields) {
	int wild_card, mask_p, field_p;

	/* start off with wild_card turned on if rooted == 0 */
	if (rooted) {
		wild_card = 0;
	} else {
		wild_card = 1;
	}

	/* loop through each state in the field_mask[], testing for a
           match in fields[] */
	field_p = 0;
	for (mask_p = 0; mask_p < field_mask_len; mask_p++) {
		/* test for the special "..." case */
		if (field_mask[mask_p] == 0) {
			/* turn on wild_card for the next iteration */
			wild_card = 1;
		} else {
			/* otherwise we test for a match */
			if (wild_card == 0) {
				/* this case is simple: there must be a perfect match */
				/* first make sure we haven't run off the end of fields[] */
				if ( (field_p >= ETYMON_MAX_FIELD_NEST) || (fields[field_p] == 0) ) {
					return 0;
				}
				if (field_mask[mask_p] != fields[field_p]) {
					return 0;
				}
				field_p++;
			} else {
				/* with wild_card turned on, we need to search
				   forward in fields[] for a match */
				wild_card = 0;
				do {
					/* first make sure we haven't run off the end of fields[] */
					if ( (field_p >= ETYMON_MAX_FIELD_NEST) || (fields[field_p] == 0) ) {
						return 0;
					}
				} while (field_mask[mask_p] != fields[field_p++]);
			}
		}
	}

	/* if wild_card is off, then at this point we need to be at
           the end of fields[], otherwise the match is not perfect */
	if (wild_card == 0) {
		if ( (field_p < ETYMON_MAX_FIELD_NEST) && (fields[field_p] != 0) ) {
			return 0;
		}
	}

	/* if we made it to this point, then we have a match */
	return 1;
}


int etymon_af_score_vector(ETYMON_AF_SEARCH_STATE* state,
			    ETYMON_AF_IRESULT* iresults,
			    int iresults_n, Uint4 corpus_doc_n, int tf_qj) {
	int x;
	float* idf;
	float sc;
	float sumsq = 0;
	
	float w_qj, d_ij, tf_ij, idf_j, d, df_j;
	
	w_qj = 1 + log(tf_qj);
	d = corpus_doc_n;
	df_j = iresults_n;
	idf_j = log(d / df_j);
	for (x = 0; x < iresults_n; x++) {
		tf_ij = iresults[x].w_d;
		d_ij = (1 + log(tf_ij)) * idf_j;
		iresults[x].w_d = w_qj * d_ij;
		iresults[x].d2 = d_ij * d_ij;
/*		printf("w_d: %f\td2: %f\n", iresults[x].w_d, iresults[x].d2); */
	}
	
	return 0;
}


int etymon_af_score_boolean(ETYMON_AF_SEARCH_STATE* state,
			    ETYMON_AF_IRESULT* iresults,
			    int iresults_n, Uint4 corpus_doc_n, int tf_qj) {
	int x;
	float* idf;
	float sc;
	float sumsq = 0;

	idf = (float*)(malloc(iresults_n * sizeof(float)));
	if (idf == NULL)
		return aferr(AFEMEM);
		
	for (x = 0; x < iresults_n; x++) {
		sc = (corpus_doc_n / iresults_n)
			* iresults[x].w_d;
		idf[x] = sc;
		sumsq += (sc * sc);
	}

	sumsq = sqrt(sumsq);
	if (sumsq == 0.0) {
		sumsq = 1.0;
	}

	for (x = 0; x < iresults_n; x++) {
/*		printf(">> %f %f\n", idf[x], sumsq); */
		iresults[x].w_d = (int)((idf[x] / sumsq) * 10000);
	}
	
	free(idf);
	return 0;
}


int etymon_af_search_term(ETYMON_AF_SEARCH_STATE* state, unsigned char* term, int tf_qj, ETYMON_AF_IRESULT** iresults, int* iresults_n) {
	Uint2 field_mask[ETYMON_MAX_FIELD_NEST * 2];
	int field_mask_len;
	int sx, yy, x, x_r0, r0_count, r0_size, r0_n, wn_found, t;
	unsigned int ux, uz, x_r, x_wn;
	int done;
	unsigned char* phrase_start;
	unsigned char* phrase_p;
	unsigned char* p;
	int search_field_len;
	unsigned char word[ETYMON_MAX_WORD_SIZE];
	int word_len;
	int unstemmed_word_len;
	int phrase_operator;
	Uint4 udict_p;
	Uint1 leaf_flag;
	ETYMON_INDEX_PAGE_NL page_nl;
	ETYMON_INDEX_PAGE_L page_l;
	ETYMON_INDEX_LPOST lpost;
	ETYMON_INDEX_LPOST *posts = NULL;
	ETYMON_INDEX_UPOST upost;
	ETYMON_INDEX_LFIELD lfield;
	ETYMON_INDEX_UFIELD ufield;
	ETYMON_INDEX_LWORD lword;
	ETYMON_INDEX_UWORD uword;
	ETYMON_INDEX_LWORD* lword_list;
	int insertion;
	int insertion_first, insertion_last;
	Uint4 lf_first, lf_last, lf, lf_old;
	int match;
	int word_counter;
	ETYMON_AF_R0* r0 = NULL;
	int r0_wn_size = 0;
	int rooted = 0;
	int r_good;
	int field_match;
	Uint4 field_x;
	Uint4 wn_key;
	int right_truncation;
	int done_lf;
	int comp;
	int lf_counter;
	Uint4 post_n;
	int term_match_n; /* number of multiple words that match a single term */
	int tm_x;
	int intersect = 0;
	int exists;
	ETYMON_INDEX_LWORD* wlist_bsearch;
	int lword_list_n;
	Uint4 postp;
	Uint4 uwordp = 0;
	Uint4 ufieldp = 0;
	int uwordx;
	int upostx;
	int postsw;
	int postsn = 0;
	int postsx = 0;
	int first;
	int donepost;
	
	/* initialize results - I think this is where this should go */
	*iresults_n = 0;
	*iresults = NULL;

	/* parse term to isolate prepended field tag path (if any):
           basically search for the last '/' in the term that occurs
           before '\0' or '"' */
	search_field_len = 0; /* start with the assumption of no field path present */
	done = 0;
	sx = 0;
	while (done == 0) {
		switch (term[sx]) {
		case '/':
			search_field_len = sx + 1;
			break;
		case '"':
		case '\0':
			done = 1;
			break;
		default:
			break;
		}
		sx++;
	}

	/* identify start of the phrase part of the term */
	phrase_start = term + search_field_len;

	/* convert the field tag path to an array */
	field_mask_len = 0;
	if (search_field_len > 0) {
		/* check if field path is rooted or unrooted */
		if (*term == '/') {
			rooted = 1;
			sx = 1;
		} else {
			rooted = 0;
			sx = 0;
		}
		/* loop through field path and extract each field name
                   separated by '/' */
		while (sx < search_field_len) {
			/* copy field name to word[] */
			p = word;
			while ( (sx < search_field_len) && (term[sx] != '/') ) {
				*(p++) = term[sx++];
			}
			sx++; /* scoot past '/' */
			*p = '\0';
			if (strcmp((char*)word, "...") == 0) {
				/* in the search field mask we use 0
                                   to indicate a wild card, unlike in
                                   the field arrays, where it marks
                                   the end of the array */
				field_mask[field_mask_len++] = 0;
			} else {
				if ( (field_mask[field_mask_len++] = etymon_af_fdef_get_field(etymon_af_state[state->dbid]->fdef,
											   word)) == 0 )
/*					return aferr(AFEBADFIELD);*/
					return 0;
			}
		}
	}

	/* loop through each word in phrase part of term, intersecting
           result sets */
	phrase_p = phrase_start;
	phrase_operator = 0; /* 0=phrase */
	word_counter = 0;
	r0_n = 0;
	r0_size = 0;
	r0_count = 0;
	while (*phrase_p != '\0') {
		
		/* move past any whitespace */
		while ( (*phrase_p == ' ') ||
#ifdef UMLS
/*				(*phrase_p == ',') || (*phrase_p == '(') || (*phrase_p == ')') || */
#endif
				(*phrase_p == '"') ) {
			phrase_p++;
		}

		/* if next word is a phrase operator (e.g. proximity),
                   then record it and move to the next word - and if
                   the next word is also a phrase operator, then we
                   have a syntax error */
		/* to be implemented */
		/* for now, leave the phrase operator as 0 (phrase) */
		
		/* isolate word to search on, and convert to lowercase */
		word_len = 0;
		while ( (word_len < (ETYMON_MAX_WORD_SIZE - 1)) && (phrase_p[word_len] != ' ') && 
#ifdef UMLS
/*				(phrase_p[word_len] != ',') && (phrase_p[word_len] != '(') && (phrase_p[word_len] != ')') && */
#endif
				(phrase_p[word_len] != '"') && (phrase_p[word_len] != '\0') ) {
			word[word_len] = tolower(phrase_p[word_len]);
			word_len++;
		}
		word[word_len] = '\0';
		/*printf("term: [%s]\n", word);*/
		
		/* search for word */

		udict_p = etymon_af_state[state->dbid]->info.udict_root;

		if (*word == '\0') {
			break;
		}
		
		if (word[word_len - 1] == '*') {
			right_truncation = 1;
			word[--word_len] = '\0';
		} else {
			right_truncation = 0;
		}

		unstemmed_word_len = word_len;
		if (etymon_af_state[state->dbid]->info.stemming)
			word_len = af_stem(word);

		do {
			if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], (etymon_af_off_t)udict_p, SEEK_SET) == -1) {
				perror("etymon_af_search_term():lseek()");
			}
			if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &(leaf_flag), 1) == -1) {
				perror("etymon_af_search_term():read()");
			}
			if (leaf_flag == 0) {
				if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &page_nl,
					 sizeof(ETYMON_INDEX_PAGE_NL)) == -1) {
					perror("etymon_af_search_term():read()");
				}
				insertion = etymon_index_search_keys_nl(word, word_len, &page_nl);
				udict_p = page_nl.p[insertion];
			}
		} while (leaf_flag == 0);

		if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &page_l, sizeof(ETYMON_INDEX_PAGE_L)) == -1) {
			perror("etymon_af_search_term():read()");
		}
		insertion = etymon_index_search_keys_l(word, word_len, &page_l, &match);

		if (match) {
			post_n = page_l.post_n[insertion];
		} else {
			post_n = 0;
		}
		
		lf = udict_p;
		lf_first = udict_p;
		lf_last = udict_p;
		insertion_first = insertion;
		insertion_last = insertion;
		if (match) {
			term_match_n = 1;
		} else {
			term_match_n = 0;
		}

		if (right_truncation) {

			if (match == 0) {
				insertion_first = insertion + 1;
				insertion_last = insertion - 1;
			}
			
			/* right truncation is on, so we have to
                           search left and right for endpoints,
                           expanding the lf and insertion range */

			/* first search left */

			done_lf = 0;
			do {
				insertion = insertion_first - 1;
				if (insertion < 0) {
					lf = page_l.prev;
					if (lf) {
						if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], (etymon_af_off_t)lf,
							     SEEK_SET) == -1) {
							perror("etymon_af_search_term():lseek()");
						}
						if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &(leaf_flag), 1) == -1) {
							perror("etymon_af_search_term():read()");
						}
						if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &page_l,
							 sizeof(ETYMON_INDEX_PAGE_L)) == -1) {
							perror("etymon_af_search_term():read()");
						}
						insertion = page_l.n - 1;
					} else {
						done_lf = 1;
					}
				}
				if (!done_lf) {
					comp = strncmp( (char*)word, (char*)(page_l.keys + page_l.offset[insertion]),
							word_len);
					if (comp == 0) {
						term_match_n++;
						match = 1;
						post_n += page_l.post_n[insertion];
						lf_first = lf;
						insertion_first = insertion;
					} else {
						done_lf = 1;
					}
				}
			} while (!done_lf);

			lf = udict_p;
			/* force reload of the original page */
			if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], (etymon_af_off_t)lf,
				     SEEK_SET) == -1) {
				perror("etymon_af_search_term():lseek()");
			}
			if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &(leaf_flag), 1) == -1) {
				perror("etymon_af_search_term():read()");
			}
			if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &page_l,
				 sizeof(ETYMON_INDEX_PAGE_L)) == -1) {
				perror("etymon_af_search_term():read()");
			}
			
			/* now search right */

			done_lf = 0;
			do {
				insertion = insertion_last + 1;
				if (insertion >= page_l.n) {
					lf = page_l.next;
					if (lf) {
						if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], (etymon_af_off_t)lf,
							     SEEK_SET) == -1) {
							perror("etymon_af_search_term():lseek()");
						}
						if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &(leaf_flag), 1) == -1) {
							perror("etymon_af_search_term():read()");
						}
						if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &page_l,
							 sizeof(ETYMON_INDEX_PAGE_L)) == -1) {
							perror("etymon_af_search_term():read()");
						}
						insertion = 0;
					} else {
						done_lf = 1;
					}
				}
				if (!done_lf) {
					comp = strncmp( (char*)word, (char*)(page_l.keys + page_l.offset[insertion]),
							word_len);
					if (comp == 0) {
						term_match_n++;
						match = 1;
						post_n += page_l.post_n[insertion];
						lf_last = lf;
						insertion_last = insertion;
					} else {
						done_lf = 1;
					}
				}
			} while (!done_lf);
			
		} /* if: right_truncation */

		/* if no match, then any intersection will result in
		   the empty set; therefore we are done */
		if (match == 0) {
			etymon_af_search_free_r0(r0, r0_size, r0_wn_size);
			return 0;
		}

		lf_old = lf;
		lf = lf_first;
		insertion = insertion_first;
		done_lf = 0;
		lf_counter = 0;
		x_r0 = 0;
		if (word_counter == 1) {
			qsort(r0, r0_n, sizeof(ETYMON_AF_R0), etymon_af_search_r0_compare_reverse);
		}

		if (word_counter >= 2) {
			for (x = 0; x < r0_n; x++) {
				for (yy = 0; yy < r0_wn_size; yy++) {
					t = 0;
					for (uz = 0; uz < r0[x].wn[yy].wn_n; uz++) {
						if (r0[x].wn[yy].new_word[uz] != 0) {
							r0[x].wn[yy].wlist[t] = r0[x].wn[yy].wlist[uz];
							r0[x].wn[yy].new_word[t] = 0;
							t++;
						}
					}
					r0[x].wn[yy].wn_n = t;
				}
			}
		}
		
		do {

			if (lf != lf_old) {
				lf_old = lf;
				if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], (etymon_af_off_t)lf,
					     SEEK_SET) == -1) {
					perror("etymon_af_search_term():lseek()");
				}
				if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &(leaf_flag), 1) == -1) {
					perror("etymon_af_search_term():read()");
				}
				if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &page_l,
					 sizeof(ETYMON_INDEX_PAGE_L)) == -1) {
					perror("etymon_af_search_term():read()");
				}
			}

			/* if this is the first result set, build a structure to hold the results */
			if ( (word_counter == 0) && (lf_counter == 0) ) {
				/* first we need an array to store the doc_id's */
				r0 = (ETYMON_AF_R0*)(calloc(sizeof(ETYMON_AF_R0) * post_n, 1));
				if (r0 == NULL)
					return aferr(AFEMEM);
				r0_size = post_n;
				/* also store word number data if phrase search is enabled */
				if ( (etymon_af_state[state->dbid]->info.phrase) ||
				     (etymon_af_state[state->dbid]->info.word_proximity) ) {
					for (ux = 0; ux < post_n; ux++) {
						r0[ux].wn = (ETYMON_AF_R0_WN*)(calloc(sizeof(ETYMON_AF_R0_WN) * term_match_n, 1));
						if (r0[ux].wn == NULL)
							return aferr(AFEMEM);
					}
					r0_wn_size = term_match_n;
				}
			}

			/* loop through each result, validating fields and
			   intersecting with running set */
			/* if word_counter == 0, then x_r0 is used to put the
			   results into r0_*[], otherwise it's used for
			   keeping track of the current place for doing
			   intersections with r0_*[]. */
			if (word_counter > 0) {
				x_r0 = 0;
			}

			postp = page_l.post[insertion];
/*			printf("page_l.post_n[insertion] %lu\n", (unsigned long) page_l.post_n[insertion]);*/
			for (x_r = 0; x_r < page_l.post_n[insertion]; x_r++) {
/*				printf("x_r %i\n", x_r);*/
				
				/* read a result */
/*				printf("Read a result\n");*/
				if (etymon_af_state[state->dbid]->info.optimized) {
					if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_LPOST],
						     (etymon_af_off_t)( ((etymon_af_off_t)(postp - 1)) *
								 ((etymon_af_off_t)(sizeof(ETYMON_INDEX_LPOST))) ),
						     SEEK_SET) == -1) {
						perror("etymon_af_search_term():lseek()");
					}
					if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_LPOST], &lpost,
						 sizeof(ETYMON_INDEX_LPOST)) == -1) {
						perror("etymon_af_search_term():read()");
					}
					postp++;
				} else {
					postsw = 16;
					postsn = 0;
					posts = (ETYMON_INDEX_LPOST *) malloc(postsw *
									      sizeof (ETYMON_INDEX_LPOST));
					first = 1;
					donepost = 0;
					lpost.doc_id = 0;
					lpost.freq = 0;
					lpost.fields = 0;
					lpost.fields_n = 0;
					lpost.word_numbers = 0;
					lpost.word_numbers_n = 0;
					do {
						if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UPOST],
								    (etymon_af_off_t)( ((etymon_af_off_t)(postp - 1)) *
										       ((etymon_af_off_t)(sizeof(ETYMON_INDEX_UPOST))) ),
								    SEEK_SET) == -1) {
							perror("etymon_af_search_term():lseek()");
						}
						if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UPOST], &upost,
							 sizeof(ETYMON_INDEX_UPOST)) == -1) {
							perror("etymon_af_search_term():read()");
						}
/*						printf("doc_id %lu\n", (unsigned long) upost.doc_id);*/
						if (first) {
							first = 0;
							lpost.doc_id = upost.doc_id;
							postp = upost.next;
							posts[postsn].doc_id = upost.doc_id;
							posts[postsn].freq = upost.freq;
							posts[postsn].fields = upost.fields;
							posts[postsn].fields_n = upost.fields_n;
							posts[postsn].word_numbers = upost.word_numbers;
							posts[postsn].word_numbers_n = upost.word_numbers_n;
							lpost.freq += upost.freq;
							lpost.fields_n += upost.fields_n;
							lpost.word_numbers_n += upost.word_numbers_n;
							postsn++;
						} else {
							if (upost.doc_id == lpost.doc_id) {
/*								printf("Matching docid; upost.next=%lu\n",
								(unsigned long) upost.next);*/
								postp = upost.next;
								if (postsn >= postsw) {
									postsw = postsw * 2;
									posts = (ETYMON_INDEX_LPOST *)
										realloc(posts, postsw *
											sizeof (ETYMON_INDEX_LPOST));
								}
								posts[postsn].doc_id = upost.doc_id;
								posts[postsn].freq = upost.freq;
								posts[postsn].fields = upost.fields;
								posts[postsn].fields_n = upost.fields_n;
								posts[postsn].word_numbers = upost.word_numbers;
								posts[postsn].word_numbers_n = upost.word_numbers_n;
								lpost.freq += upost.freq;
								lpost.fields_n += upost.fields_n;
								lpost.word_numbers_n += upost.word_numbers_n;
								postsn++;
								x_r++;
							} else {
								donepost = 1;
							}
						}
					} while (postp && !donepost);
				}
/*				printf("Done\n");*/
				
				/* if this is the first result set, store the results in r0_*[] */
				if (word_counter == 0) {
					exists = 0;
					yy = 0;
					/* first check if it is already in r0[] */
					if (lf_counter > 0) {
						for (yy = 0; yy < r0_n; yy++) {
							if (r0[yy].doc_id == lpost.doc_id) {
								exists = 1;
								break;
							}
						}
					}
					
					if (!exists) {
						r0[x_r0].doc_id = lpost.doc_id;
						r0[x_r0].freq = lpost.freq;
						r_good = x_r0;
						yy = x_r0;
						x_r0++;
						r0_count++;
						r0_n++;
					} else {
						r_good = yy;
						r0[x_r0].freq += lpost.freq;
					}

					if ( (etymon_af_state[state->dbid]->info.phrase) ||
					     (etymon_af_state[state->dbid]->info.word_proximity) ) {

						/* load word numbers */
						r0[yy].wn[lf_counter].wn_n = lpost.word_numbers_n;
						r0[yy].wn[lf_counter].wlist =
							(ETYMON_INDEX_LWORD*)(malloc(r0[yy].wn[lf_counter].wn_n *
										     sizeof(ETYMON_INDEX_LWORD)));
						r0[yy].wn[lf_counter].new_word =
							(Uint1*)(calloc(r0[yy].wn[lf_counter].wn_n *
									sizeof(Uint1), 1));
						if ( (r0[yy].wn[lf_counter].wlist == NULL) ||
						     (r0[yy].wn[lf_counter].new_word == NULL) ) {
							etymon_af_search_free_r0(r0, r0_size, r0_wn_size);
							return aferr(AFEMEM);
						}
						if (etymon_af_state[state->dbid]->info.optimized) {
							if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_LWORD],
									    (etymon_af_off_t)( ((etymon_af_off_t)(lpost.word_numbers - 1)) *
											       ((etymon_af_off_t)(sizeof(ETYMON_INDEX_LWORD))) ),
									    SEEK_SET) == -1) {
								perror("etymon_af_search_term():lseek()");
							}
							if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_LWORD],
								 r0[yy].wn[lf_counter].wlist,
								 r0[yy].wn[lf_counter].wn_n * sizeof(ETYMON_INDEX_LWORD)) == -1) {
								perror("etymon_af_search_term():read()");
							}
						} else {
							postsx = 0;
							uwordp = posts[postsx].word_numbers;
							uwordx = 0;
							do {
								if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UWORD],
										    (etymon_af_off_t)( ((etymon_af_off_t)(uwordp - 1)) *
												       ((etymon_af_off_t)(sizeof(ETYMON_INDEX_UWORD))) ),
										    SEEK_SET) == -1) {
									perror("etymon_af_search_term():lseek()");
								}
								if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UWORD],
									 &uword,
									 sizeof(ETYMON_INDEX_UWORD)) == -1) {
									perror("etymon_af_search_term():read()");
								}
/*								printf("%lu\n", (unsigned long) uword.wn);*/
								r0[yy].wn[lf_counter].wlist[uwordx].wn = uword.wn;
								uwordp = uword.next;
								uwordx++;
								while (!uwordp && postsx < (postsn - 1))
									uwordp = posts[++postsx].word_numbers;
							} while (uwordp);
						}
						/* reverse list */
						for (ux = 0; ux < (r0[yy].wn[lf_counter].wn_n / 2); ux++) {
							lword = r0[yy].wn[lf_counter].wlist[ux];
							r0[yy].wn[lf_counter].wlist[ux] =
								r0[yy].wn[lf_counter].wlist[r0[yy].wn[lf_counter].wn_n -
											    ux - 1];
							r0[yy].wn[lf_counter].wlist[r0[yy].wn[lf_counter].wn_n - ux - 1] =
								lword;
						}
					}
						
				} else { /* otherwise intersect with r0_*[] */
					intersect = 0;
				
					/* skip over results in r0_*[] */

					while ( (x_r0 < r0_n) &&
						( (r0[x_r0].doc_id == 0) || (r0[x_r0].doc_id > lpost.doc_id) ) ) {
						x_r0++;
					}
					/* see if r0_*[] contains a match */
					if ( (x_r0 < r0_n) &&
					     (r0[x_r0].doc_id == lpost.doc_id) ) {
						r_good = x_r0;
						intersect = 1;
						x_r0++;
					} else {
						r_good = -1;
					}
				}
				
				/* validate against field array */
				if ( (r_good != -1) && (field_mask_len > 0) ) {
					field_match = 0;
					/* check if field matches */
					if (lpost.fields_n > 0) {
						if (etymon_af_state[state->dbid]->info.optimized) {
							if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_LFIELD],
									    (etymon_af_off_t)( ((etymon_af_off_t)(lpost.fields - 1)) *
											       ((etymon_af_off_t)(sizeof(ETYMON_INDEX_LFIELD))) ),
									    SEEK_SET) == -1) {
								perror("etymon_af_search_term():lseek()");
							}
						} else {
							postsx = 0;
							ufieldp = posts[postsx].fields;
						}
						field_x = 0;
						do {
							if (etymon_af_state[state->dbid]->info.optimized) {
								if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_LFIELD],
									 &lfield, sizeof(ETYMON_INDEX_LFIELD)) == -1) {
									perror("etymon_af_search_term():read()");
								}
							} else {
								if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UFIELD],
										    (etymon_af_off_t)( ((etymon_af_off_t)(ufieldp - 1)) *
												       ((etymon_af_off_t)(sizeof(ETYMON_INDEX_UFIELD))) ),
										    SEEK_SET) == -1) {
									perror("etymon_af_search_term():lseek()");
								}
								if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UFIELD],
									 &ufield, sizeof(ETYMON_INDEX_UFIELD)) == -1) {
									perror("etymon_af_search_term():read()");
								}
								ufieldp = ufield.next;
								while (!ufieldp && postsx < (postsn - 1))
									ufieldp = posts[++postsx].fields;
								memcpy(lfield.fields, ufield.fields,
								       sizeof (Uint2) * ETYMON_MAX_FIELD_NEST);
							}
							/* new field search supporting general containment */
							field_match = etymon_af_search_fields(field_mask, field_mask_len, rooted,
										       lfield.fields);
							field_x++;
						} while ( (field_match == 0) && (field_x < lpost.fields_n) );
					}
					if (field_match == 0) {
						if (word_counter == 0) {
							r0[r_good].doc_id = 0;
							r0_count--;
						}
						r_good = -1;
						intersect = 0;
					}
				}

				/* intersect by phrase operator */
				if ( ( (etymon_af_state[state->dbid]->info.phrase) ||
				       (etymon_af_state[state->dbid]->info.word_proximity) ) &&
				     (r_good != -1) && (word_counter > 0) ) {
					
					wn_found = 0;
					
					for (tm_x = 0; tm_x < r0_wn_size; tm_x++) {
						
						if (r0[r_good].wn[tm_x].wlist) {
							
							/* load original (r0) word numbers */
							lword_list = r0[r_good].wn[tm_x].wlist;

							/* load and compare (intersect by phrase operator) new
							   word numbers */

							/* if optimized, we only have to seek once */
							if (etymon_af_state[state->dbid]->info.optimized) {
								if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_LWORD],
									     (etymon_af_off_t)( ((etymon_af_off_t)(lpost.word_numbers - 1)) *
											 ((etymon_af_off_t)(sizeof(ETYMON_INDEX_LWORD))) ),
									     SEEK_SET) == -1) {
									perror("etymon_af_search_term():lseek()");
								}
							} else {
								postsx = 0;
								uwordp = posts[postsx].word_numbers;
							}
							/* loop through new word numbers and intersect with lword_list[] */
							for (x_wn = 0; x_wn < lpost.word_numbers_n; x_wn++) {
								/* read a word number */
								if (etymon_af_state[state->dbid]->info.optimized) {
									if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_LWORD], &lword,
										 sizeof(ETYMON_INDEX_LWORD)) == -1) {
										perror("etymon_af_search_term():read()");
									}
								} else {
									if (etymon_af_lseek(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UWORD],
											    (etymon_af_off_t)( ((etymon_af_off_t)(uwordp - 1)) *
													       ((etymon_af_off_t)(sizeof(ETYMON_INDEX_UWORD))) ),
											    SEEK_SET) == -1) {
										perror("etymon_af_search_term():lseek()");
									}
									if (read(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UWORD], &uword,
										 sizeof(ETYMON_INDEX_UWORD)) == -1) {
										perror("etymon_af_search_term():read()");
									}
									lword.wn = uword.wn;
									uwordp = uword.next;
									while (!uwordp && postsx < (postsn - 1))
										uwordp = posts[++postsx].word_numbers;
								}
								/* determine search key based on phrase operator */
								if (phrase_operator == 0) {
									wn_key = lword.wn - word_counter;
								}
								/* search lword_list[] for the wn key */
								lword_list_n = r0[r_good].wn[tm_x].wn_n;
								wlist_bsearch = (ETYMON_INDEX_LWORD*)(
									bsearch(&(wn_key),
										lword_list,
										lword_list_n,
										sizeof(ETYMON_INDEX_LWORD),
										etymon_af_search_term_lword_compare));
								if (wlist_bsearch) {
									wn_found++;
									r0[r_good].wn[tm_x].new_word[
										wlist_bsearch - lword_list] = 1;
								}
							}
							
						}
						
					} /* for: tm_x */
					
					if (r0[r_good].new_doc == 0) {
						if (wn_found == 0) {
							r_good = -1;
							intersect = 0;
						}
						
						if (intersect) {
							r0[r_good].new_doc = 1;
							r0[r_good].freq = wn_found;
						}
					}
					
				}

				if (!etymon_af_state[state->dbid]->info.optimized)
					free(posts);
				
			} /* for: x_r */
			
			if ( (lf == lf_last) && (insertion == insertion_last) ) {
				done_lf = 1;
			} else {
				insertion++;
				if (insertion >= page_l.n) {
					lf = page_l.next;
					insertion = 0;
				}
			}

			lf_counter++;

		} while (!done_lf);

		if (word_counter > 0) {

			for (x = 0; x < r0_n; x++) {
				if (r0[x].new_doc) {

					r0[x].new_doc = 0;
				} else {
					if (r0[x].doc_id) {
						r0[x].doc_id = 0;
						r0_count--;
					}
				}
			}
		}
			
		word_counter++;
		phrase_p += unstemmed_word_len;
		if (right_truncation) {
			phrase_p++;
		}
	}

	/* convert results array into an ETYMON_AF_RESULT array */
	if (r0_count > 0) {
		*iresults = (ETYMON_AF_IRESULT*)(malloc(r0_count * sizeof(ETYMON_AF_IRESULT)));
	} else {
		*iresults = (ETYMON_AF_IRESULT*)(malloc(sizeof(ETYMON_AF_IRESULT)));
	}
	if (*iresults == NULL) {
		etymon_af_search_free_r0(r0, r0_size, r0_wn_size);
		return aferr(AFEMEM);
	}
	x_r = 0;
	for (x = 0; x < r0_n; x++) {
		if (r0[x].doc_id != 0) {
			(*iresults)[x_r].doc_id = r0[x].doc_id;
			(*iresults)[x_r].w_d =
				state->opt->score ?
				r0[x].freq : 0;
			x_r++;
		}
	}
	*iresults_n = r0_count;
	
	etymon_af_search_free_r0(r0, r0_size, r0_wn_size);
	
	/* add relevance scores */
	if (state->opt->score == AFSCOREBOOLEAN) {
		etymon_af_score_boolean(state, *iresults, *iresults_n,
					state->corpus_doc_n, tf_qj);
	}
	if (state->opt->score == AFSCOREVECTOR) {
		etymon_af_score_vector(state, *iresults, *iresults_n,
					state->corpus_doc_n, tf_qj);
	}
	
	if (*iresults_n > 1) {
		qsort((*iresults), (*iresults_n), sizeof(ETYMON_AF_IRESULT), etymon_af_search_iresult_compare_reverse);
	}
	
	return 0;
}


int etymon_af_boolean_or(ETYMON_AF_SEARCH_STATE* state, ETYMON_AF_IRESULT** r_stack, int* rn_stack, int r1, int r2) {
	int p1, p2;
	int new_size;
	ETYMON_AF_IRESULT* rset1;
	ETYMON_AF_IRESULT* rset2;

	/* expand available space in result set */
	new_size = rn_stack[r1] + rn_stack[r2];
	if (new_size == 0) {
		new_size = 1;
	}
	r_stack[r1] = (ETYMON_AF_IRESULT*)(realloc(r_stack[r1], new_size * sizeof(ETYMON_AF_IRESULT)));
	if (r_stack[r1] == NULL) {
		/* ERROR */
	}
	
	p1 = 0;
	rset1 = r_stack[r1];
	rset2 = r_stack[r2];

	for (p2 = 0; p2 < rn_stack[r2]; p2++) {
		while ( (p1 < rn_stack[r1]) && (rset1[p1].doc_id > rset2[p2].doc_id) ) {
			p1++;
		}
		if ( (p1 >= rn_stack[r1]) || (rset1[p1].doc_id != rset2[p2].doc_id) ) {
			rset1[rn_stack[r1]++] = rset2[p2];
		} else {
/*			printf("OR-ing: %f/%f OR %f/%f\n", rset1[p1].w_d, rset1[p1].d2, rset2[p2].w_d, rset2[p2].d2); */
			rset1[p1].w_d += rset2[p2].w_d;
			rset1[p1].d2 += rset2[p2].d2;
			p1++;
		}
	}

	/* shrink result set buffer to fit */
	new_size = rn_stack[r1];
	if (new_size == 0) {
		new_size = 1;
	}
	r_stack[r1] = (ETYMON_AF_IRESULT*)(realloc(r_stack[r1], new_size * sizeof(ETYMON_AF_IRESULT)));
	if (r_stack[r1] == NULL) {
		/* ERROR */
	}

	/* sort by doc_id */
	if (rn_stack[r1] > 1) {
		qsort(r_stack[r1], rn_stack[r1], sizeof(ETYMON_AF_IRESULT), etymon_af_search_iresult_compare_reverse);
	}
	
	return 0;
}


int etymon_af_boolean_and(ETYMON_AF_SEARCH_STATE* state, ETYMON_AF_IRESULT** r_stack, int* rn_stack, int r1, int r2) {
	int p1, p2;
	int new_size;
	ETYMON_AF_IRESULT* rset1;
	ETYMON_AF_IRESULT* rset2;
	int insert;

	p2 = 0;
	insert = 0;
	rset1 = r_stack[r1];
	rset2 = r_stack[r2];

	for (p1 = 0; p1 < rn_stack[r1]; p1++) {
		while ( (p2 < rn_stack[r2]) && (rset1[p1].doc_id < rset2[p2].doc_id) ) {
			p2++;
		}
		if ( (p2 < rn_stack[r2]) && (rset1[p1].doc_id == rset2[p2].doc_id) ) {
			if (insert != p1) {
				rset1[insert] = rset2[p2];
				rset1[insert].w_d = rset1[p1].w_d
					+ rset2[p2].w_d;
				rset1[insert].d2 = rset1[p1].d2
					+ rset2[p2].d2;
			} else {
				rset1[insert].w_d += rset2[p2].w_d;
				rset1[insert].d2 += rset2[p2].d2;
			}
			insert++;
			p2++;
		}
	}

	rn_stack[r1] = insert;
	
	/* shrink result set buffer to fit */
	new_size = rn_stack[r1];
	if (new_size == 0) {
		new_size = 1;
	}
	r_stack[r1] = (ETYMON_AF_IRESULT*)(realloc(r_stack[r1], new_size * sizeof(ETYMON_AF_IRESULT)));
	if (r_stack[r1] == NULL) {
		/* ERROR */
	}

	/* product of "and" shouldn't need to be sorted */
	
	return 0;
}


char *strcpy_hash(char *dst, const char *src)
{
	int sum;
	const char *p;
	char *q;
	char ch;
	
	sum = 0;
	p = src;
	q = dst + 1;
	do {
		*(q++) = ch = *(p++);
		sum += (int) ch;
	} while (ch != '\0');
	*dst = (char) (sum % 256);
	return dst;
}


typedef struct {
	unsigned char hterm[ETYMON_MAX_QUERY_TERM_SIZE + 1];  /* hash and query term */
	int tf_qj;  /* number of occurrences of term in query */
} QUERY_TABLE;


/* possible errors:
   EX_IO
*/
int search_db_new(ETYMON_AF_SEARCH_STATE* state) {
	ETYMON_AF_STAT st;
	int term_start, term_len, term_done, quote_on;
	int query_len;
	unsigned char* query;
	char ch;
	unsigned char term[ETYMON_MAX_QUERY_TERM_SIZE];
	int op_stack[ETYMON_AF_MAX_OP_STACK_DEPTH];
	int op_stack_p;
	ETYMON_AF_IRESULT* r_stack[ETYMON_AF_MAX_R_STACK_DEPTH];
	int rn_stack[ETYMON_AF_MAX_R_STACK_DEPTH];
	int r_stack_p;
	int op_type;
	int try_op;

	int x;
	QUERY_TABLE *query_table;
	int query_table_size, query_table_n;
	unsigned char hterm[ETYMON_MAX_QUERY_TERM_SIZE + 1];  /* hash and query term */
	int hterm_found;
	
	/* open database files */
	if (etymon_af_state[state->dbid]->keep_open == 0) {
		if (etymon_af_open_files(state->dbid, O_RDONLY) == -1) {
			return -1;
		}
	}

	/* only perform the search if there is something in the index */
	if (etymon_af_fstat(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &st) == -1) {
		perror("etymon_af_search_db():fstat()");
	}
	if (st.st_size > ((etymon_af_off_t)0)) {

		query = state->opt->query;
		query_len = strlen((char*)query);
		term_start = 0;
		op_stack_p = 0;
		r_stack_p = 0;

		/* set up query term table, to track tf_qj */
		query_table_size = query_len / 2 + 1;  /* conservative upper bound on number of terms */
		query_table = (QUERY_TABLE *) malloc( (sizeof (QUERY_TABLE)) * query_table_size );
		query_table_n = 0;
				
		while (term_start < query_len) {

			/* skip past spaces */
			while ( (term_start < query_len) && (query[term_start] == ' ') ) {
				term_start++;
			}

			/* parse out term */
			term_len = 0;
			quote_on = 0;
			term_done = 0;
			while ( ((term_start + term_len) < query_len) && (!term_done) ) {
				ch = query[term_start + term_len];
				if (quote_on) {
					switch (ch) {
					case '\"':
						quote_on = 0;
						break;
					default:
						break;
					}
				} else {
					switch (ch) {
					case '\"':
						quote_on = 1;
						break;
					case ' ':
						term_done = 1;
						term_len--;
						break;
					case '(':
					case ')':
						term_done = 1;
						if (term_len > 0) {
							term_len--;
						}
						break;
					default:
						break;
					}
				}
				term_len++;
			}

			/* make sure the query term is not too long */
			if (term_len >= ETYMON_MAX_QUERY_TERM_SIZE) {
				/* make a temporary buffer to hold the term */
				char* term_tmp = (char*)(malloc(term_len + 1));
				if (term_tmp) {
					memcpy(term_tmp, query + term_start, term_len);
					term_tmp[term_len] = '\0';
				}
				if (term_tmp) {
					free(term_tmp);
				}
				/* close database files */
				if (etymon_af_state[state->dbid]->keep_open == 0) {
					etymon_af_close_files(state->dbid);
				}
				/* free result sets */
				while (--r_stack_p >= 0) {
					free(r_stack[r_stack_p]);
				}
				return aferr(AFETERMLEN);
			}

			memcpy(term, query + term_start, term_len);
			term[term_len] = '\0';
			
			/* process token */

			if (strcmp((char*)term, "|") == 0) {
				op_type = ETYMON_AF_OP_OR;
			}
			else if (strcmp((char*)term, "&") == 0) {
				op_type = ETYMON_AF_OP_AND;
			}
			else if (strcmp((char*)term, "(") == 0) {
				op_type = ETYMON_AF_OP_GROUP_OPEN;
			}
			else if (strcmp((char*)term, ")") == 0) {
				op_type = ETYMON_AF_OP_GROUP_CLOSE;
			}
			else {
				op_type = 0;
			}

			if (op_type == 0) {
/*				printf("found term: [%s]\n", term); */
				strcpy_hash((char *) hterm, (char *) term);
				hterm_found = 0;
				/* search for term in query table */
				for (x = 0; x < query_table_n; x++) {
					if (strcmp((char *) hterm, (char *) query_table[x].hterm) == 0) {
						query_table[x].tf_qj++;
						hterm_found = 1;
						break;
					}
				}
				/* if not found, add it */
				if (!hterm_found) {
					x = query_table_n;
					if (++query_table_n > query_table_size) {
						printf("FATAL ERROR: query table overflow\n");
						exit(-1);
					}
					strcpy((char *) query_table[x].hterm, (char *) hterm);
					query_table[x].tf_qj = 1;
				}
				/* dump query table */
				/*				
 				printf("***** query table ***** (size = %d)\n", query_table_size);
				for (x = 0; x < query_table_n; x++)
					printf("%d [%s]\n", query_table[x].tf_qj, (char *) query_table[x].hterm + 1);
				*/
			}
			
#ifdef DISABLE
			try_op = 0;

			switch (op_type) {

			case 0: /* search term */

				/* make sure there is space left on the r_stack */
				if (r_stack_p >= ETYMON_AF_MAX_R_STACK_DEPTH) {
					/* close database files */
					if (etymon_af_state[state->dbid]->keep_open == 0) {
						etymon_af_close_files(state->dbid);
					}
					/* free result sets */
					while (--r_stack_p >= 0) {
						free(r_stack[r_stack_p]);
					}
					return aferr(AFEQUERYNEST);
				}
				
				/* perform the search */
				if (etymon_af_search_term(state, term, &(r_stack[r_stack_p]), &(rn_stack[r_stack_p])) == -1) {
					/* close database files */
					if (etymon_af_state[state->dbid]->keep_open == 0) {
						etymon_af_close_files(state->dbid);
					}
					/* free result sets */
					while (--r_stack_p >= 0) {
						free(r_stack[r_stack_p]);
					}
					return -1;
				}

				r_stack_p++;
				try_op = 1;
					
				break;

			case ETYMON_AF_OP_OR:
			case ETYMON_AF_OP_AND:
			case ETYMON_AF_OP_GROUP_OPEN:
				
				/* make sure there is space left on the op_stack */
				if (op_stack_p >= ETYMON_AF_MAX_OP_STACK_DEPTH) {
					/* close database files */
					if (etymon_af_state[state->dbid]->keep_open == 0) {
						etymon_af_close_files(state->dbid);
					}
					/* free result sets */
					while (--r_stack_p >= 0) {
						free(r_stack[r_stack_p]);
					}
					return aferr(AFEQUERYNEST);
				}

				/* push the operator onto the op_stack */
				op_stack[op_stack_p] = op_type;
				op_stack_p++;

				break;

			case ETYMON_AF_OP_GROUP_CLOSE:
				/* top element on op_stack should be ETYMON_AF_OP_GROUP_OPEN */
				if ( (op_stack_p <= 0) || (op_stack[op_stack_p - 1] != ETYMON_AF_OP_GROUP_OPEN) ) {
					/* close database files */
					if (etymon_af_state[state->dbid]->keep_open == 0) {
						etymon_af_close_files(state->dbid);
					}
					/* free result sets */
					while (--r_stack_p >= 0) {
						free(r_stack[r_stack_p]);
					}
					return aferr(AFEQUERYSYN);
				}
				
				/* pop ETYMON_AF_OP_GROUP_OPEN from op_stack */
				op_stack_p--;
				
				try_op = 1;
				
				break;

			default:
				break;

			}

			/* if try_op is set, we need to look for an
                           operator on the top of op_stack, and if we
                           find one, and there are enough operands for
                           it on r_stack, then execute the operation */
			if ( (try_op) && (op_stack_p > 0) ) {
				switch (op_stack[op_stack_p - 1]) {
				case ETYMON_AF_OP_OR:
					if (r_stack_p > 1) {
						if (etymon_af_boolean_or(state, r_stack, rn_stack, r_stack_p - 2, r_stack_p - 1)
						    == -1) {
							/* close database files */
							if (etymon_af_state[state->dbid]->keep_open == 0) {
								etymon_af_close_files(state->dbid);
							}
							/* free result sets */
							while (--r_stack_p >= 0) {
								free(r_stack[r_stack_p]);
							}
							return -1;
						}
						r_stack_p--;
						op_stack_p--;
					}
					break;
				case ETYMON_AF_OP_AND:
					if (r_stack_p > 1) {
						if (etymon_af_boolean_and(state, r_stack, rn_stack, r_stack_p - 2, r_stack_p - 1)
						    == -1) {
							/* close database files */
							if (etymon_af_state[state->dbid]->keep_open == 0) {
								etymon_af_close_files(state->dbid);
							}
							/* free result sets */
							while (--r_stack_p >= 0) {
								free(r_stack[r_stack_p]);
							}
							return -1;
						}
						r_stack_p--;
						op_stack_p--;
					}
					break;
				default:
					break;
				}
			}
#endif
			
			term_start += term_len;
			
		} /* while: term_start < query_len */

		/* loop over query table and perform term searches */
		for (x = 0; x < query_table_n; x++) {
	
			/* perform the search */
			if (etymon_af_search_term(state, query_table[x].hterm + 1, query_table[x].tf_qj, &(r_stack[r_stack_p]),
			        &(rn_stack[r_stack_p])) == -1) {

				/* close database files */
				if (etymon_af_state[state->dbid]->keep_open == 0) {
					etymon_af_close_files(state->dbid);
				}
				/* free result sets */
				while (--r_stack_p >= 0) {
					free(r_stack[r_stack_p]);
				}
				return -1;
			}
			r_stack_p++;

			if (r_stack_p > 1) {
				if (etymon_af_boolean_or(state, r_stack, rn_stack, r_stack_p - 2, r_stack_p - 1)
				    == -1) {
					/* close database files */
					if (etymon_af_state[state->dbid]->keep_open == 0) {
						etymon_af_close_files(state->dbid);
					}
					/* free result sets */
					while (--r_stack_p >= 0) {
						free(r_stack[r_stack_p]);
					}
					return -1;
				}
				r_stack_p--;
				op_stack_p--;
			}
		
		}  /* query_table loop */
		
		/* there should be a single result set remaining on the r_stack */
		if (r_stack_p != 1) {
			/* close database files */
			if (etymon_af_state[state->dbid]->keep_open == 0) {
				etymon_af_close_files(state->dbid);
			}
			/* free result sets */
			while (--r_stack_p >= 0) {
				free(r_stack[r_stack_p]);
			}
			return aferr(AFEQUERYSYN);
		}

		/* add results to the main result set */
		if (rn_stack[0] > 0) {
			int x_r, x;
			x_r = state->optr->resultn;
			if (state->optr->result == NULL) {
				state->optr->result = (Afresult *) malloc((x_r + rn_stack[0]) * sizeof(Afresult));
			} else {
				state->optr->result = (Afresult *) realloc(state->optr->result, (x_r + rn_stack[0]) * sizeof(Afresult));
			}
			if (state->optr->result == NULL) {
				/* ERROR */
				return -1;
			}
			for (x = 0; x < rn_stack[0]; x++) {
				state->optr->result[x_r].dbid = state->dbid;
				state->optr->result[x_r].docid = (r_stack[0])[x].doc_id;
/*				printf("w_d / sqrt(d2): %f / %f\n", (r_stack[0])[x].w_d, sqrt( (r_stack[0])[x].d2 ) );
				printf("Raw score: %f\n", (r_stack[0])[x].w_d / sqrt( (r_stack[0])[x].d2 ) ); */
				state->optr->result[x_r].score = (int) ( ((r_stack[0])[x].w_d / sqrt((r_stack[0])[x].d2)) *  100000000 );
				x_r++;
			}
			state->optr->resultn += rn_stack[0];
		}

		if (r_stack[0]) {
			free(r_stack[0]);
		}
		
	}
	
	/* close database files */
	if (etymon_af_state[state->dbid]->keep_open == 0) {
		if (etymon_af_close_files(state->dbid) == -1) {
			return -1;
		}
	}
	
	return 0;
}


/* possible errors:
   EX_IO
*/
int etymon_af_search_db(ETYMON_AF_SEARCH_STATE* state) {
	ETYMON_AF_STAT st;
	int term_start, term_len, term_done, quote_on;
	int query_len;
	unsigned char* query;
	char ch;
	unsigned char term[ETYMON_MAX_QUERY_TERM_SIZE];
	int op_stack[ETYMON_AF_MAX_OP_STACK_DEPTH];
	int op_stack_p;
	ETYMON_AF_IRESULT* r_stack[ETYMON_AF_MAX_R_STACK_DEPTH];
	int rn_stack[ETYMON_AF_MAX_R_STACK_DEPTH];
	int r_stack_p;
	int op_type;
	int try_op;

	/* open database files */
	if (etymon_af_state[state->dbid]->keep_open == 0) {
		if (etymon_af_open_files(state->dbid, O_RDONLY) == -1) {
			return -1;
		}
	}

	/* only perform the search if there is something in the index */
	if (etymon_af_fstat(etymon_af_state[state->dbid]->fd[ETYMON_DBF_UDICT], &st) == -1) {
		perror("etymon_af_search_db():fstat()");
	}
	if (st.st_size > ((etymon_af_off_t)0)) {

		query = state->opt->query;
		query_len = strlen((char*)query);
		term_start = 0;
		op_stack_p = 0;
		r_stack_p = 0;

		while (term_start < query_len) {

			/* skip past spaces */
			while ( (term_start < query_len) && (query[term_start] == ' ') ) {
				term_start++;
			}

			/* parse out term */
			term_len = 0;
			quote_on = 0;
			term_done = 0;
			while ( ((term_start + term_len) < query_len) && (!term_done) ) {
				ch = query[term_start + term_len];
				if (quote_on) {
					switch (ch) {
					case '\"':
						quote_on = 0;
						break;
					default:
						break;
					}
				} else {
					switch (ch) {
					case '\"':
						quote_on = 1;
						break;
					case ' ':
						term_done = 1;
						term_len--;
						break;
					case '(':
					case ')':
						term_done = 1;
						if (term_len > 0) {
							term_len--;
						}
						break;
					default:
						break;
					}
				}
				term_len++;
			}

			/* make sure the query term is not too long */
			if (term_len >= ETYMON_MAX_QUERY_TERM_SIZE) {
				/* make a temporary buffer to hold the term */
				char* term_tmp = (char*)(malloc(term_len + 1));
				if (term_tmp) {
					memcpy(term_tmp, query + term_start, term_len);
					term_tmp[term_len] = '\0';
				}
				if (term_tmp) {
					free(term_tmp);
				}
				/* close database files */
				if (etymon_af_state[state->dbid]->keep_open == 0) {
					etymon_af_close_files(state->dbid);
				}
				/* free result sets */
				while (--r_stack_p >= 0) {
					free(r_stack[r_stack_p]);
				}
				return aferr(AFETERMLEN);
			}

			memcpy(term, query + term_start, term_len);
			term[term_len] = '\0';
			
			/* process token */

			if (strcmp((char*)term, "|") == 0) {
				op_type = ETYMON_AF_OP_OR;
			}
			else if (strcmp((char*)term, "&") == 0) {
				op_type = ETYMON_AF_OP_AND;
			}
			else if (strcmp((char*)term, "(") == 0) {
				op_type = ETYMON_AF_OP_GROUP_OPEN;
			}
			else if (strcmp((char*)term, ")") == 0) {
				op_type = ETYMON_AF_OP_GROUP_CLOSE;
			}
			else {
				op_type = 0;
			}

			try_op = 0;
			
			switch (op_type) {

			case 0: /* search term */

				/* make sure there is space left on the r_stack */
				if (r_stack_p >= ETYMON_AF_MAX_R_STACK_DEPTH) {
					/* close database files */
					if (etymon_af_state[state->dbid]->keep_open == 0) {
						etymon_af_close_files(state->dbid);
					}
					/* free result sets */
					while (--r_stack_p >= 0) {
						free(r_stack[r_stack_p]);
					}
					return aferr(AFEQUERYNEST);
				}
				
				/* perform the search */
				if (etymon_af_search_term(state, term, 1, &(r_stack[r_stack_p]), &(rn_stack[r_stack_p])) == -1) {
					/* close database files */
					if (etymon_af_state[state->dbid]->keep_open == 0) {
						etymon_af_close_files(state->dbid);
					}
					/* free result sets */
					while (--r_stack_p >= 0) {
						free(r_stack[r_stack_p]);
					}
					return -1;
				}

				r_stack_p++;
				try_op = 1;
					
				break;

			case ETYMON_AF_OP_OR:
			case ETYMON_AF_OP_AND:
			case ETYMON_AF_OP_GROUP_OPEN:
				
				/* make sure there is space left on the op_stack */
				if (op_stack_p >= ETYMON_AF_MAX_OP_STACK_DEPTH) {
					/* close database files */
					if (etymon_af_state[state->dbid]->keep_open == 0) {
						etymon_af_close_files(state->dbid);
					}
					/* free result sets */
					while (--r_stack_p >= 0) {
						free(r_stack[r_stack_p]);
					}
					return aferr(AFEQUERYNEST);
				}

				/* push the operator onto the op_stack */
				op_stack[op_stack_p] = op_type;
				op_stack_p++;

				break;

			case ETYMON_AF_OP_GROUP_CLOSE:
				/* top element on op_stack should be ETYMON_AF_OP_GROUP_OPEN */
				if ( (op_stack_p <= 0) || (op_stack[op_stack_p - 1] != ETYMON_AF_OP_GROUP_OPEN) ) {
					/* close database files */
					if (etymon_af_state[state->dbid]->keep_open == 0) {
						etymon_af_close_files(state->dbid);
					}
					/* free result sets */
					while (--r_stack_p >= 0) {
						free(r_stack[r_stack_p]);
					}
					return aferr(AFEQUERYSYN);
				}
				
				/* pop ETYMON_AF_OP_GROUP_OPEN from op_stack */
				op_stack_p--;
				
				try_op = 1;
				
				break;

			default:
				break;

			}

			/* if try_op is set, we need to look for an
                           operator on the top of op_stack, and if we
                           find one, and there are enough operands for
                           it on r_stack, then execute the operation */
			if ( (try_op) && (op_stack_p > 0) ) {
				switch (op_stack[op_stack_p - 1]) {
				case ETYMON_AF_OP_OR:
					if (r_stack_p > 1) {
						if (etymon_af_boolean_or(state, r_stack, rn_stack, r_stack_p - 2, r_stack_p - 1)
						    == -1) {
							/* close database files */
							if (etymon_af_state[state->dbid]->keep_open == 0) {
								etymon_af_close_files(state->dbid);
							}
							/* free result sets */
							while (--r_stack_p >= 0) {
								free(r_stack[r_stack_p]);
							}
							return -1;
						}
						r_stack_p--;
						op_stack_p--;
					}
					break;
				case ETYMON_AF_OP_AND:
					if (r_stack_p > 1) {
						if (etymon_af_boolean_and(state, r_stack, rn_stack, r_stack_p - 2, r_stack_p - 1)
						    == -1) {
							/* close database files */
							if (etymon_af_state[state->dbid]->keep_open == 0) {
								etymon_af_close_files(state->dbid);
							}
							/* free result sets */
							while (--r_stack_p >= 0) {
								free(r_stack[r_stack_p]);
							}
							return -1;
						}
						r_stack_p--;
						op_stack_p--;
					}
					break;
				default:
					break;
				}
			}

			term_start += term_len;
			
		} /* while: term_start < query_len */

		/* there should be a single result set remaining on the r_stack */
		if (r_stack_p != 1) {
			/* close database files */
			if (etymon_af_state[state->dbid]->keep_open == 0) {
				etymon_af_close_files(state->dbid);
			}
			/* free result sets */
			while (--r_stack_p >= 0) {
				free(r_stack[r_stack_p]);
			}
			return aferr(AFEQUERYSYN);
		}

		/* add results to the main result set */
		if (rn_stack[0] > 0) {
			int x_r, x;
			x_r = state->optr->resultn;
			if (state->optr->result == NULL) {
				state->optr->result = (Afresult *) malloc((x_r + rn_stack[0]) * sizeof(Afresult));
			} else {
				state->optr->result = (Afresult *) realloc(state->optr->result, (x_r + rn_stack[0]) * sizeof(Afresult));
			}
			if (state->optr->result == NULL) {
				/* ERROR */
				return -1;
			}
			for (x = 0; x < rn_stack[0]; x++) {
				state->optr->result[x_r].dbid = state->dbid;
				state->optr->result[x_r].docid = (r_stack[0])[x].doc_id;
				state->optr->result[x_r].score = (r_stack[0])[x].w_d;
				x_r++;
			}
			state->optr->resultn += rn_stack[0];
		}

		if (r_stack[0]) {
			free(r_stack[0]);
		}
		
	}
	
	/* close database files */
	if (etymon_af_state[state->dbid]->keep_open == 0) {
		if (etymon_af_close_files(state->dbid) == -1) {
			return -1;
		}
	}
	
	return 0;
}


/* possible errors:
   EX_DB_ID_INVALID
int etymon_af_search(ETYMON_AF_SEARCH* opt) {
*/
int afsearch(const Afsearch *r, Afsearch_r *rr)
{
/*	int* p_db;*/
	int dbx;
	Uint2 dbid;
	ETYMON_AF_SEARCH_STATE state;

	/* validate database identifiers */
	for (dbx = 0; dbx < r->dbidn; dbx++) {
		dbid = r->dbid[dbx];
		if (etymon_af_state[dbid] == NULL)
			return aferr(AFEINVAL);
		/* this is only until we support unoptimized database searching */
/*
		if (etymon_af_state[*p_db]->info.optimized == 0) {
			fprintf(stderr,
				"af: %s: The current version cannot search a non-linearized database\n",
				etymon_af_state[*p_db]->dbname);
			return -1;
		}
*/
		if (etymon_af_state[dbid]->info.stemming && !af_stem_available()) {
			fprintf(stderr,
				"af: %s: Database requires stemming support\n",
				etymon_af_state[dbid]->dbname);
			return -1;
		}
	}
	
	/* build corpus data */
	state.corpus_doc_n = 0;
	if (r->score) {
		/* compute total number of (non-deleted) documents in
		   all databases to be searched */
		for (dbx = 0; dbx < r->dbidn; dbx++) {
			dbid = r->dbid[dbx];
			state.corpus_doc_n += etymon_af_state[dbid]->info.doc_n;
		}
	}

	/* clear results */
	rr->result = NULL;
	rr->resultn = 0;
	
	/* loop through each database to search */
	for (dbx = 0; dbx < r->dbidn; dbx++) {
		dbid = r->dbid[dbx];
		state.dbid = dbid;
		state.opt = (Afsearch *) r;
		state.optr = rr;
		if (r->qtype == AFQUERYBOOLEAN) {
			if (etymon_af_search_db(&state) == -1) {
				return -1;
			}
		} else {
			if (search_db_new(&state) == -1) {
				return -1;
			}
		}
	}

	/* scale results from 0 to 10000 */
	if (r->score && r->score_normalize) {
		int x;
		float high = 0;
		float low = 0;
		for (x = 0; x < rr->resultn; x++) {
			if (rr->result[x].score > high) {
				high = rr->result[x].score;
			}
			if (rr->result[x].score < low) {
				low = rr->result[x].score;
			}
		}
		for (x = 0; x < rr->resultn; x++)
			rr->result[x].score = (Uint4) ( ( ( ( (float) rr->result[x].score ) - low) * 10000 ) / (high - low) );
	}
	
	/* sort results */
	/*
	if (rr->resultn > 1) {
		/ sort by score /
		if (opt->sort_results == ETYMON_AF_SORT_SCORE) {
			qsort(opt->results, opt->results_n, sizeof(ETYMON_AF_RESULT), etymon_af_search_term_compare_score);
		} else {
			/ sort by doc_id /
			qsort(opt->results, opt->results_n, sizeof(ETYMON_AF_RESULT), etymon_af_search_term_compare_docid);
		}
	}
	*/
		
	return 0;
}


int afgetresultmd(const Afresult *result, int resultn, Afresultmd *resultmd)
{
	ETYMON_DOCTABLE doctable;
	int results_x;
	int db_id;
	int files_opened;

	memset(resultmd, 0, resultn * sizeof (Afresultmd));
	
	/* find each id currently in use */
	for (db_id = 1; db_id < ETYMON_AF_MAX_OPEN; db_id++) {

		/* if it's in use (i.e. it's an open database), then
                   we process all results with that db_id */
		if (etymon_af_state[db_id]) {

			/* we may not have any results for this db_id,
                           so no need to open the database files yet */
			files_opened = 0;

			/* loop through each result looking for
                           db_id's that match the database we are
                           currently working with */
			for (results_x = 0; results_x < resultn; results_x++) {

				/* test whether it's a relevant result */
				if (result[results_x].dbid == db_id) {

					/* we delay opening the
                                           database files until we
                                           find an actual result from
                                           the database */
					if (!files_opened) {
						/* open database files */
						if (etymon_af_state[db_id]->keep_open == 0) {
							if (etymon_af_open_files(db_id, O_RDONLY) == -1) {
								return -1;
							}
						}
						files_opened = 1;
					}

					/* now resolve the doc_id */
					if (etymon_af_lseek(etymon_af_state[db_id]->fd[ETYMON_DBF_DOCTABLE],
						     (etymon_af_off_t)( ((etymon_af_off_t)(result[results_x].docid - 1)) *
								 ((etymon_af_off_t)(sizeof(ETYMON_DOCTABLE))) ),
						     SEEK_SET) == -1) {
						perror("etymon_af_resolve_doc_id():lseek()");
					}
					if (read(etymon_af_state[db_id]->fd[ETYMON_DBF_DOCTABLE], &doctable,
						 sizeof(ETYMON_DOCTABLE)) == -1) {
						perror("etymon_af_resolve_doc_id():read()");
					}
/*					resultmd[results_x].docpath = afstrdup(doctable.filename);
					if (resultmd[results_x].docpath == NULL) {
						int x;
						/ run through all eresult[].filename and free all results /
						for (x = 0; x < resultn; x++) {
							if (resultmd[x].docpath) {
								free(resultmd[x].docpath);
							}
						}
						return aferr(AFEMEM);
					} */
					memcpy(resultmd[results_x].docpath, doctable.filename, AFPATHSIZE);
					resultmd[results_x].begin = doctable.begin;
					resultmd[results_x].end = doctable.end;
					resultmd[results_x].parent = doctable.parent;
					resultmd[results_x].deleted = doctable.deleted;
				} /* if: db_id's match */

			} /* for loop: results_x */

			/* close database files */
			if ( (etymon_af_state[db_id]->keep_open == 0) && (files_opened) ) {
				if (etymon_af_close_files(db_id) == -1) {
					return -1;
				}
			}
		}
	
	} /* for loop: db_id */

	return 0;
}

int afsortscore(Afresult *result, int resultn)
{
	if (resultn > 1)
		qsort(result, resultn, sizeof(Afresult), etymon_af_search_term_compare_score);
	return 0;
}

int afsortdocid(Afresult *result, int resultn)
{
	if (resultn > 1)
		qsort(result, resultn, sizeof(Afresult), etymon_af_search_term_compare_docid);
	return 0;
}
