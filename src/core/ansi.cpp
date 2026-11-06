#include "core/ansi.h"

#include <cstring>
#include <new>

namespace VReg
{
std::wstring WideFromAnsi(const char* const pch, const std::size_t cch)
{
    VReg__WString text{};
    if (VReg__WideFromAnsi(nullptr, pch, cch, &text) != ERROR_SUCCESS)
        throw std::bad_alloc();

    std::wstring result(VReg__WStringText(&text), text.cch);
    VReg__WStringFree(nullptr, &text);
    return result;
}

std::string AnsiFromWide(const wchar_t* const pch, const std::size_t cch)
{
    VReg__Buffer text{};
    if (VReg__AnsiFromWide(nullptr, pch, cch, &text) != ERROR_SUCCESS)
        throw std::bad_alloc();

    std::string result(reinterpret_cast<const char*>(text.pb), text.cb);
    VReg__BufferFree(nullptr, &text);
    return result;
}

WideArg::WideArg(const LPCSTR psz) :
    m_isNull(psz == nullptr)
{
    if (psz != nullptr)
        m_text = WideFromAnsi(psz, std::strlen(psz));
}
} // namespace VReg
