#ifndef JC_PLATFORM_H
#define JC_PLATFORM_H
#ifdef JC_HOST_TEST
#include <stdint.h>
typedef int32_t s32;
#else
#include <gctypes.h>
#endif
#endif
