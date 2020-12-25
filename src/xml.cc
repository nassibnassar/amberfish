/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

extern "C" {
#include "config.h"
#include "docbuf.h"
#include "index.h"
#include "fdef.h"
}

#include "xml.h"

#ifdef ETYMON_AF_XML

#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/sax/SAXException.hpp>
#include <xercesc/sax/AttributeList.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/parsers/SAXParser.hpp>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

using namespace std;
using namespace xercesc;

#undef BELIST_USEMEM

typedef struct DC_XML_BSTACK_STRUCT {
	int elemid;
	unsigned int begin;
	DC_XML_BSTACK_STRUCT* next;
} DC_XML_BSTACK;

typedef struct DC_XML_BELIST_STRUCT {
	int elemid;
	int parent;
	unsigned int begin;
	unsigned int end;
#ifdef BELIST_USEMEM
	DC_XML_BELIST_STRUCT* next;
#endif
} DC_XML_BELIST;


/* class AttributeList; */

class DcXmlHandlers : public HandlerBase {
 public:
	DcXmlHandlers();
	void init(ETYMON_AF_DC_INDEX* dc_index);
	void finish();
	~DcXmlHandlers();

	bool getSawErrors() const {
		return fSawErrors;
	}

	void startElement(const XMLCh* const name, AttributeList& attributes);
	void endElement(const XMLCh* const name);
	void characters(const XMLCh* const chars, const unsigned int length);

	void warning(const SAXParseException& exception);
	void error(const SAXParseException& exception);
	void fatalError(const SAXParseException& exception);
	void resetErrors();

	void setParser(SAXParser* parser);
	
 private:
	void addWords(char* text);
	unsigned int getSrcOffset();
	unsigned int getPrevSrcOffset();
	
	bool            fSawErrors;
	
	SAXParser* _parser;
	unsigned int _prev_offset;
	DC_XML_BSTACK* _bstack_head;
	int _bstack_n;
#ifdef BELIST_USEMEM
	DC_XML_BELIST* _belist_head;
	DC_XML_BELIST* _belist_tail;
#else
	char _belist_fn[ETYMON_MAX_PATH_SIZE];  /* temporary until we integrate this with
						   the built-in file types */
	FILE* _belist_fp;
	DC_XML_BELIST _belist_node;
#endif	
	ETYMON_AF_DC_INDEX* _dc_index;
	ETYMON_AF_INDEX_ADD_DOC _add_doc;
	int _elem_n;
	int _max_doc_res;  /* maximum number of levels to descend for
			      purposes of defining document boundaries */
	int _doc_res_overflow;  /* how many levels over maximum we
				   have descended */
	Uint4 _next_doc_id;
	ETYMON_AF_INDEX_ADD_WORD _add_word;
	ETYMON_AF_FDEF_RESOLVE_FIELD _resolve_field;
	Uint2 _fields[ETYMON_MAX_FIELD_NEST];
	int _field_len;
};

/* returns 0 if everything went OK */
int dc_xml_init(ETYMON_AF_DC_INIT* dc_init) {
	dc_init->use_docbuf = 0;  /* disable docbuf */
	return 0;
}


unsigned char dc_xml_next_char(ETYMON_DOCBUF* docbuf,
				       etymon_af_off_t* offset) {
	(*offset)++;
	return etymon_docbuf_next_char(docbuf);
}


/* returns 0 if everything went OK */
int dc_xml_index(ETYMON_AF_DC_INDEX* dc_index) {
	try {
		XMLPlatformUtils::Initialize();
	}
	catch (const XMLException& toCatch)
	{
		cerr << "Error during initialization! Message:\n"
		     << XMLString::transcode(toCatch.getMessage()) << endl;
		return 1;
	}

	SAXParser* parser = new SAXParser;
	parser->setValidationScheme(SAXParser::Val_Never);
	parser->setDoNamespaces(false);
	parser->setDoSchema(false);
	parser->setValidationSchemaFullChecking(false);
	parser->setCalculateSrcOfs(true);
	
	DcXmlHandlers handler;
	handler.init(dc_index);
	handler.setParser(parser);
	parser->setDocumentHandler(&handler);
	parser->setErrorHandler(&handler);

        try {
		parser->parse(dc_index->filename);
		handler.finish();
        }
        catch (const XMLException& e) {
		printf("Error: %s\n", XMLString::transcode(e.getMessage()));
        }
        catch (...) {
		printf("Unexpected exception during parsing\n");
        }

        if (handler.getSawErrors()) {
		return -1;
	} else {
		return 0;
	}
}


