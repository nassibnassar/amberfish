/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "xml_test.h"
#include "fdef.h"


/* returns 0 if everything went OK */
int dc_xml_test_init(ETYMON_AF_DC_INIT* dc_init) {
	return 0;
}


/* returns 0 if everything went OK */
int dc_xml_test_index(ETYMON_AF_DC_INDEX* dc_index) {
	ETYMON_DOCBUF* docbuf = dc_index->docbuf;
	ETYMON_AF_INDEX_ADD_DOC add_doc;
	ETYMON_AF_INDEX_ADD_WORD add_word;
	ETYMON_AF_FDEF_RESOLVE_FIELD resolve_field;
	unsigned char word[ETYMON_MAX_TOKEN_SIZE];
	unsigned char word_upper[ETYMON_MAX_TOKEN_SIZE];
	unsigned char word_last[ETYMON_MAX_TOKEN_SIZE];
	unsigned char element[ETYMON_MAX_TOKEN_SIZE];
	Uint2 fields_zero[ETYMON_MAX_FIELD_NEST];
	Uint2 fields[ETYMON_MAX_FIELD_NEST];
	int field_len;
	unsigned char ch;
	int token_alnum;
	int done;
	int x;
	int comment;
	int tag, last_tag;
	int attr;
	int special_tag;

	ch = 0;

	/* return if the document size is 0 */
	if (docbuf->data_len == 0) {
		return 0;
	}

	/* add the entire file as a single document */
	add_doc.key = NULL;
	add_doc.filename = docbuf->fn;
	add_doc.begin = 0;
	add_doc.end = docbuf->st.st_size;
	add_doc.parent = 0;
	add_doc.dclass_id = dc_index->dclass_id;
	add_doc.state = dc_index->state;

	add_word.doc_id = etymon_af_index_add_doc(&add_doc);
	add_word.word_number = 1;
	add_word.state = dc_index->state;

	resolve_field.state = dc_index->state;

	/* parse out the words */
	add_word.word = word_upper;
	word[0] = '\0';
	word_upper[0] = '\0';
	element[0] = '\0';
	memset(fields, 0, ETYMON_MAX_FIELD_NEST * 2);
	field_len = 0;
	memset(fields_zero, 0, ETYMON_MAX_FIELD_NEST * 2);

	comment = 0;
	tag = 0;
	last_tag = 0;
	attr = 0;
	special_tag = 0;
	
	while ( ! docbuf->eof ) {

		memcpy(word_last, word, ETYMON_MAX_TOKEN_SIZE);
		
		/* parse token */

		/* skip past whitespace */
		while ( (docbuf->eof == 0) && (isspace(ch = etymon_docbuf_next_char(docbuf)) != 0) ) {
		}

		if (docbuf->eof == 0) {
			
			/* check what type the new character is */
			token_alnum = isalnum(ch);

			/* make it the first character of a new token */
			if (token_alnum) {
				word_upper[0] = tolower(ch);
			} else {
				word_upper[0] = ch;
			}
			word[0] = ch;

			done = 0;
			x = 1;
			do {
				ch = etymon_docbuf_next_char_peek(docbuf);
				if (token_alnum) {
					if ( (ch != '.') &&
					     (ch != '-') &&
					     (ch != '_') &&
					     (ch != ':') &&
					     ( (isalnum(ch) == 0) || (isspace(ch) != 0) ) ) {
						done = 1;
					} else {
						if (x < (ETYMON_MAX_WORD_SIZE - 1)) {
							word_upper[x] = tolower(ch);
							word[x++] = ch;
						}
						etymon_docbuf_next_char(docbuf);
					}
				} else {
					/* token is non-alphanumeric */
					if ( (isalnum(ch) != 0) || (isspace(ch) != 0) ) {
						done = 1;
					}
					else if (ch == '<') {
						done = 1;
					}
					else if (word[0] == '>') {
						done = 1;
					}
					else if ( (word[0] == '/') && (word[1] == '>') ) {
						done = 1;
					}
					else if (word[0] == '=') {
						done = 1;
					}
					else if (word[0] == '?') {
						done = 1;
					}
					else if ( (word[0] == '\"') || (ch == '\"') ) {
						done = 1;
					}
					else if ( (word[0] == '\'') || (ch == '\'') ) {
						done = 1;
					}
					else if ( (word[0] == '<') && (word[1] == '\0') && (ch != '/') &&
						(ch != '?') && (ch != '!') ) {
						done = 1;
					} else {
						if (x < (ETYMON_MAX_WORD_SIZE - 1)) {
							word_upper[x] = ch;
							word[x++] = ch;
						}
						etymon_docbuf_next_char(docbuf);
					}
				}
			} while ( (done == 0) && (docbuf->eof == 0) );

			/* truncate if last character is '.' */
			if (word[x - 1] == '.') {
				x--;
			}
			
			word[x] = '\0';
			word_upper[x] = '\0';

			
			/* process token */

			if (isalnum(word[0])) {
				if ( (tag != 0) && (last_tag != 0) ) {
					/* token is an element name */
					resolve_field.word = word;
					x = etymon_af_fdef_resolve_field(&resolve_field);
					if (field_len >= ETYMON_MAX_FIELD_NEST) {
						/* ERROR, OVERFLOW! */
						fprintf(stderr, "ERROR: Field nesting overflow (element name)\n");
						exit(1);
					}
					fields[field_len++] = x;
					memcpy(element, word, ETYMON_MAX_TOKEN_SIZE);
					/* add attribute field */
					resolve_field.word = (unsigned char*)"_a";
					x = etymon_af_fdef_resolve_field(&resolve_field);
					if (field_len >= ETYMON_MAX_FIELD_NEST) {
						/* ERROR, OVERFLOW! */
						fprintf(stderr, "ERROR: Field nesting overflow (_a)\n");
						exit(1);
					}
					fields[field_len++] = x;
					
				} else {
					/* add the token as a indexable word */
					if (comment == 1) {
						add_word.fields = fields_zero;
					} else {
						add_word.fields = fields;
					}

					
					if (etymon_af_index_add_word(&add_word) == -1) {
						return -1;
					}
					add_word.word_number++;
				}
				if (last_tag) {
					last_tag = 0;
				}
			} else {

				if ( (comment == 0) && (strcmp((char*)word, "<") == 0) ) {
					last_tag = 1;
					tag = 1;
				}
				else if ( (comment == 0) && (special_tag == 0) && (tag == 1) && (strcmp((char*)word, ">") == 0) ) {
					tag = 0;
					/* remove attribute field */
					if (field_len == 0) {
						/* ERROR, UNDERFLOW! */
						fprintf(stderr, "ERROR: Field nesting underflow (>)\n");
						exit(1);
					}
					fields[--field_len] = 0;
					/* add element content field */
					strcpy((char*)word, "_c");
					resolve_field.word = word;
					x = etymon_af_fdef_resolve_field(&resolve_field);
					if (field_len >= ETYMON_MAX_FIELD_NEST) {
						/* ERROR, OVERFLOW! */
						fprintf(stderr, "ERROR: Field nesting overflow (>)\n");
						exit(1);
					}
					fields[field_len++] = x;
				}
				else if ( (comment == 0) && (strcmp((char*)word, "/>") == 0) ) {
					tag = 0;
					if (field_len == 0) {
						/* ERROR, UNDERFLOW! */
						fprintf(stderr, "ERROR: Field nesting underflow (/>)\n");
						exit(1);
					}
					fields[--field_len] = 0;
					fields[--field_len] = 0;
				}
				else if ( (comment == 0) && (strcmp((char*)word, "</") == 0) ) {
					if (field_len == 0) {
						/* ERROR, UNDERFLOW! */
						fprintf(stderr, "ERROR: Field nesting underflow (</)\n");
						exit(1);
					}
					fields[--field_len] = 0;
					fields[--field_len] = 0;
				}
				else if ( (comment == 0) && (tag == 1) && (strcmp((char*)word, "=") == 0) ) {
					attr = 1;
					/* last token was an attribute name */
					resolve_field.word = word_last;
					x = etymon_af_fdef_resolve_field(&resolve_field);
					if (field_len >= ETYMON_MAX_FIELD_NEST) {
						/* ERROR, OVERFLOW! */
						fprintf(stderr, "ERROR: Field nesting overflow (=)\n");
						exit(1);
					}
					fields[field_len++] = x;
				}
				else if ( (comment == 0) && (tag == 1) && (attr == 1) && (strcmp((char*)word, "\"") == 0) ) {
					attr = 10;
				}
				else if ( (comment == 0) && (tag == 1) && (attr == 1) && (strcmp((char*)word, "\'") == 0) ) {
					attr = 20;
				}
				else if ( (comment == 0) && (tag == 1) && (attr == 10) && (strcmp((char*)word, "\"") == 0) ) {
					attr = 0;
					/* remove attribute */
					if (field_len == 0) {
						/* ERROR, UNDERFLOW! */
						fprintf(stderr, "ERROR: Field nesting underflow\n");
						exit(1);
					}
					fields[--field_len] = 0;
				}
				else if ( (comment == 0) && (tag == 1) && (attr == 20) && (strcmp((char*)word, "\'") == 0) ) {
					attr = 0;
					/* remove attribute */
					if (field_len == 0) {
						/* ERROR, UNDERFLOW! */
						fprintf(stderr, "ERROR: Field nesting underflow\n");
						exit(1);
					}
					fields[--field_len] = 0;
				}
				else if ( (comment == 0) && (strcmp((char*)word, "<!--") == 0) ) {
					comment = 1;
				}
				else if (strcmp((char*)word, "-->") == 0) {
					comment = 0;
				}
				/* for now, treat <! and <? as comments */
				else if ( (comment == 0) && (special_tag == 0) && (strcmp((char*)word, "<!") == 0) ) {
					special_tag = 1;
				}
				else if ( (comment == 0) && (special_tag == 0) && (strcmp((char*)word, "<?") == 0) ) {
					special_tag = 1;
				}
				else if ( (comment == 0) && (special_tag == 1) && (strcmp((char*)word, ">") == 0) ) {
					special_tag = 0;
				}
				
			}

		}

	}
	return 0;
}
