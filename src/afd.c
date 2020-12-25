#include "soapH.h"
#include "open.h"
#include "search.h"

struct Namespace namespaces[] = {
	{ "SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/" },
	{ "SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/" },
	{ "xsi", "http://www.w3.org/2001/XMLSchema-instance", "http://wwww.w3.org/*/XMLSchema-instance" },
	{ "xsd", "http://www.w3.org/2001/XMLSchema", "http://www.w3.org/*/XMLSchema" },
	{ "SRW", "http://www.loc.gov/zing/srw/" },
	{ NULL, NULL }
};

int SRW__test(struct soap *soap, char **s)
{
	*s = (char *) soap_strdup(soap, "Welcome to Amberfish.");
	return SOAP_OK;
}

typedef struct {
	int score;
	char dbname[AFPATHSIZE];
	int db_id;
	int doc_id;
	int parent;  /* doc_id of parent document */
        char filename[AFPATHSIZE]; /* document source file name */
	etymon_af_off_t begin; /* starting offset of document within the file */
        etymon_af_off_t end; /* ending offset of document within the file (one byte past end) */
} AFSEARCH_RESULT;

typedef struct AFSEARCH_RLIST_STRUCT {
	AFSEARCH_RESULT r;
	struct AFSEARCH_RLIST_STRUCT* next;
} AFSEARCH_RLIST;

typedef struct AFSEARCH_RTREE_STRUCT {
	AFSEARCH_RESULT r;
	struct AFSEARCH_RTREE_STRUCT* next;
	struct AFSEARCH_RTREE_STRUCT* child;
} AFSEARCH_RTREE;

void ses_spresult(char *s, AFSEARCH_RESULT *res)
{
	sprintf(s, "%i %s %i %i %s %ld %ld",
	       res->score / 100,
	       res->dbname,
	       res->doc_id,
	       res->parent,
	       res->filename,
	       (long)(res->begin),
	       (long)(res->end));
}

int SRW__searchRetrieveRequest(struct soap *soap, 
			       char *SRW__version,
			       char *SRW__query,
			       struct SRW__searchRetrieveResponse *srs)
{
	Afopen op;
        Afopen_r opr;
        Afsearch se;
        Afsearch_r ser;
        Afclose cl;
        Afclose_r clr;
	ETYMON_AF_LOG log;
        Uint2 dbid[ETYMON_AF_MAX_OPEN];
        int dbidn;
        int x;
        Afresultmd* resultmd;
        char** p;
/*        AFSEARCH_RESULT* res; */
        int res_n;
        int r;
	AFSEARCH_RESULT* res;
	int search_numhits = -1;
	int search_skiphits = 0;

	char *dbname[1];
/*	char *dbname = soap->path + 1; */
	int dbname_n = 1;

	dbname[0] = soap->path + 1;
	
/*	printf("Received query `%s' for database `%s'\n", SRW__query,
 * dbname[0]); */

	op.mode = "r";
        for (x = 0; x < dbname_n; x++) {
                op.dbpath = dbname[x];
                if (afopen(&op, &opr) < 0) {
/*                        return searcherr();*/
		}
                dbid[x] = opr.dbid;
        }
        dbidn = x;
        
        se.dbid = dbid;
        se.dbidn = dbidn;
        se.query = (Afchar *) SRW__query;
        se.qtype = AFQUERYBOOLEAN;
        se.score = AFSCOREDEFAULT;
        
        r = afsearch(&se, &ser);
        afsortscore(ser.result, ser.resultn);

        if (r != -1 && search_numhits) {

		srs->SRW__records.__ptrSRW__record = (struct SRWRecord *) soap_malloc(
			soap, ser.resultn * sizeof (struct SRWRecord));

                if (search_skiphits) {
                        if (search_skiphits < ser.resultn) {
                                size_t movebytes = (ser.resultn - search_skiphits) *
                                        sizeof (Afresult);
                                memmove(ser.result,
                                        ser.result + search_skiphits,
                                        movebytes);
                                ser.result = (Afresult *) realloc(
                                        ser.result,
                                        movebytes);
				                                ser.resultn = ser.resultn - search_skiphits;
                        } else {
                                ser.result = (Afresult *) realloc(
                                        ser.result, sizeof (Afresult));
                                ser.resultn = 0;
                        }
                }
                
                if (search_numhits > 0 && ser.resultn > search_numhits) {
                        ser.resultn = search_numhits;
                        ser.result = (Afresult *) realloc(
                                ser.result,
                                search_numhits * sizeof (Afresult));
                }
                
                res_n = ser.resultn;
                resultmd = (Afresultmd*)(malloc((res_n + 1) * sizeof(Afresultmd)));
                res = (AFSEARCH_RESULT*)(malloc((res_n + 1) * sizeof(AFSEARCH_RESULT)));
                if ( (!resultmd) || (!res) ) {
                        fprintf(stderr, "af: unable to allocate memory for search results\n");
                } else {
                        if (afgetresultmd(ser.result,
                                                      res_n,
                                                      resultmd) != -1) {
                                for (x = 0; x < res_n; x++) {
                                        res[x].score = ser.result[x].score;
                                        strcpy(res[x].dbname,
                                               dbname[ser.result[x].dbid - 1]);
                                        res[x].db_id =
                                                ser.result[x].dbid;
                                        res[x].doc_id =
                                                ser.result[x].docid;
                                        res[x].parent =
                                                resultmd[x].parent;
                                        memcpy(res[x].filename, resultmd[x].docpath, AFPATHSIZE);
                                        res[x].begin = resultmd[x].begin;
                                        res[x].end = resultmd[x].end;

					srs->SRW__records.__ptrSRW__record[x].SRW__recordData = 
						(char *) soap_malloc(soap, 1024);
					ses_spresult(srs->SRW__records.__ptrSRW__record[x].SRW__recordData,
						     res + x);
                                }
                        }

                        free(resultmd);
                }

                if (ser.result) {
                        free(ser.result);
                }
                free(res);
        }
	
        for (x = 0; x < dbidn; x++) {
                cl.dbid = dbid[x];
                afclose(&cl, &clr);
        }

	srs->SRW__version = "1.1";
	srs->SRW__numberOfRecords = ser.resultn;
	srs->SRW__records.__size = ser.resultn;

/*	printf("%i results.\n", srs->SRW__numberOfRecords); */
	return SOAP_OK;
}

int http_get(struct soap *soap)
{
	char *s = strchr(soap->path, '?');
	if (s && !strcmp(s, "?test")) {
		soap_response(soap, SOAP_HTML);
		soap_send(soap, "<HTML>You called test().</HTML>");
		soap_end_send(soap);
		return SOAP_OK;
	}
	
	soap_response(soap, SOAP_HTML);
	soap_send(soap, "<HTML>success - afd is running</HTML>");
	soap_end_send(soap);
	return SOAP_OK;
}

int afdmain(int argc, char **argv)
{
	struct soap *soap;
	int child_socket;

	soap = soap_new();
	soap->fget = http_get;
	if (soap_bind(soap, NULL, 8080, 100) < 0) {
		soap_print_fault(soap, stderr);
		return -1;
	}
	printf("afd started\n");
	while (1) {
		if ((child_socket = soap_accept(soap)) < 0) {
			soap_print_fault(soap, stderr);
			return -1;
		}
		soap_serve(soap);
		soap_destroy(soap);
		soap_end(soap);
	}
}
