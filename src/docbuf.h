#ifndef _AF_DOCBUF_H
#define _AF_DOCBUF_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"

typedef struct {
	int eof; /* flag, turned on once all the data have been traversed */
	ssize_t index; /* index of next character to read */
	ssize_t data_len; /* length of data within buf */
	long buf_size; /* total capacity of buf */
	unsigned char* buf; /* input buffer */
	int filedes; /* file descriptor */
	ETYMON_AF_STAT st; /* stat structure */
	char* fn; /* input file name */
} ETYMON_DOCBUF;

unsigned char etymon_docbuf_next_char(ETYMON_DOCBUF* docbuf);

void etymon_docbuf_load_page(ETYMON_DOCBUF* docbuf);

unsigned char etymon_docbuf_next_char_peek(ETYMON_DOCBUF* docbuf);

unsigned char etymon_docbuf_next_char(ETYMON_DOCBUF* docbuf);

int etymon_docbuf_next_word(ETYMON_DOCBUF* docbuf, unsigned char* word);

#endif
