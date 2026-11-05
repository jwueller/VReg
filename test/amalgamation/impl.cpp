// A hostile consumer: the system header comes first, with whatever macros the
// combination defines, then the implementation.
#include <Windows.h>

#define VREG_IMPLEMENTATION
#include "vreg.h"

// The implementation's own macros stay out of the consumer's file.
#if defined(VREG_VERSION) || defined(VREG_VERSION_MAJOR) || defined(VREG_REFUSED_HOOKS)
#error "The amalgamated implementation leaks a macro."
#endif
