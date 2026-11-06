#pragma once

#include <Windows.h>

/// @file
/// @brief What every VReg declaration carries: the linkage of the functions.
///  Defining `VREG_STATIC` before the headers (and the implementation, in
///  the single-header form) makes every function `static`, so a module can
///  hold a private copy beside another module's; two copies in one module
///  behave as two copies and share nothing but the handle namespace.

#ifdef VREG_STATIC
#define VREGDEF static
#else
#define VREGDEF extern
#endif
