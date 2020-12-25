/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <stdio.h>
#include <string.h>
#include "linear.h"
#include "lock.h"
#include "util.h"
#include "info.h"
#include "linbuf.h"

typedef struct {
	const Aflinear *rq;
	int postx;
	Affile f;
	Dbinfo info;
	off_t udictp;
	off_t udictnb;
	Uint4 upostp;
	off_t upostn;
	Uint4 ufieldp;
	off_t ufieldn;
	Uint4 uwordp;
	off_t uwordn;
	off_t lpostn;
	off_t lfieldn;
	off_t lwordn;
	Uint4 lpostpsave;
	Uint4 lfieldpsave;
	Uint4 fieldc;
	Uint4 lwordpsave;
	Uint4 wordc;
	ETYMON_INDEX_PAGE_L pagel;
	ETYMON_INDEX_UPOST upost;
	ETYMON_INDEX_LPOST lpost;
	ETYMON_INDEX_UFIELD ufield;
	ETYMON_INDEX_LFIELD lfield;
	ETYMON_INDEX_UWORD uword;
	ETYMON_INDEX_LWORD lword;
} Aflinst;

static int getlock(const char *db)
{
	if (!etymon_db_ready(db))
		return aferr(AFEDBLOCK);
	if (etymon_db_lock(db, NULL) < 0)
		return -1;

	return 0;
}

static int freelock(const char *db)
{
	etymon_db_unlock(db);

	return 0;
}

static int openfiles(const char *db, Affile *f)
{
	memset(f, 0, sizeof *f);
	if (!(f->info = afopendbf(db, AFFTINFO, "r+b")))
		return -1;
	if (!(f->udict = afopendbf(db, AFFTUDICT, "r+b")))
		return -1;
	if (!(f->upost = afopendbf(db, AFFTUPOST, "rb")))
		return -1;
	if (!(f->ufield = afopendbf(db, AFFTUFIELD, "rb")))
		return -1;
	if (!(f->uword = afopendbf(db, AFFTUWORD, "rb")))
		return -1;
	if (!(f->lpost = afopendbf(db, AFFTLPOST, "ab")))
		return -1;
	if (!(f->lfield = afopendbf(db, AFFTLFIELD, "ab")))
		return -1;
	if (!(f->lword = afopendbf(db, AFFTLWORD, "ab")))
		return -1;
	return 0;
}

static int closefile(FILE *f)
{
	if (f) {
		if (fclose(f) != 0)
			return aferr(AFEDBIO);
	}
	
	return 0;
}

static int truncfile(const char *db, int type)
{
	FILE *f;

	if (!(f = afopendbf(db, type, "wb")))
		return aferr(AFEDBIO);
	if (fclose(f) != 0)
		return aferr(AFEDBIO);

	return 0;
}

static int truncufiles(const char *db)
{
	if (truncfile(db, AFFTUPOST) < 0)
		return -1;
	if (truncfile(db, AFFTUFIELD) < 0)
		return -1;
	if (truncfile(db, AFFTUFIELD) < 0)
		return -1;

	return 0;
}

static int closefiles(Affile *f)
{
	if (closefile(f->info) < 0)
		return -1;
	if (closefile(f->udict) < 0)
		return -1;
	if (closefile(f->upost) < 0)
		return -1;
	if (closefile(f->ufield) < 0)
		return -1;
	if (closefile(f->uword) < 0)
		return -1;
	if (closefile(f->lpost) < 0)
		return -1;
	if (closefile(f->lfield) < 0)
		return -1;
	if (closefile(f->lword) < 0)
		return -1;

	return 0;
}

