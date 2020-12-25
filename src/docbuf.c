/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include "docbuf.h"
#include "defs.h"

void etymon_docbuf_load_page(ETYMON_DOCBUF* docbuf) {
	docbuf->data_len = read(docbuf->filedes,
				docbuf->buf,
				docbuf->buf_size);
	if (docbuf->data_len == -1) {
		perror("docbuf_load_page():read()");
	}
	if (docbuf->data_len == 0) {
		docbuf->eof = 1;
	} else {
		docbuf->index = 0;
	}
}


unsigned char etymon_docbuf_next_char_peek(ETYMON_DOCBUF* docbuf) {
	/* return if we are at the end */
	if (docbuf->eof) {
		return '\0';
	}

	/* otherwise the next char is in docbuf->buf */
	return docbuf->buf[docbuf->index];
}


unsigned char etymon_docbuf_next_char(ETYMON_DOCBUF* docbuf) {
	unsigned char ch;
	
	ch = etymon_docbuf_next_char_peek(docbuf);

	/* now increment and load the next page if necessary */
	docbuf->index++;
	if (docbuf->index >= docbuf->data_len) {
		etymon_docbuf_load_page(docbuf);
	}

	return ch;
}


/* assumes that word is of size ETYMON_MAX_WORD_SIZE */
/* returns 1 if a word was found and put into word[] */
int etymon_docbuf_next_word(ETYMON_DOCBUF* docbuf, unsigned char* word) {
	unsigned char ch = '\0';
	int good = 0;
	int x;

	/* loop past non alphanumeric chars */
	while ( (docbuf->eof == 0) && (isalnum(ch = etymon_docbuf_next_char(docbuf)) == 0) ) {
	}

	/* return if we hit eof */
	if (docbuf->eof) {
		return 0;
	}
	
	/* otherwise ch is the first char of the word */
	word[0] = ch;

	/* add the rest of the chars to the word */
	x = 1;
	while (
		(x < (ETYMON_MAX_WORD_SIZE - 1)) && (docbuf->eof == 0) &&
		( ((good = isalnum(ch = etymon_docbuf_next_char(docbuf))) != 0) ||
		  (good = (ch == '.')) ||
		  (good = (ch == '-')) )
		) {
		/* add ch to the word */
		word[x++] = ch;
	}

	/* iterate past any remaining chars (if the word was truncated because it was too long to fit in word[] */
	if (good != 0) {
		/* the char was good, so we either ran out of room or hit eof */
		while (
			(docbuf->eof == 0) &&
			( (isalnum(ch = etymon_docbuf_next_char(docbuf)) != 0) ||
			  (ch == '.') ||
			  (ch == '-') )
			) {
		}
	}

	/* truncate if last character is '.' */
	if (word[x - 1] == '.') {
		x--;
	}
	
	/* terminate the word[] string */
	word[x] = '\0';

	return 1;
}