/* DcXmlHandlers */

DcXmlHandlers::DcXmlHandlers() : fSawErrors(false) {
	_parser = 0;
	_prev_offset = 0;
	_dc_index = 0;
	_bstack_head = 0;
	_bstack_n = 0;
#ifdef BELIST_USEMEM
	_belist_head = 0;
	_belist_tail = 0;
#endif
	_elem_n = 0;
	_doc_res_overflow = 0;
}

void DcXmlHandlers::init(ETYMON_AF_DC_INDEX* dc_index) {
	_add_doc.key = NULL;
	_add_doc.dclass_id = dc_index->dclass_id;
	_add_doc.state = dc_index->state;
	_add_word.word_number = 1;
	_add_word.fields = _fields;
	_add_word.state = dc_index->state;
	_resolve_field.state = dc_index->state;
        memset(_fields, 0, ETYMON_MAX_FIELD_NEST * 2);
	_field_len = 0;
	_next_doc_id =
		etymon_index_dclass_get_next_doc_id(dc_index->state);
	_max_doc_res = dc_index->dlevel;
	_dc_index = dc_index;
#ifndef BELIST_USEMEM
	// temporary file name construction until we integrate this
	// with the built-in file types
	strncpy(_belist_fn, dc_index->state->dbname,
		ETYMON_MAX_PATH_SIZE - 1);
	_belist_fn[ETYMON_MAX_PATH_SIZE - 1] = '\0';
	int leftover = ETYMON_MAX_PATH_SIZE - strlen(_belist_fn) - 1;
	strncat(_belist_fn, ".xm", leftover);
	_belist_fp = fopen(_belist_fn, "w+");
#endif
}

DcXmlHandlers::~DcXmlHandlers() {
}

void DcXmlHandlers::addWords(char* text) {
	unsigned char word[ETYMON_MAX_WORD_SIZE];
	char* p = text;
	int good;
	int x;
	int long_words;

	long_words = _dc_index->state->long_words;
	
	while (*p != '\0') {

		good = 0;
		
		// skip past non alphanumeric chars
		while ( (*p != '\0') && (isalnum(*p) == 0) ) {
			p++;
		}

		if (*p == '\0') {
			break;
		}

		// *p is the first char of the word
		word[0] = tolower(*p);
		p++;

		// add the rest of the chars to the word
		x = 1;
		while (
			(x < (ETYMON_MAX_WORD_SIZE - 1)) && (*p != '\0') &&
			( ((good = isalnum(*p)) != 0) ||
			  (good = (*p == '.')) ||
			  (good = (*p == '-')) )
			) {
			// add ch to the word
			word[x++] = tolower(*p);
			p++;
		}
		
		// iterate past any remaining chars (if the word was truncated because it was too long to fit in word[]
		if (good != 0) {
			// the char was good, so we either ran out of
			// room or hit end of string
			while (
				(*p != '\0') &&
				( (isalnum(*p) != 0) ||
				  (*p == '.') ||
				  (*p == '-') )
				) {
				p++;
			}
		}

		if ( (good) && (!long_words) )
			continue;

		// truncate if last character is '.'
		if (word[x - 1] == '.') {
			x--;
		}
		
		// terminate the word[] string
		word[x] = '\0';

		_add_word.word = word;
		_add_word.doc_id = _bstack_head->elemid + _next_doc_id;
		if (etymon_af_index_add_word(&_add_word) ==
		    -1) {
			fprintf(stderr, "ERROR: add_word returned -1\n");
			exit(1);
		}
		_add_word.word_number++;
		
	}
}

