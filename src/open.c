/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

/***** old *****/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "open.h"
#include "util.h"
#include "lock.h"
#include "stem.h"

static int etymon_af_init_flag = 0;

ETYMON_AF_STATE* etymon_af_state[ETYMON_AF_MAX_OPEN];

void etymon_af_init() {
	memset(etymon_af_state, 0, sizeof(ETYMON_AF_STATE*) * ETYMON_AF_MAX_OPEN);
	etymon_af_init_flag = 1;
}


/* assumes that db_id is valid */
int etymon_af_open_files(Uint2 db_id, int flags) {
	int x_fn, x;
	
	/* open all database files */
	for (x_fn = 0; x_fn < ETYMON_AF_MAX_DB_FILES; x_fn++) {
		etymon_af_state[db_id]->fd[x_fn] = open(etymon_af_state[db_id]->fn[x_fn], flags | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
		if (etymon_af_state[db_id]->fd[x_fn] == -1) {
			/* before we leave, make an attempt to close all files */
			for (x = 0; x < x_fn; x++) {
				close(etymon_af_state[db_id]->fd[x]);
			}
			return aferr(AFEDBIO);
		}
	}
	return 0;
}


/* assumes that db_id is valid */
int etymon_af_close_files(Uint2 db_id) {
	int x_fn;
	
	/* close all the database files */
	for (x_fn = 0; x_fn < ETYMON_AF_MAX_DB_FILES; x_fn++) {
		if (close(etymon_af_state[db_id]->fd[x_fn]) == -1)
			return aferr(AFEDBIO);
	}

	return 0;
}


/* returns a database identifier in the range 1..255 */
int etymon_af_open(ETYMON_AF_OPEN* opt) {
	int db_id;
	Uint4 magic;
	ssize_t nbytes;
	int x_fn;
	int flags;
	int x;
	ETYMON_AF_STAT st;

	/* check for option errors */
	if (opt->dbname == NULL)
		return aferr(AFEINVAL);
	if ( (opt->read_only) && (opt->create) )
		return aferr(AFEINVAL);
	
	/* make sure we are initialized */
	if (etymon_af_init_flag == 0) {
		etymon_af_init();
	}

	/* find next free id */
	db_id = 1;
	while ( (db_id < ETYMON_AF_MAX_OPEN) && (etymon_af_state[db_id] != NULL) ) {
		db_id++;
	}
	if (db_id >= ETYMON_AF_MAX_OPEN)
		return aferr(AFEOPENLIM);

	/* create new table at the free id */
	etymon_af_state[db_id] = (ETYMON_AF_STATE*)(malloc(sizeof(ETYMON_AF_STATE)));
	if (etymon_af_state[db_id] == NULL)
		return aferr(AFEMEM);
	etymon_af_state[db_id]->dbname = opt->dbname;
	etymon_af_state[db_id]->keep_open = opt->keep_open;
	etymon_af_state[db_id]->read_only = opt->read_only;
	
	/* construct file names */
	for (x_fn = 0; x_fn < ETYMON_AF_MAX_DB_FILES; x_fn++) {
		etymon_db_construct_path(x_fn, opt->dbname, etymon_af_state[db_id]->fn[x_fn]);
	}
	
	/* create database */
	if (opt->create) {

		/* open/create all database files */
		if (etymon_af_open_files(db_id, O_WRONLY | O_CREAT | O_TRUNC) == -1) {
			free(etymon_af_state[db_id]);
			return -1;
		}		

		/* clear any lock and then lock the database */
		etymon_db_unlock(opt->dbname);
		etymon_db_lock(opt->dbname, NULL);
		
		/* initialize dbinfo */
		magic = ETYMON_INDEX_MAGIC;
		nbytes = write(etymon_af_state[db_id]->fd[ETYMON_DBF_INFO], &magic, sizeof(Uint4));
		if (nbytes != sizeof(Uint4)) {
			free(etymon_af_state[db_id]);
			return aferr(AFEDBIO);
		}
		sprintf(etymon_af_state[db_id]->info.version_stamp, ETYMON_AF_BANNER_STAMP);
		etymon_af_state[db_id]->info.udict_root = 0;
		etymon_af_state[db_id]->info.doc_n = 0;
		etymon_af_state[db_id]->info.optimized = 0;
		etymon_af_state[db_id]->info.phrase = opt->phrase;
		etymon_af_state[db_id]->info.word_proximity = 0;
		etymon_af_state[db_id]->info.stemming = af_stem_available() ? opt->stem : 0;
		/* write db info */
		nbytes = write(etymon_af_state[db_id]->fd[ETYMON_DBF_INFO], &(etymon_af_state[db_id]->info), sizeof(ETYMON_DB_INFO));
		if (nbytes != sizeof(ETYMON_DB_INFO)) {
			free(etymon_af_state[db_id]);
			return aferr(AFEDBIO);
		}
	    
		/* close all the database files */
		if (etymon_af_close_files(db_id) == -1)
			return -1;

		/* unlock the database */
		etymon_db_unlock(opt->dbname);

	} /* create */ else {

		if (etymon_db_ready(opt->dbname) == 0) {
			free(etymon_af_state[db_id]);
			return aferr(AFEDBLOCK);
		}

	}

	/* open all database files */
	if (opt->read_only) {
		flags = O_RDONLY;
	} else {
		flags = O_RDWR;
	}
	if (etymon_af_open_files(db_id, flags) == -1) {
		free(etymon_af_state[db_id]);
		return -1;
	}		
	
	/* cache database information */
        nbytes = read(etymon_af_state[db_id]->fd[ETYMON_DBF_INFO], &magic, sizeof(Uint4));
        if (nbytes != sizeof(Uint4)) {
		/* before we leave, make an attempt to close all files and free table */
		for (x = 0; x < x_fn; x++) {
			close(etymon_af_state[db_id]->fd[x]);
		}
		free(etymon_af_state[db_id]);
		return aferr(AFEDBIO);
        }
	if (magic != ETYMON_INDEX_MAGIC) {
		/* before we leave, make an attempt to close all files and free table */
		for (x = 0; x < x_fn; x++) {
			close(etymon_af_state[db_id]->fd[x]);
		}
		free(etymon_af_state[db_id]);
		return aferr(AFEVERSION);
	}
	nbytes = read(etymon_af_state[db_id]->fd[ETYMON_DBF_INFO], &(etymon_af_state[db_id]->info), sizeof(ETYMON_DB_INFO));
	if (nbytes != sizeof(ETYMON_DB_INFO)) {
		/* before we leave, make an attempt to close all files and free table */
		for (x = 0; x < x_fn; x++) {
			close(etymon_af_state[db_id]->fd[x]);
		}
		free(etymon_af_state[db_id]);
		return aferr(AFEDBIO);
	}

	/* cache field definitions */
	if (etymon_af_fstat(etymon_af_state[db_id]->fd[ETYMON_DBF_FDEF], &st) == -1) {
		perror("etymon_af_open:fstat()");
	}
	if (st.st_size > 0) {
		/* read fdef table into array */
		etymon_af_state[db_id]->fdef = (ETYMON_AF_FDEF_DISK*)(malloc(st.st_size));
		if (etymon_af_state[db_id]->fdef == NULL)
			return aferr(AFEMEM);
		if (read(etymon_af_state[db_id]->fd[ETYMON_DBF_FDEF], etymon_af_state[db_id]->fdef, st.st_size) == -1) {
			/* before we leave, make an attempt to close all files and free table */
			for (x = 0; x < x_fn; x++) {
				close(etymon_af_state[db_id]->fd[x]);
			}
			free(etymon_af_state[db_id]);
			return aferr(AFEDBIO);
		}
		etymon_af_state[db_id]->fdef_count = st.st_size / sizeof(ETYMON_AF_FDEF_DISK);
	} else {
		etymon_af_state[db_id]->fdef = NULL;
		etymon_af_state[db_id]->fdef_count = 0;
	}
	
	/* close files */
	if (opt->keep_open == 0) {
		/* close all the database files */
		if (etymon_af_close_files(db_id) == -1) {
			return -1;
		}
	}

	return db_id;
}

int etymon_af_close(ETYMON_AF_CLOSE* opt) {
	ssize_t nbytes;
	
	/* make sure we have a valid pointer in the table */
	if ( (opt->db_id < 1) || (etymon_af_state[opt->db_id] == NULL) )
		return aferr(AFEINVAL);

	/* write out cached database info */
	if (etymon_af_state[opt->db_id]->read_only == 0) {
		/* first we may have to open the file */
		if (etymon_af_state[opt->db_id]->keep_open == 0) {
			etymon_af_state[opt->db_id]->fd[ETYMON_DBF_INFO] = open(etymon_af_state[opt->db_id]->fn[ETYMON_DBF_INFO],
									 O_RDWR | ETYMON_AF_O_LARGEFILE, ETYMON_DB_PERM);
			if (etymon_af_state[opt->db_id]->fd[ETYMON_DBF_INFO] == -1)
				return aferr(AFEDBIO);
		}
		/* now write out dbinfo */
		if (etymon_af_lseek(etymon_af_state[opt->db_id]->fd[ETYMON_DBF_INFO], (etymon_af_off_t)4, SEEK_SET) == -1)
			return aferr(AFEDBIO);
		nbytes = write(etymon_af_state[opt->db_id]->fd[ETYMON_DBF_INFO], &(etymon_af_state[opt->db_id]->info),
			       sizeof(ETYMON_DB_INFO));
		if (nbytes != sizeof(ETYMON_DB_INFO))
			return aferr(AFEDBIO);
		/* close the file if we opened it */
		if (etymon_af_state[opt->db_id]->keep_open == 0) {
			if (close(etymon_af_state[opt->db_id]->fd[ETYMON_DBF_INFO]) == -1)
				return aferr(AFEDBIO);
		}
	}
	
	/* close database files if they are open */
	if (etymon_af_state[opt->db_id]->keep_open) {
		if (etymon_af_close_files(opt->db_id) == -1) {
			return -1;
		}
	}

	/* free the table */
	if (etymon_af_state[opt->db_id]->fdef) {
		free(etymon_af_state[opt->db_id]->fdef);
	}
	free(etymon_af_state[opt->db_id]);
	etymon_af_state[opt->db_id] = NULL;
	
	return 0;
}

/***** new *****/

static int setomode(const char *mode, ETYMON_AF_OPEN *op)
{
	if (mode[0] == 'r') {
		op->read_only = mode[1] == '+' ? 0 : 1;
		op->create = 0;
		return 0;
	}
	if (mode[0] == 'w' && mode[1] == '+') {
		op->read_only = 0;
		op->create = 1;
		return 0;
	}
	return aferr(AFEINVAL);
}

int afopen(const Afopen *r, Afopen_r *rr)
{
	ETYMON_AF_OPEN op;
	int x;
	
	op.dbname = r->dbpath;
	if (setomode(r->mode, &op) < 0)
		return -1;
	op.keep_open = 0;
	op.phrase = r->phrase;
	op.stem = r->stem;
	if ((x = etymon_af_open(&op)) == -1)
		return -1;
	rr->dbid = x;
	return 0;
}

int afclose(const Afclose *r, Afclose_r *rr)
{
	ETYMON_AF_CLOSE c;

	c.db_id = r->dbid;
	return etymon_af_close(&c);
}
