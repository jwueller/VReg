#pragma once

#include <VReg/api.h>

#include <Windows.h>

/// @file
/// @brief How VReg allocates. A consumer overrides `VREG_MALLOC`,
///  `VREG_REALLOC` and `VREG_FREE` together, before the implementation is
///  compiled; each takes the allocation context the registry or hive that
///  allocates was created with. The defaults use the process heap: no CRT
///  dependency, and safe under the loader lock.

#ifndef VREG_MALLOC
#define VREG_MALLOC(pAllocContext, cb) HeapAlloc(GetProcessHeap(), 0, (cb))
#endif

#ifndef VREG_REALLOC
#define VREG_REALLOC(pAllocContext, p, cb) HeapReAlloc(GetProcessHeap(), 0, (p), (cb))
#endif

#ifndef VREG_FREE
#define VREG_FREE(pAllocContext, p) HeapFree(GetProcessHeap(), 0, (p))
#endif

/// Breaks into an attached debugger in a `_DEBUG` build; nothing otherwise.
#ifndef VREG_ASSERT
#ifdef _DEBUG
#define VREG_ASSERT(x) ((x) ? (void)0 : VReg__AssertFailed())
#else
#define VREG_ASSERT(x) ((void)0)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

VREGDEF void VReg__AssertFailed(void);

/// Null for @p cb of zero or when memory ran out.
VREGDEF _Ret_maybenull_ void* VReg__Alloc(_In_opt_ void* pAllocContext, SIZE_T cb);

/// Like @ref VReg__Alloc, zero-filled.
VREGDEF _Ret_maybenull_ void* VReg__AllocZeroed(_In_opt_ void* pAllocContext, SIZE_T cb);

/// Resizes @p p (null allocates) to @p cb bytes; null when memory ran out,
/// leaving @p p as it was.
VREGDEF _Ret_maybenull_ void* VReg__Realloc(_In_opt_ void* pAllocContext, _In_opt_ void* p, SIZE_T cb);

/// Frees @p p; null is ignored.
VREGDEF void VReg__Free(_In_opt_ void* pAllocContext, _In_opt_ void* p);

#ifdef __cplusplus
} // extern "C"
#endif