static int getfsizes(Aflinst *t)
{
	off_t nb;

	if (afgetfsize(t->f.udict, &t->udictnb) < 0)
		return -1;
	
	if (afgetfsize(t->f.upost, &nb) < 0)
		return -1;
	t->upostn = nb / sizeof (ETYMON_INDEX_UPOST);
	if (afgetfsize(t->f.ufield, &nb) < 0)
		return -1;
	t->ufieldn = nb / sizeof (ETYMON_INDEX_UFIELD);
	if (afgetfsize(t->f.uword, &nb) < 0)
		return -1;
	t->uwordn = nb / sizeof (ETYMON_INDEX_UWORD);
	if (afgetfsize(t->f.lpost, &nb) < 0)
		return -1;
	t->lpostn = nb / sizeof (ETYMON_INDEX_LPOST);
	if (afgetfsize(t->f.lfield, &nb) < 0)
		return -1;
	t->lfieldn = nb / sizeof (ETYMON_INDEX_LFIELD);
	if (afgetfsize(t->f.lword, &nb) < 0)
		return -1;
	t->lwordn = nb / sizeof (ETYMON_INDEX_LWORD);

	return 0;
}

static int seekleftleaf(Aflinst *t)
{
	Uint1 leaf;
	ETYMON_INDEX_PAGE_NL pagenl;
	
	t->udictp = t->info.udict_root;
	while (1) {
		if (fseeko(t->f.udict, (off_t) t->udictp, SEEK_SET) < 0)
			return aferr(AFEDBIO);
		if (fread(&leaf, 1, 1, t->f.udict) < 0)
			return aferr(AFEDBIO);
		if (leaf)
			return 0;
		if (fread(&pagenl, 1, sizeof pagenl, t->f.udict) < sizeof pagenl)
			return aferr(AFEDBIO);
		t->udictp = pagenl.p[0];
	}
}

static int readpagel(Aflinst *t)
{
	if (fseeko(t->f.udict, (off_t) (t->udictp + 1), SEEK_SET) < 0)
		return aferr(AFEDBIO);
	if (fread(&(t->pagel), 1, sizeof t->pagel, t->f.udict) < sizeof t->pagel)
		return aferr(AFEDBIO);

	return 0;
}

static int readupost(Aflinst *t)
{
	if (t->rq->nobuffer) {
		if (fseeko(t->f.upost,
			   (off_t) ( ((off_t) (t->upostp - 1)) *
				     ((off_t) (sizeof t->upost)) ),
			   SEEK_SET) < 0)
			return aferr(AFEDBIO);
		if (fread(&(t->upost), 1, sizeof t->upost, t->f.upost) < sizeof t->upost)
			return aferr(AFEDBIO);
	} else {
		if (aflinread(&(t->upost), 
			      (off_t) ( ((off_t) (t->upostp - 1)) *
					((off_t) (sizeof t->upost)) ),
			      sizeof t->upost) < 0)
			return -1;
	}
		  
	return 0;
}

static int readufield(Aflinst *t)
{
	if (fseeko(t->f.ufield,
		   (off_t) ( ((off_t) (t->ufieldp - 1)) *
			     ((off_t) (sizeof t->ufield)) ),
		   SEEK_SET) < 0)
		return aferr(AFEDBIO);
	if (fread(&(t->ufield), 1, sizeof t->ufield, t->f.ufield) <
	    sizeof t->ufield)
		return aferr(AFEDBIO);

	return 0;
}

static int writelfield(Aflinst *t)
{
	if (fwrite(&(t->lfield), 1, sizeof t->lfield, t->f.lfield) <
	    sizeof t->lfield)
		return aferr(AFEDBIO);

	return 0;
}

static int linfield(Aflinst *t)
{
	/* do we need to look for duplicates?? */
	t->lfieldpsave = t->lfieldn + 1;
	t->fieldc = 0;
	t->ufieldp = t->upost.fields;
	while (t->ufieldp) {
		t->fieldc++;
		if (readufield(t) < 0)
			return -1;
		memcpy(t->lfield.fields, t->ufield.fields,
		       ETYMON_MAX_FIELD_NEST * 2);
		if (writelfield(t) < 0)
			return -1;
		(t->lfieldn)++;
		t->ufieldp = t->ufield.next;
	}
		
	return 0;
}

