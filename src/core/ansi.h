#pragma once

#include <VReg/api.h>

#include "core/buffer.h"

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Widens @p cch bytes through @ref CP_ACP into @p pText, without a
/// terminator beyond the one the string type keeps. Answers
/// @ref ERROR_OUTOFMEMORY when memory ran out or the text is longer than the
/// conversion can take.
VREGDEF LSTATUS VReg__WideFromAnsi(
    _In_opt_ void* pAllocContext,
    _In_reads_(cch) const char* pch,
    SIZE_T cch,
    _Inout_ VReg__WString* pText
);

/// Narrows @p cch UTF-16 units through @ref CP_ACP into @p pText, a buffer of
/// bytes without a terminator; what the code page cannot represent becomes
/// its default character. Answers @ref ERROR_OUTOFMEMORY as
/// @ref VReg__WideFromAnsi does.
VREGDEF LSTATUS VReg__AnsiFromWide(
    _In_opt_ void* pAllocContext,
    _In_reads_(cch) const wchar_t* pch,
    SIZE_T cch,
    _Inout_ VReg__Buffer* pText
);

/// A string argument that may be null: the wide copy of an ANSI argument.
typedef struct VReg__WideArg
{
    BOOL isNull;
    VReg__WString text;
} VReg__WideArg;

/// Fills @p pArg from @p psz; a zero-filled @p pArg stands for a null one.
VREGDEF LSTATUS VReg__WideArgInit(_In_opt_ void* pAllocContext, _In_opt_z_ LPCSTR psz, _Out_ VReg__WideArg* pArg);

/// The wide text, null when the argument was null.
VREGDEF _Ret_maybenull_z_ LPCWSTR VReg__WideArgText(_In_ const VReg__WideArg* pArg);

/// Like @ref VReg__WideArgText, as a buffer the callee may write to.
VREGDEF _Ret_maybenull_ LPWSTR VReg__WideArgMutable(_In_ VReg__WideArg* pArg);

VREGDEF void VReg__WideArgFree(_In_opt_ void* pAllocContext, _Inout_ VReg__WideArg* pArg);

#ifdef __cplusplus
} // extern "C"
#endif
