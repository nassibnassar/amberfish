#ifndef DC_ERC_H
#define DC_ERC_H

#include "config.h"
#include "index.h"

#ifdef af__cplusplus
extern "C" {
#endif

	int dc_erc_init(ETYMON_AF_DC_INIT* dc_init);
	int dc_erc_index(ETYMON_AF_DC_INDEX* dc_index);

#ifdef af__cplusplus
}
#endif
	
#endif
