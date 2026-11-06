#pragma once

#include <VReg/api.h>

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// A status a failing path answers, which the analyzer cannot prove nonzero
/// when it was stored by another function.
_Post_satisfies_(return != ERROR_SUCCESS)
VREGDEF LSTATUS VReg__FailureStatus(LSTATUS status);

#ifdef __cplusplus
} // extern "C"
#endif
