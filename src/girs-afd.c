/*
 *  Copyright (C) 2004  Etymon Systems, Inc.
 *
 *  Authors:  Nassib Nassar
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define SERVER_PORT (6145)

#include "girs.h"

int search(const Girs_search_request *rq, Girs_search_response *rs)
{
	/* testing only */
	printf("Received query: [%s]\n", rq->query);
	rs->results_n = 321;
	/* end test */
	
        return 0;
}

int afdmain(int argc, char *argv[])
{
	Girs_server_start server_start;

	memset(&server_start, 0, sizeof(Girs_server_start));
	server_start.port = SERVER_PORT;
	server_start.f_search = search;
        return girs_server_start(&server_start);
}
