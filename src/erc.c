/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include "erc.h"
#include "fdef.h"
#include "util.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>


/* returns 0 if everything went OK */
int dc_erc_init(ETYMON_AF_DC_INIT* dc_init) {
	return 0;
}


unsigned char dc_erc_next_char(ETYMON_DOCBUF* docbuf,
				       etymon_af_off_t* offset) {
	(*offset)++;
	return etymon_docbuf_next_char(docbuf);
}


/* returns 0 if everything went OK */
int dc_erc_index(ETYMON_AF_DC_INDEX* dc_index) {
	ETYMON_DOCBUF* docbuf = dc_index->docbuf;
	ETYMON_AF_INDEX_ADD_DOC add_doc;
	ETYMON_AF_INDEX_ADD_WORD add_word;
	unsigned char word[ETYMON_MAX_WORD_SIZE];
	Uint2 fields[ETYMON_MAX_FIELD_NEST];
	ETYMON_AF_DC_SPLIT* split_list = dc_index->split_list;
	ETYMON_AF_DC_SPLIT* split_p = split_list;
	unsigned char ch;
	unsigned char old_ch;
	int good;
	int x;
	etymon_af_off_t offset = 0;
	ETYMON_AF_FDEF_RESOLVE_FIELD resolve_field;
	
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

	add_word.word = word;
	add_word.fields = fields;
	memset(fields, 0, ETYMON_MAX_FIELD_NEST * 2);
	add_word.state = dc_index->state;

	resolve_field.word = word;
	resolve_field.state = dc_index->state;
	
	add_doc.end = 0;
	
	while (split_p) {

		/* add document */
		add_doc.begin = add_doc.end;
		add_doc.end = split_p->end;
		add_word.doc_id = etymon_af_index_add_doc(&add_doc);
		
		/* parse out the words */
		add_word.word_number = 1;
		fields[0] = 0;
		old_ch = '\n';
		while ( (docbuf->eof == 0) && (offset < add_doc.end) ) {

			ch = '\0';
			good = 0;

			/* loop past non alphanumeric chars */
			while ( (docbuf->eof == 0) && (offset < add_doc.end) && (isalnum(ch =
							       dc_erc_next_char(docbuf, &offset)) == 0) ) {
				old_ch = ch;
			}
			
			if ( (docbuf->eof == 0) && (offset < add_doc.end) ) {
				
				/* otherwise ch is the first char of the word */
				word[0] = ch;
				
				/* add the rest of the chars to the word */
				x = 1;
				while (
					(x < (ETYMON_MAX_WORD_SIZE - 1)) && (docbuf->eof == 0) &&
					(offset < add_doc.end) &&
					( ((good = isalnum(ch =
							   dc_erc_next_char(docbuf, &offset))) != 0) ||
					  (good = (ch == '.')) ||
					  (good = (ch == '-')) )
					) {
					/* add ch to the word */
					word[x++] = ch;
				}
				
				/* iterate past any remaining chars (if the word was truncated because it was too long to fit in word[] */
				if (good != 0) {
					/* the char was good, so we either ran out of room or hit eof/eod */
					while (
						(docbuf->eof == 0) &&
						(offset < add_doc.end) &&
						( (isalnum(ch =
							   dc_erc_next_char(docbuf, &offset)) != 0) ||
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

				/* determine if the word is a field
				   name or an indexable word */
				if (old_ch == '\n') {
					/* field name */
					x = etymon_af_fdef_resolve_field(&resolve_field);
					fields[0] = x;
				} else {
					/* indexable word */
					etymon_tolower((char*)word);
					if (etymon_af_index_add_word(&add_word) == -1) {
						return -1;
					}
					add_word.word_number++;
				}

				old_ch = ch;
			}
		}

		/* next split */
		split_p = split_p->next;
		
	}
	
	return 0;
}
