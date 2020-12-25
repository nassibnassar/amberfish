/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <stdlib.h>
#include <string.h>
#include "explain.h"
#include "open.h"

extern ETYMON_AF_STATE* etymon_af_state[];

int etymon_af_fdef_list_fields_recursive(ETYMON_AF_FDEF_DISK* fdef, ETYMON_AF_EXPLAIN* opt, int p, int* n) {

	if (fdef[p - 1].left) {
		etymon_af_fdef_list_fields_recursive(fdef, opt, fdef[p - 1].left, n);
	}

	memcpy(opt->fields[*n].name, fdef[p - 1].name, ETYMON_AF_MAX_FIELDNAME_SIZE);
	(*n)++;

	if (fdef[p - 1].right) {
		etymon_af_fdef_list_fields_recursive(fdef, opt, fdef[p - 1].right, n);
	}

	return 0;
}


int etymon_af_fdef_list_fields(ETYMON_AF_FDEF_DISK* fdef, ETYMON_AF_EXPLAIN* opt) {
	int n;
	
	/* first check to see if there are no fields */
	if (etymon_af_state[opt->db_id]->fdef_count == 0) {
		opt->fields = NULL;
		opt->fields_n = 0;
		return 0;
	}

	/* allocate the output buffer */
	opt->fields = (ETYMON_AF_FIELD_NAME*)(malloc(etymon_af_state[opt->db_id]->fdef_count *
						     sizeof(ETYMON_AF_FIELD_NAME)));
	if (opt->fields == NULL) {
		etymon_af_log(opt->log, EL_CRITICAL, EX_MEMORY, "etymon_af_explain()", etymon_af_state[opt->db_id]->dbname,
			      NULL, NULL);
		return -1;
	}

	n = 0;
	etymon_af_fdef_list_fields_recursive(fdef, opt, 1, &n);
	opt->fields_n = n;
	return 0;
}


/* possible errors:
   EX_IO
   EX_DB_ID_INVALID
*/
int etymon_af_explain(ETYMON_AF_EXPLAIN* opt) {

	/* make sure we have a valid pointer in the table */
	if ( (opt->db_id < 1) || (etymon_af_state[opt->db_id] == NULL) ) {
		etymon_af_log(opt->log, EL_ERROR, EX_DB_ID_INVALID, "etymon_af_explain()", NULL, NULL, NULL);
		return -1;
	}

	if (opt->list_fields) {
		etymon_af_fdef_list_fields(etymon_af_state[opt->db_id]->fdef, opt);
	}
	
	return 0;
}
