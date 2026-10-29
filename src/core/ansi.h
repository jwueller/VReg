#pragma once

#include <Windows.h>

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
