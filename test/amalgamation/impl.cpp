// A hostile consumer: the system header comes first, with whatever macros the
// combination defines, then the implementation.
#include <Windows.h>

#define VREG_IMPLEMENTATION
#include "vreg.h"

// The implementation's own macros stay out of the consumer's file.
#ifdef VREG_VERSION
#error "The amalgamated implementation leaks VREG_VERSION."
#endif
#ifdef VREG_VERSION_MAJOR
#error "The amalgamated implementation leaks VREG_VERSION_MAJOR."
#endif
#ifdef VREG_REFUSED_HOOKS
#error "The amalgamated implementation leaks VREG_REFUSED_HOOKS."
#endif
