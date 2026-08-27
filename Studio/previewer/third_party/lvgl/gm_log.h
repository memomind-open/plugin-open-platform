#pragma once

/* The device LVGL fork routes diagnostics through the firmware logger. The
 * host build keeps the rendering code unchanged and discards those optional
 * diagnostics because Studio has its own logging channel. */
#define GMLOGD(...) ((void)0)
#define GMLOGE(...) ((void)0)
#define GMLOGD_STREAM(...) ((void)0)
#define GMLOGE_STREAM(...) ((void)0)
