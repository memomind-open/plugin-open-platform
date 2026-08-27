#pragma once

/* The firmware routes disabled LVGL logs through its application logger. */
#define LOGD(...) ((void)0)
#define LOGW(...) ((void)0)
#define LOGE(...) ((void)0)
#define LOGR(...) ((void)0)
#ifndef UNUSED
#define UNUSED(value) ((void)(value))
#endif
