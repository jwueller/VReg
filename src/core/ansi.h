#pragma once

#include "core/buffer.h"

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Widens @p cch bytes through @ref CP_ACP into @p pText, without a
/// terminator beyond the one the string type keeps. Answers
/// @ref ERROR_OUTOFMEMORY when memory ran out or the text is longer than the
/// conversion can take.
LSTATUS VReg__WideFromAnsi(
    _In_opt_ void* pAllocContext,
    _In_reads_(cch) const char* pch,
    SIZE_T cch,
    _Inout_ VReg__WString* pText
);

/// Narrows @p cch UTF-16 units through @ref CP_ACP into @p pText, a buffer of
/// bytes without a terminator; what the code page cannot represent becomes
/// its default character. Answers @ref ERROR_OUTOFMEMORY as
/// @ref VReg__WideFromAnsi does.
LSTATUS VReg__AnsiFromWide(
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
LSTATUS VReg__WideArgInit(_In_opt_ void* pAllocContext, _In_opt_z_ LPCSTR psz, _Out_ VReg__WideArg* pArg);

/// The wide text, null when the argument was null.
_Ret_maybenull_z_ LPCWSTR VReg__WideArgText(_In_ const VReg__WideArg* pArg);

/// Like @ref VReg__WideArgText, as a buffer the callee may write to.
_Ret_maybenull_ LPWSTR VReg__WideArgMutable(_In_ VReg__WideArg* pArg);

void VReg__WideArgFree(_In_opt_ void* pAllocContext, _Inout_ VReg__WideArg* pArg);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <cstddef>
#include <string>

namespace VReg
{
/// Widens @p cch bytes through @ref CP_ACP.
/// @throws std::bad_alloc, std::length_error
[[nodiscard]] std::wstring WideFromAnsi(const char* pch, std::size_t cch);

/// Narrows @p cch UTF-16 units through @ref CP_ACP; what the code page cannot
/// represent becomes its default character.
/// @throws std::bad_alloc, std::length_error
[[nodiscard]] std::string AnsiFromWide(const wchar_t* pch, std::size_t cch);

/// A string argument that may be null: the wide copy of an ANSI argument.
class WideArg
{
public:
    explicit WideArg(LPCSTR psz);

    /// Null when the argument was null.
    [[nodiscard]] LPCWSTR Get() const noexcept
    {
        return m_isNull ? nullptr : m_text.c_str();
    }

    /// A mutable buffer, null when the argument was null.
    [[nodiscard]] LPWSTR GetMutable() noexcept
    {
        return m_isNull ? nullptr : m_text.data();
    }

private:
    bool m_isNull;
    std::wstring m_text;
};
} // namespace VReg

#endif // __cplusplus