void DcXmlHandlers::startElement(const XMLCh* const name,
				  AttributeList& attributes) {

	if (_bstack_n < _max_doc_res) {
		// push onto bstack
		DC_XML_BSTACK* bp =
			(DC_XML_BSTACK*)(malloc(sizeof(DC_XML_BSTACK)));
		if (!bp) exit(-1);
		bp->elemid = _elem_n++;
		bp->begin =getPrevSrcOffset();
		bp->next = _bstack_head;
		_bstack_head = bp;
		_bstack_n++;
	} else {
		_doc_res_overflow++;
	}

	// add element field
	char* s = XMLString::transcode(name);
	if (strlen(s) >= ETYMON_MAX_TOKEN_SIZE ) s[ETYMON_MAX_TOKEN_SIZE - 1] = '\0';
	_resolve_field.word = (unsigned char*)s;
	int x = etymon_af_fdef_resolve_field(&_resolve_field);
	if (_field_len >= ETYMON_MAX_FIELD_NEST) {
		/* ERROR, OVERFLOW! */
		fprintf(stderr, "ERROR: Field nesting overflow (element name)\n");
		exit(1);
	}
	_fields[_field_len++] = x;
	free(s);

	// add _a field
	_resolve_field.word = (unsigned char*)"_a";
	x = etymon_af_fdef_resolve_field(&_resolve_field);
	if (_field_len >= ETYMON_MAX_FIELD_NEST) {
		/* ERROR, OVERFLOW! */
		fprintf(stderr, "ERROR: Field nesting overflow (_a)\n");
		exit(1);
	}
	_fields[_field_len++] = x;

	// add attribute fields and index words
	for (unsigned int u = 0; u < attributes.getLength(); u++) {

		// add attribute field
		s = XMLString::transcode(attributes.getName(u));
		if (strlen(s) >= ETYMON_MAX_TOKEN_SIZE ) s[ETYMON_MAX_TOKEN_SIZE - 1] = '\0';
		_resolve_field.word = (unsigned char*)s;
		x = etymon_af_fdef_resolve_field(&_resolve_field);
		if (_field_len >= ETYMON_MAX_FIELD_NEST) {
			/* ERROR, OVERFLOW! */
			fprintf(stderr, "ERROR: Field nesting overflow (attribute name)\n");
			exit(1);
		}
		_fields[_field_len] = x;
		free(s);

		// add words from attribute value
		s = XMLString::transcode(attributes.getValue(u));
		addWords(s);
		free(s);
		
	}
	_fields[_field_len] = 0;
	_fields[--_field_len] = 0;
	
	// add _c field
	_resolve_field.word = (unsigned char*)"_c";
	x = etymon_af_fdef_resolve_field(&_resolve_field);
	if (_field_len >= ETYMON_MAX_FIELD_NEST) {
		/* ERROR, OVERFLOW! */
		fprintf(stderr, "ERROR: Field nesting overflow (_c)\n");
		exit(1);
	}
	_fields[_field_len++] = x;

}

void DcXmlHandlers::endElement(const XMLCh* const name) {

	if (_doc_res_overflow) {
		_doc_res_overflow--;
	} else {
		// pop from bstack
		DC_XML_BSTACK* bp = _bstack_head;
		_bstack_head = bp->next;
		_bstack_n--;

		// add to belist
#ifdef BELIST_USEMEM
		DC_XML_BELIST* bep =
			(DC_XML_BELIST*)(malloc(sizeof(DC_XML_BELIST)));
		if (!bep) exit(-1);
		bep->elemid = bp->elemid;
		if (_bstack_head) bep->parent = _bstack_head->elemid + _next_doc_id;
		else bep->parent = 0;
		bep->begin = bp->begin;
		bep->end = getSrcOffset();
		bep->next = 0;
		if (_belist_head) _belist_tail->next = bep;
		else _belist_head = bep;
		_belist_tail = bep;
#else
		_belist_node.elemid = bp->elemid;
		if (_bstack_head) _belist_node.parent = _bstack_head->elemid + _next_doc_id;
		else _belist_node.parent = 0;
		_belist_node.begin = bp->begin;
		_belist_node.end = getSrcOffset();
		fwrite(&_belist_node, 1, sizeof(DC_XML_BELIST), _belist_fp);
#endif
		
		free(bp);
	}

	// remove fields
	if (_field_len < 2) {
		/* ERROR, UNDERFLOW! */
		fprintf(stderr, "ERROR: Field nesting underflow (end element)\n");
		exit(1);
	}
	_fields[--_field_len] = 0;
	_fields[--_field_len] = 0;

}