static int readuword(Aflinst *t)
{
	if (fseeko(t->f.uword,
		   (off_t) ( ((off_t) (t->uwordp - 1)) *
			     ((off_t) (sizeof t->uword)) ),
		   SEEK_SET) < 0)
		return aferr(AFEDBIO);
	if (fread(&(t->uword), 1, sizeof t->uword, t->f.uword) <
	    sizeof t->uword)
		return aferr(AFEDBIO);

	return 0;
}

static int writelword(Aflinst *t)
{
	if (fwrite(&(t->lword), 1, sizeof t->lword, t->f.lword) <
	    sizeof t->lword)
		return aferr(AFEDBIO);

	return 0;
}

static int linwn(Aflinst *t)
{
	t->lwordpsave = t->lwordn + 1;
	t->wordc = 0;
	t->uwordp = t->upost.word_numbers;
	while (t->uwordp) {
		t->wordc++;
		if (readuword(t) < 0)
			return -1;
		t->lword.wn = t->uword.wn;
		if (writelword(t) < 0)
			return -1;
		(t->lwordn)++;
		t->uwordp = t->uword.next;
	}
		
	return 0;
}

static int writelpost(Aflinst *t)
{
	if (fwrite(&(t->lpost), 1, sizeof t->lpost, t->f.lpost) <
	    sizeof t->lpost)
		return aferr(AFEDBIO);

	return 0;
}

static int updatelpost(Aflinst *t)
{
	/* compare the doc_id with our cached lpost */
	if (t->upost.doc_id == t->lpost.doc_id) {
		/* increment the frequency and field count */
		t->lpost.freq += t->upost.freq;
		t->lpost.fields_n += t->fieldc;
		t->lpost.word_numbers_n += t->wordc;
	} else {
		/* flush lpost */
		/* only flush if lpost contains something */
		if (t->lpost.doc_id) {
			if (writelpost(t) < 0)
				return -1;
			(t->lpostn)++;
			(t->pagel.post_n[t->postx])++;
		}
		/* replace lpost with upost */
		t->lpost.doc_id = t->upost.doc_id;
		t->lpost.freq = t->upost.freq;
		t->lpost.fields_n = t->fieldc;
		t->lpost.word_numbers_n = t->wordc;
		/* set field pointer */
		t->lpost.fields = t->lfieldpsave;
		t->lpost.word_numbers = t->lwordpsave;
	}

	return 0;
}

static int writepagel(Aflinst *t)
{
	if (fseeko(t->f.udict, (off_t) (t->udictp + 1), SEEK_SET) < 0)
		return aferr(AFEDBIO);
	if (fwrite(&(t->pagel), 1, sizeof t->pagel, t->f.udict) <
	    sizeof t->pagel)
		return aferr(AFEDBIO);

	return 0;
}

static void debugpostings(Aflinst *t)
{
	if (t->rq->verbose >= 6) {
		int x;
		afprintvp(t->rq->verbose, 6);
		printf("Read postings (key=\"");
		for (x = t->pagel.offset[t->postx];
		     x < t->pagel.offset[t->postx + 1];
		     x++)
			printf("%c", (char) t->pagel.keys[x]);
		printf("\")\n");
	}
}

static int linpost(Aflinst *t)
{
	for (t->postx = 0; t->postx < t->pagel.n; (t->postx)++) {
		debugpostings(t);
		t->pagel.post_n[t->postx] = 0;
		t->lpostpsave = t->lpostn + 1;
		t->upostp = t->pagel.post[t->postx];
		t->lpost.doc_id = 0;
		while (t->upostp) {
			afprintv(t->rq->verbose, 6, "Read posting");
			if (readupost(t) < 0)
				return -1;
			afprintv(t->rq->verbose, 6, "Linearize fields");
			if (linfield(t) < 0)
				return -1;
			afprintv(t->rq->verbose, 6, "Linearize word numbers");
			if (linwn(t) < 0)
				return -1;
			if (updatelpost(t) < 0)
				return -1;
			t->upostp = t->upost.next;
		}
		/* flush lpost if it contains something */
		if (t->lpost.doc_id) {
			if (writelpost(t) < 0)
				return -1;
			(t->lpostn)++;
			(t->pagel.post_n[t->postx])++;
		}
		t->pagel.post[t->postx] = t->lpostpsave;
	}
	if (writepagel(t) < 0)
		return -1;
	t->udictp = t->pagel.next;
	
	return 0;
}

