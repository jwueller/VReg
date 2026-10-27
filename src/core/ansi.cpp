#include "ansi.h"

#include <climits>
#include <cstring>
#include <stdexcept>

namespace VReg
{
std::wstring WideFromAnsi(const char* const pch, const std::size_t cch)
{
    if (cch == 0)
        return {};
    if (cch > INT_MAX)
        throw std::length_error("ANSI string too long");

    const auto cchBytes = static_cast<int>(cch);
    const auto cchWide = MultiByteToWideChar(
        CP_ACP,
        0,
        pch,
        cchBytes,
        nullptr,
        0
    );
    std::wstring text(static_cast<std::size_t>(cchWide), L'\0');
    if (cchWide > 0)
        MultiByteToWideChar(
            CP_ACP,
            0,
            pch,
            cchBytes,
            text.data(),
            cchWide
        );
    return text;
}

std::string AnsiFromWide(const wchar_t* const pch, const std::size_t cch)
{
    if (cch == 0)
        return {};
    if (cch > INT_MAX)
        throw std::length_error("UTF-16 string too long");

    const auto cchUnits = static_cast<int>(cch);
    const auto cchAnsi = WideCharToMultiByte(
        CP_ACP,
        0,
        pch,
        cchUnits,
        nullptr,
        0,
        nullptr,
        nullptr
    );
    std::string text(static_cast<std::size_t>(cchAnsi), '\0');
    if (cchAnsi > 0)
        WideCharToMultiByte(
            CP_ACP,
            0,
            pch,
            cchUnits,
            text.data(),
            cchAnsi,
            nullptr,
            nullptr
        );
    return text;
}

WideArg::WideArg(const LPCSTR psz) :
    m_isNull(psz == nullptr)
{
    if (psz != nullptr)
        m_text = WideFromAnsi(psz, std::strlen(psz));
}
} // namespace VReg
