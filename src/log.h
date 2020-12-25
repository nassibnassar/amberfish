#ifndef _AF_LOG_H
#define _AF_LOG_H

/* maximum char[] size for a diagnostic message */
#define ETYMON_AF_MAX_MSG_SIZE (1024)

#define EL_INFO 0
#define EL_WARNING 1
#define EL_ERROR 2
#define EL_CRITICAL 3

#define EX_MEMORY 1
#define EX_IO 2
#define EX_CREATE_READ_ONLY 10
#define EX_DB_OPEN_LIMIT 11
#define EX_DB_NAME_NULL 12
#define EX_DB_OPEN 13
#define EX_DB_CREATE 14
#define EX_DB_INCOMPATIBLE 15
#define EX_DB_ID_INVALID 16
#define EX_FIELD_UNKNOWN 17
#define EX_QUERY_TERM_TOO_LONG 18
#define EX_QUERY_TOO_COMPLEX 19
#define EX_QUERY_SYNTAX_ERROR 20
#define EX_DB_NOT_READY 21

static const char* etymon_af_ex[] = {
	"",
	"Out of memory", /* EX_MEMORY */
	"File error", /* EX_IO */
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"Create not allowed in read-only mode", /* EX_CREATE_READ_ONLY */
	"Too many open databases", /* EX_DB_OPEN_LIMIT */
	"Database name is NULL", /* EX_DB_NAME_NULL */
	"Unable to open database", /* EX_DB_OPEN */
	"Unable to create database", /* EX_DB_CREATE */
	"Database incompatible with this version", /* EX_DB_INCOMPATIBLE */
	"Invalid database identifier", /* EX_DB_ID_INVALID */
	"Unrecognized field name", /* EX_FIELD_UNKNOWN */
	"Maximum query term length exceeded", /* EX_QUERY_TERM_TOO_LONG */
	"Query is too deeply nested", /* EX_QUERY_TOO_COMPLEX */
	"Syntax error in query", /* EX_QUERY_SYNTAX_ERROR */
	"Database not ready", /* EX_DB_NOT_READY */
	""
};

typedef struct {
	int level; /* 0=informational, 1=warning, 2=error, 3=critical error (memory) */
	int code; /* specific error code */
	char msg[ETYMON_AF_MAX_MSG_SIZE]; /* diagnostic message */
	char where[ETYMON_AF_MAX_MSG_SIZE]; /* function where the exception occurred */
} ETYMON_AF_EXCEPTION;

typedef struct {
	void (*write)(const ETYMON_AF_EXCEPTION*);
	ETYMON_AF_EXCEPTION ex;
} ETYMON_AF_LOG;

void etymon_af_log(ETYMON_AF_LOG* log, int level, int code,
		   char* where, char* file, char* extfile,
		   char* extmsg);

#endif
