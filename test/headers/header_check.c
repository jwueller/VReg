// A hostile consumer, once per combination of the macros a consumer may or
// may not define (see CMakeLists.txt): the system header comes first, then
// the implementation, compiled as C. It only has to compile, warning-free.
#include <Windows.h>

#define VREG_IMPLEMENTATION
#include <vreg.h>

// The implementation's own macros stay out of the consumer's file.
#ifdef VREG__REFUSED_HOOKS
#error "The implementation leaks VREG__REFUSED_HOOKS."
#endif
