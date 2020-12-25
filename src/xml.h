#ifndef DC_XML_H
#define DC_XML_H

#ifdef ETYMON_AF_XML

#include "config.h"
#include "index.h"
#include "fdef.h"

#ifdef __cplusplus
extern "C" {
#endif

	int dc_xml_init(ETYMON_AF_DC_INIT* dc_init);
	int dc_xml_index(ETYMON_AF_DC_INDEX* dc_index);

#ifdef __cplusplus
}
#endif

#endif

#endif
