/*
 *  Copyright (C) 1999-2004 Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <string.h>
#include "log.h"

void etymon_af_log(ETYMON_AF_LOG* log, int level, int code, char* where, char* file, char* extfile, char* extmsg) {

	if (!log) {
		return;
	}
	
	/* fill in codes */
	log->ex.level = level;
	log->ex.code = code;

	/* fill in ex.where */
	if (where) {
		strncpy(log->ex.where, where, ETYMON_AF_MAX_MSG_SIZE - 1);
		log->ex.where[ETYMON_AF_MAX_MSG_SIZE - 1] = '\0';
	} else {
		log->ex.where[0] = '\0';
	}

	/* fill in ex.msg */
	
	if (file) {
		strncpy(log->ex.msg, file, ETYMON_AF_MAX_MSG_SIZE - 1);
		log->ex.msg[ETYMON_AF_MAX_MSG_SIZE - 1] = '\0';
		strncat(log->ex.msg, ": ", ETYMON_AF_MAX_MSG_SIZE - strlen(log->ex.msg) - 1);
	} else {
		log->ex.msg[0] = '\0';
	}

	if (level == EL_WARNING) {
		strncat(log->ex.msg, "warning: ", ETYMON_AF_MAX_MSG_SIZE - strlen(log->ex.msg) - 1);
	}
	
	strncat(log->ex.msg, etymon_af_ex[code], ETYMON_AF_MAX_MSG_SIZE - strlen(log->ex.msg) - 1);

	if (extfile) {
		strncat(log->ex.msg, ": ", ETYMON_AF_MAX_MSG_SIZE - strlen(log->ex.msg) - 1);
		strncat(log->ex.msg, extfile, ETYMON_AF_MAX_MSG_SIZE - strlen(log->ex.msg) - 1);
	}
	
	if (extmsg) {
		strncat(log->ex.msg, ": ", ETYMON_AF_MAX_MSG_SIZE - strlen(log->ex.msg) - 1);
		strncat(log->ex.msg, extmsg, ETYMON_AF_MAX_MSG_SIZE - strlen(log->ex.msg) - 1);
	}

	/* write to log */
	log->write(&(log->ex));
}