static void debugpagel(Aflinst *t)
{
	if (t->rq->verbose >= 6) {
		afprintvp(t->rq->verbose, 6);
		printf("Leaf node: keys=\"%s\"\n",
		       (char *) t->pagel.keys);
	}
}

/* this was written before the advent of ETYMON_INDEX_PAGE_L.post_n[],
   ETYMON_INDEX_UPOST.fields_n, and ETYMON_INDEX_UPOST.word_numbers_n
   in the first unlinearized pass; so it explicitly counts these
   values while building the linear structures */
static int linearize(Aflinst *t)
{
	afprintv(t->rq->verbose, 5, "Seek to leftmost leaf node");
	if (seekleftleaf(t) < 0)
		return -1;
	do {
		afprintv(t->rq->verbose, 5, "Read leaf node");
		if (readpagel(t) < 0)
			return -1;
		debugpagel(t);
		afprintv(t->rq->verbose, 5, "Linearize postings");
		if (linpost(t) < 0)
			return -1;
	} while (t->udictp);

	return 0;
}

static int linopen(Aflinst *t)
{
	afprintv(t->rq->verbose, 4, "Locking database");
	if (getlock(t->rq->db) < 0)
		return -1;

	afprintv(t->rq->verbose, 4, "Opening database files");
	if (openfiles(t->rq->db, &(t->f)) < 0)
		return -1;

	afprintv(t->rq->verbose, 4, "Reading database information");
	if (afreadinfo(t->f.info, &(t->info)) < 0)
		return -1;

	afprintv(t->rq->verbose, 4, "Checking file sizes");
	if (getfsizes(t) < 0)
		return -1;

	if (!t->rq->nobuffer) {
		if (aflinbuf(t->f.upost, t->rq->memory) < 0)
			return -1;
	}

	return 0;
}

static int linclose(Aflinst *t)
{
	t->info.optimized = 1;

	afprintv(t->rq->verbose, 4, "Writing database information");
	if (afwriteinfo(t->f.info, &(t->info)) < 0)
		return -1;

	afprintv(t->rq->verbose, 4, "Closing database files");
	if (closefiles(&(t->f)) < 0)
		return -1;
	
	afprintv(t->rq->verbose, 4, "Truncating unlinearized files");
	if (truncufiles(t->rq->db) < 0)
		return -1;

	afprintv(t->rq->verbose, 4, "Unlocking database");
	if (freelock(t->rq->db) < 0)
		return -1;

	return 0;
}

int _aflinear(const Aflinear *rq)
{
	Aflinst t;

	t.rq = rq;
	afprintv(rq->verbose, 2, "Linearizing");

	afprintv(rq->verbose, 3, "Opening database");
	if (linopen(&t) < 0)
		return -1;
	afprintv(rq->verbose, 4, "Checking if database is linearized");
	/* exit if db is already linearized */
	if (t.info.optimized) {
		if (afclosedbf(&t.f) < 0)
			return -1;
		if (freelock(t.rq->db) < 0)
			return -1;
		return aferr(AFELINEAR);
	}

	afprintv(rq->verbose, 3, "Performing linearize process");
	if (linearize(&t) < 0)
		return -1;

	afprintv(rq->verbose, 3, "Closing database");
	if (linclose(&t) < 0)
		return -1;

	return 0;
}
