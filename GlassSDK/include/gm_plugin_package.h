#ifndef GM_PLUGIN_PACKAGE_H
#define GM_PLUGIN_PACKAGE_H
#include "gm_plugin_xip.h"
#define GM_PLUGIN_PACKAGE_MAGIC "GMPK"
#define GM_PLUGIN_PACKAGE_FORMAT_VERSION GM_XIP_FORMAT
#define GM_PLUGIN_PACKAGE_HEADER_SIZE GM_XIP_HEADER_BYTES
typedef gm_xip_header_t gm_plugin_package_header_t;
#endif
