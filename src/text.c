/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include "text.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/* returns 0 if everything went OK */
int dc_text_init(ETYMON_AF_DC_INIT* dc_init) {
	return 0;
}


unsigned char dc_text_next_char(ETYMON_DOCBUF* docbuf,
				       etymon_af_off_t* offset) {
	(*offset)++;
	return etymon_docbuf_next_char(docbuf);
}


/* returns 0 if everything went OK */
int dc_text_index(ETYMON_AF_DC_INDEX* dc_index) {
	ETYMON_DOCBUF* docbuf = dc_index->docbuf;
	ETYMON_AF_INDEX_ADD_DOC add_doc;
	ETYMON_AF_INDEX_ADD_WORD add_word;
	unsigned char word[ETYMON_MAX_WORD_SIZE];
	Uint2 fields[ETYMON_MAX_FIELD_NEST];
	ETYMON_AF_DC_SPLIT* split_list = dc_index->split_list;
	ETYMON_AF_DC_SPLIT* split_p = split_list;
	unsigned char ch;
	int good;
	int x;
	etymon_af_off_t offset = 0;
	int long_words;

	long_words = dc_index->state->long_words;
	
	/* return if the document size is 0 */
	if (docbuf->data_len == 0) {
		return 0;
	}

	/* initialize variables */
	add_doc.key = NULL;
	add_doc.filename = docbuf->fn;
	add_doc.parent = 0;
	add_doc.dclass_id = dc_index->dclass_id;
	add_doc.state = dc_index->state;

	add_word.state = dc_index->state;

	add_doc.end = 0;
	
	while (split_p) {

		/* add document */
		add_doc.begin = add_doc.end;
		add_doc.end = split_p->end;
		add_word.doc_id = etymon_af_index_add_doc(&add_doc);
		
		/* parse out the words */
		add_word.word = word;
		add_word.fields = fields;
		add_word.word_number = 1;
		memset(fields, 0, ETYMON_MAX_FIELD_NEST * 2);
		while ( (docbuf->eof == 0) && (offset < add_doc.end) ) {

			ch = '\0';
			good = 0;
			
			/* loop past non alphanumeric chars */
			while ( (docbuf->eof == 0) && (offset < add_doc.end) &&
#ifdef UMLS
					(isspace(ch = dc_text_next_char(docbuf, &offset))) 
#else
					(isalnum(ch = dc_text_next_char(docbuf, &offset)) == 0) 
#endif
			) {
				}
			
			if ( (docbuf->eof == 0) && (offset < add_doc.end) ) {
				
				/* otherwise ch is the first char of the word */
				word[0] = tolower(ch);
				
				/* add the rest of the chars to the word */
				x = 1;
				while (
					(x < (ETYMON_MAX_WORD_SIZE - 1)) && (docbuf->eof == 0) &&
					(offset < add_doc.end) &&
					( ((good = isalnum(ch =
							   dc_text_next_char(docbuf, &offset))) != 0) ||
#ifdef UMLS
					  (good = (!isspace(ch))) ||
#endif
					  (good = (ch == '.')) ||
					  (good = (ch == '-')) )
					) {
					/* add ch to the word */
					word[x++] = tolower(ch);
				}
				
				/* iterate past any remaining chars (if the word was truncated because it was too long to fit in word[] */
				if (good != 0) {
					/* the char was good, so we either ran out of room or hit eof/eod */
					while (
						(docbuf->eof == 0) &&
						(offset < add_doc.end) &&
						( (isalnum(ch =
							   dc_text_next_char(docbuf, &offset)) != 0) ||
						  (ch == '.') ||
						  (ch == '-') )
						) {
					}
				}

				if ( (good) && (!long_words) )
					continue;
				
				/* truncate if last character is '.' */
				if (word[x - 1] == '.') {
					x--;
				}
				
				/* terminate the word[] string */
				word[x] = '\0';

/*				if (good)
				printf("Truncated: \"%s\"\n", word);*/
				/* let's try skipping if truncated */
/*				if (good)
					continue;
*/

/*				printf("\"%s\"\n", word); */
				
				if (etymon_af_index_add_word(&add_word) == -1) {
					return -1;
				}
				add_word.word_number++;
			}
		}

		/* next split */
		split_p = split_p->next;
		
	}
	
	return 0;
}
