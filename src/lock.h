#ifndef _AF_LOCK_H
#define _AF_LOCK_H

#include "defs.h"

int etymon_db_ready(const char* dbname);

void etymon_db_unlock(const char* dbname);

int etymon_db_lock(const char* dbname, ETYMON_LOG* log);

#endif
