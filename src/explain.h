#ifndef _AF_EXPLAIN_H
#define _AF_EXPLAIN_H

#include "defs.h"
#include "log.h"

typedef struct {
	char name[ETYMON_AF_MAX_FIELDNAME_SIZE];
} ETYMON_AF_FIELD_NAME;

typedef struct {
	int db_id;
	int list_fields;
	ETYMON_AF_FIELD_NAME* fields;
	int fields_n;
	ETYMON_AF_LOG* log;
} ETYMON_AF_EXPLAIN;

int etymon_af_explain(ETYMON_AF_EXPLAIN* opt);

#endif
