/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include "linbuf.h"
#include "util.h"

#define LINBUFSEGSIZE ((size_t) 1000000000)

static size_t bufsize;
static FILE *fbuf;
static unsigned char **linbuf;
static size_t *linbuflen;
static int linbufn;

static int createbuf()
{
	int x;
	size_t z;

	linbufn = (bufsize / LINBUFSEGSIZE) + 1;
	linbuf = (unsigned char **) malloc(linbufn * sizeof (unsigned char *));
	if (!linbuf)
		return aferr(AFEMEM);
	linbuflen = (size_t *) malloc(linbufn * sizeof (size_t));
	if (!linbuflen)
		return aferr(AFEMEM);
	for (z = bufsize, x = 0; z > 0; z -= LINBUFSEGSIZE, x++) {
		linbuf[x] = (unsigned char *) malloc( linbuflen[x] = (
			z < LINBUFSEGSIZE ? z : LINBUFSEGSIZE) );
		if (!linbuf[x])
			return aferr(AFEMEM);
		if (z < LINBUFSEGSIZE)
			break;
	}
	
	return 0;
}

static int fillbuf()
{
	int x;

	if (fseeko(fbuf, 0, SEEK_SET) < 0)
                return aferr(AFEDBIO);
	for (x = 0; x < linbufn; x++) {
		if (fread(linbuf[x], 1, linbuflen[x], fbuf) < linbuflen[x])
			return aferr(AFEDBIO);
	}
	
	return 0;
}

int aflinbuf(FILE *f, int mem)
{
	off_t fsize;

	afgetfsize(f, &fsize);
	bufsize = ((size_t) mem) * 1048576;
/*	printf("aflinbuf: %i %lu %lu\n", mem, (unsigned long) fsize,
	(unsigned long) bufsize);*/
	bufsize = fsize < bufsize ? fsize : bufsize;
	fbuf = f;
	if (createbuf() < 0)
		return -1;
	if (fillbuf() < 0)
		return -1;
	
	return 0;
}

void bufcpy(unsigned char *ptr, off_t offset, size_t size)
{
	int x;
	int b, bx;

	b = offset / LINBUFSEGSIZE;
	bx = offset % LINBUFSEGSIZE;
	for (x = 0; x < size; x++) {
		ptr[x] = linbuf[b][bx];
		if (++bx >= linbuflen[b]) {
			b++;
			bx = 0;
		}
	}
}

int aflinread(void *ptr, off_t offset, size_t size)
{
	if ((offset + size) <= bufsize) {
/*		printf("."); fflush(stdout);*/
		bufcpy((unsigned char *)ptr, offset, size);
	} else {
/*		printf("\n%lu %lu %lu\n", (unsigned long) offset,
		(unsigned long) size,
		(unsigned long) bufsize);*/
/*		printf("*"); fflush(stdout);*/
		fseeko(fbuf, offset, SEEK_SET);
		if (fread(ptr, 1, size, fbuf) < size)
			return aferr(AFEDBIO);
	}

	return 0;
}
