// Force-included when RPL sources are built for the host: wut's layout
// asserts assume 32-bit pointers, and the tests only need the enums.
#pragma once
#include <wut_structsize.h>
#undef WUT_CHECK_SIZE
#undef WUT_CHECK_OFFSET
#define WUT_CHECK_SIZE(Type, Size)
#define WUT_CHECK_OFFSET(Type, Offset, Field)
