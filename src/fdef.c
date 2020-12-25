/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "fdef.h"

/* assumes we are positioned at the beginning of the file */
Uint2 etymon_af_fdef_read_mem(int fdef_fd, ETYMON_AF_FDEF_MEM** root, ETYMON_AF_FDEF_MEM** tail) {
	ETYMON_AF_STAT st;
	etymon_af_off_t fdef_count;
	ETYMON_AF_FDEF_MEM** fdef_array;
	Uint2 x;
	ETYMON_AF_FDEF_DISK fdef_disk_buf;
	
	/* stat fdef file to get size */
	if (etymon_af_fstat(fdef_fd, &st) == -1) {
		perror("fdef_read_mem():fstat()");
	}
	fdef_count = st.st_size / sizeof(ETYMON_AF_FDEF_DISK);
	if (fdef_count == 0) {
		*root = NULL;
		*tail = NULL;
		return 0;
	}

	/* create an array of pointers to point to each node */
	fdef_array = (ETYMON_AF_FDEF_MEM**)(malloc(sizeof(ETYMON_AF_FDEF_MEM*) * fdef_count));
	for (x = 0; x < fdef_count; x++) {
		fdef_array[x] = (ETYMON_AF_FDEF_MEM*)(malloc(sizeof(ETYMON_AF_FDEF_MEM)));
	}

	/* read in each node, allocating memory and adding to fdef_array */
	for (x = 0; x < fdef_count; x++) {
		if (read(fdef_fd, &fdef_disk_buf, sizeof(ETYMON_AF_FDEF_DISK)) == -1) {
			perror("fdef_read_mem():read()");
		}
		fdef_array[x]->n = x + 1;
		memcpy(fdef_array[x]->name, fdef_disk_buf.name, ETYMON_AF_MAX_FIELDNAME_SIZE);
		if (fdef_disk_buf.left == 0) {
			fdef_array[x]->left = NULL;
		} else {
			fdef_array[x]->left = fdef_array[fdef_disk_buf.left - 1];
		}
		if (fdef_disk_buf.right == 0) {
			fdef_array[x]->right = NULL;
		} else {
			fdef_array[x]->right = fdef_array[fdef_disk_buf.right - 1];
		}
		if ((x + 1) < fdef_count) {
			fdef_array[x]->next = fdef_array[x + 1];
		} else {
			fdef_array[x]->next = NULL;
		}
	}

	*root = fdef_array[0];
	*tail = fdef_array[fdef_count - 1];
	
	free(fdef_array);

	return fdef_count;
}


void etymon_af_fdef_write_mem(int fdef_fd, ETYMON_AF_FDEF_MEM* root) {
	ETYMON_AF_FDEF_DISK fdef_disk_buf;
	ETYMON_AF_FDEF_MEM* p;
	
	/* seek to the beginning of the file and truncate to 0 length */
	/* no need to do this anymore since we are now close and reopen the
	   file in index.c */
	/*
	if (etymon_af_lseek(fdef_fd, (etymon_af_off_t)0, SEEK_SET) == -1) {
		perror("fdef_write_mem():lseek()");
	}
	if (etymon_af_ftruncate(fdef_fd, (etymon_af_off_t)0) == -1) {
		perror("fdef_write_mem():ftruncate()");
	}
	*/
	
	p = root;
	while (p != NULL) {
		memcpy(fdef_disk_buf.name, p->name, ETYMON_AF_MAX_FIELDNAME_SIZE);
		if (p->left) {
			fdef_disk_buf.left = p->left->n;
		} else {
			fdef_disk_buf.left = 0;
		}
		if (p->right) {
			fdef_disk_buf.right = p->right->n;
		} else {
			fdef_disk_buf.right = 0;
		}
		if (write(fdef_fd, &fdef_disk_buf, sizeof(ETYMON_AF_FDEF_DISK)) == -1) {
			perror("fdef_write_mem():write()");
		}
		p = p->next;
	}
}


void etymon_af_fdef_free_mem(ETYMON_AF_FDEF_MEM* root) {
	ETYMON_AF_FDEF_MEM* p;
	ETYMON_AF_FDEF_MEM* op;

	p = root;
	while (p != NULL) {
		op = p;
		p = p->next;
		free(op);
	}
}


Uint2 etymon_af_fdef_resolve_field(ETYMON_AF_FDEF_RESOLVE_FIELD* opt) {
	ETYMON_INDEX_INDEXING_STATE* state = opt->state;
	unsigned char* word = opt->word;
	ETYMON_AF_FDEF_MEM* p;
	int comp;
	ETYMON_AF_FDEF_MEM** parent;

	comp = 0;
	parent = &(state->fdef_root);
	p = state->fdef_root;
	while ( (p != NULL) && ((comp = strcmp((char*)word, (char*)(p->name))) != 0) ) {
		if (comp < 0) {
			parent = &(p->left);
			p = p->left;
		} else {
			parent = &(p->right);
			p = p->right;
		}
	}

	if (p) {
		/* found it! */
		return p->n;
	} else {
		/* create a new node */
		*parent = (ETYMON_AF_FDEF_MEM*)(malloc(sizeof(ETYMON_AF_FDEF_MEM)));
		if (state->fdef_tail) {
			state->fdef_tail->next = *parent;
		}
		state->fdef_tail = *parent;
		(*parent)->n = state->fdef_count + 1;
		state->fdef_count++;
		strcpy((char*)((*parent)->name), (char*)word);
		(*parent)->left = NULL;
		(*parent)->right = NULL;
		(*parent)->next = NULL;
		return (*parent)->n;
	}
}


Uint2 etymon_af_fdef_get_field(ETYMON_AF_FDEF_DISK* fdef_disk, unsigned char* field_name) {
	int fdef_p;
	int comp;

	if (fdef_disk == NULL) {
		return 0;
	}
	
	fdef_p = 1;
	comp = 0;
	while ( (fdef_p != 0) && ((comp = strcmp((char*)field_name, (char*)(fdef_disk[fdef_p - 1].name))) != 0) ) {
		if (comp < 0) {
			fdef_p = fdef_disk[fdef_p - 1].left;
		} else {
			fdef_p = fdef_disk[fdef_p - 1].right;
		}
	}

	if (fdef_p != 0) {
		/* found it! */
		return fdef_p;
	} else {
		return 0;
	}
}