void DcXmlHandlers::characters(const XMLCh* const chars, const
				unsigned int length) {

	// add words from element content
	char* s = (char*)(malloc(length + 1));
	XMLString::transcode(chars, s, length);
	s[length] = '\0';
	addWords(s);
	free(s);

}

void DcXmlHandlers::error(const SAXParseException& e) {
	fSawErrors = true;
	cerr << "\nError at line " << e.getLineNumber()
	     << ", char " << e.getColumnNumber()
	     << "\n  Message: " << XMLString::transcode(e.getMessage()) << endl;
}

void DcXmlHandlers::fatalError(const SAXParseException& e) {
	fSawErrors = true;
	cerr << "\nFatal Error at line " << e.getLineNumber()
	     << ", char " << e.getColumnNumber()
	     << "\n  Message: " << XMLString::transcode(e.getMessage()) << endl;
}

void DcXmlHandlers::warning(const SAXParseException& e) {
	cerr << "\nWarning at line " << e.getLineNumber()
	     << ", char " << e.getColumnNumber()
	     << "\n  Message: " << XMLString::transcode(e.getMessage()) << endl;
}

void DcXmlHandlers::resetErrors() {
    fSawErrors = false;
}

void DcXmlHandlers::setParser(SAXParser* parser) {
	_parser = parser;
}

unsigned int DcXmlHandlers::getSrcOffset() {
	_prev_offset = _parser->getSrcOffset();
	return _prev_offset;
}

unsigned int DcXmlHandlers::getPrevSrcOffset() {
	unsigned int prev_offset = _prev_offset;
	_prev_offset = _parser->getSrcOffset();
	return prev_offset;
}

typedef struct {
	int parent;
	unsigned int begin;
	unsigned int end;
} DC_XML_BE;

void DcXmlHandlers::finish() {
	DC_XML_BE* be = (DC_XML_BE*)(malloc(sizeof(DC_XML_BE) *
					    _elem_n));
#ifdef BELIST_USEMEM
	DC_XML_BELIST* bep = _belist_head;
	DC_XML_BELIST* obep;
	while (bep) {
		be[bep->elemid].parent = bep->parent;
		be[bep->elemid].begin = bep->begin;
		be[bep->elemid].end = bep->end;
		obep = bep;
		bep = bep->next;
		free(obep);
	}
	_belist_head = 0;
	_belist_tail = 0;
#else
	rewind(_belist_fp);
	while (fread(&_belist_node, 1, sizeof(DC_XML_BELIST),
		     _belist_fp) == sizeof(DC_XML_BELIST)) {
		be[_belist_node.elemid].parent = _belist_node.parent;
		be[_belist_node.elemid].begin = _belist_node.begin;
		be[_belist_node.elemid].end = _belist_node.end;
	}
	fclose(_belist_fp);
	unlink(_belist_fn);
#endif

	ETYMON_AF_INDEX_ADD_DOC add_doc;
	add_doc.key = NULL;
	add_doc.filename = _dc_index->filename;
	add_doc.dclass_id = _dc_index->dclass_id;
	add_doc.state = _dc_index->state;
	for (int x = 0; x < _elem_n; x++) {
		add_doc.parent = (Uint4)(be[x].parent);
		add_doc.begin = (etymon_af_off_t)(be[x].begin);
		add_doc.end = (etymon_af_off_t)(be[x].end);
		etymon_af_index_add_doc(&add_doc);
	}

	free(be);
}

#endif
