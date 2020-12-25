#ifndef _AF_LINEAR_H
#define _AF_LINEAR_H

typedef struct {
	char *db;
	int verbose;
	int memory;
	int nobuffer;
} Aflinear;

int _aflinear(const Aflinear *rq);

/*int _etymon_index_optimize(ETYMON_INDEX_OPTIONS *opt);*/

#endif
