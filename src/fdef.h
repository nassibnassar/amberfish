#ifndef _AF_FDEF_H
#define _AF_FDEF_H

#include "defs.h"

typedef struct {
	unsigned char* word;
	ETYMON_INDEX_INDEXING_STATE* state;
} ETYMON_AF_FDEF_RESOLVE_FIELD;

Uint2 etymon_af_fdef_read_mem(int fdef_fd, ETYMON_AF_FDEF_MEM** root, ETYMON_AF_FDEF_MEM** tail);

void etymon_af_fdef_write_mem(int fdef_fd, ETYMON_AF_FDEF_MEM* root);

void etymon_af_fdef_free_mem(ETYMON_AF_FDEF_MEM* root);

Uint2 etymon_af_fdef_resolve_field(ETYMON_AF_FDEF_RESOLVE_FIELD* opt);

Uint2 etymon_af_fdef_get_field(ETYMON_AF_FDEF_DISK* fdef_disk, unsigned char* field_name);

#endif
