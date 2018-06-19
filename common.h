#ifndef _common_h_
#define _common_h_

#ifdef _MSC_VER
#define FIXNES_NOINLINE __declspec(noinline)
#else
#define FIXNES_NOINLINE __attribute__((noinline))
#endif

#endif
