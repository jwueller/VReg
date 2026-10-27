#include <VReg/registry.h>

#include "ansi.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>

namespace VReg
{
namespace
{
// Agent's choice: names and classes are read into a buffer of 256 characters, which holds the longest key name,
// and grow by doubling on ERROR_MORE_DATA up to the limits below. Sizing each call with a query first was passed
// over as a second round trip for what is almost always one.
constexpr DWORD kInitialNameChars = 256;
constexpr DWORD kMaxClassChars = 32768;
constexpr DWORD kMaxValueNameChars = 16384;

template <class Function>
LSTATUS Guard(Function&& function) noexcept
{
    try
    {
        return function();
    }
    catch (const std::bad_alloc&)
    {
        return ERROR_OUTOFMEMORY;
    }
    catch (const std::length_error&)
    {
        return ERROR_OUTOFMEMORY;
    }
}

/// A predefined root such as `HKEY_CURRENT_USER`, sign-extended on x64.
bool IsPredefinedKey(const HKEY hKey) noexcept
{
    const auto value = reinterpret_cast<std::uintptr_t>(hKey);
    const auto low = static_cast<std::uint32_t>(value);
    const auto signExtended = static_cast<std::uintptr_t>(static_cast<std::int32_t>(low));
    return low >= 0x80000000u && low <= 0x800000FFu && value == signExtended;
}

bool IsStringType(const DWORD dwType) noexcept
{
    return dwType == REG_SZ || dwType == REG_EXPAND_SZ || dwType == REG_MULTI_SZ;
}

/// Copies @p text and a terminator into a name buffer of @p cchBuffer bytes.
/// False if it does not fit: native then writes nothing unless the buffer is
/// exactly the text's length, in which case it writes the text with its last
/// character replaced by the terminator.
bool CopyAnsiName(
    const std::string& text,
    LPSTR const pszBuffer,
    const DWORD cchBuffer
) noexcept
{
    if (!pszBuffer)
        return false;

    if (text.size() < cchBuffer)
    {
        std::memcpy(pszBuffer, text.data(), text.size());
        pszBuffer[text.size()] = '\0';
        return true;
    }

    if (text.size() == cchBuffer && cchBuffer > 0)
    {
        std::memcpy(pszBuffer, text.data(), text.size() - 1);
        pszBuffer[text.size() - 1] = '\0';
    }
    return false;
}

/// Like @ref CopyAnsiName for a class buffer, which native fills with as much
/// of the text as fits (and a terminator) whenever it has room for one.
bool CopyAnsiClass(
    const std::string& text,
    LPSTR const pszBuffer,
    const DWORD cchBuffer
) noexcept
{
    if (cchBuffer == 0)
        return false;

    const auto cchCopied = std::min<std::size_t>(text.size(), cchBuffer - 1);
    std::memcpy(pszBuffer, text.data(), cchCopied);
    pszBuffer[cchCopied] = '\0';
    return text.size() < cchBuffer;
}

/// The size to retry with: the one the call asked for, or more than before if
/// it asked for no more than it was given.
DWORD Grown(const DWORD cbOld, const DWORD cbRequired) noexcept
{
    return cbRequired > cbOld ? cbRequired : cbOld * 2 + 16;
}
} // namespace

LSTATUS Registry::OpenKeyExA(
    const HKEY hKey,
    const LPCSTR lpSubKey,
    const DWORD ulOptions,
    const REGSAM samDesired,
    const PHKEY phkResult
) noexcept
{
    return Guard(
        [&]
        {
            const WideArg subKey(lpSubKey);
            const HKEY hPrevious = phkResult ? *phkResult : nullptr;
            const auto status = OpenKeyExW(
                hKey,
                subKey.Get(),
                ulOptions,
                samDesired,
                phkResult
            );
            // The native A form leaves the result alone for an invalid handle; the W form nulls it.
            if (status == ERROR_INVALID_HANDLE && phkResult)
                *phkResult = hPrevious;
            return status;
        }
    );
}

LSTATUS Registry::CreateKeyExA(
    const HKEY hKey,
    const LPCSTR lpSubKey,
    const DWORD Reserved,
    const LPSTR lpClass,
    const DWORD dwOptions,
    const REGSAM samDesired,
    const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    const PHKEY phkResult,
    const LPDWORD lpdwDisposition
) noexcept
{
    return Guard(
        [&]
        {
            const WideArg subKey(lpSubKey);
            WideArg className(lpClass);
            const HKEY hPrevious = phkResult ? *phkResult : nullptr;
            const auto status = CreateKeyExW(
                hKey,
                subKey.Get(),
                Reserved,
                className.GetMutable(),
                dwOptions,
                samDesired,
                lpSecurityAttributes,
                phkResult,
                lpdwDisposition
            );
            if (status == ERROR_INVALID_HANDLE && phkResult)
                *phkResult = hPrevious;
            return status;
        }
    );
}

LSTATUS Registry::QueryValueExA(
    const HKEY hKey,
    const LPCSTR lpValueName,
    const LPDWORD lpReserved,
    const LPDWORD lpType,
    const LPBYTE lpData,
    const LPDWORD lpcbData
) noexcept
{
    if ((lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    return Guard(
        [&]
        {
            const WideArg valueName(lpValueName);
            if (!lpData && !lpcbData)
                return QueryValueExW(
                    hKey,
                    valueName.Get(),
                    nullptr,
                    lpType,
                    nullptr,
                    nullptr
                );

            DWORD dwType = lpType ? *lpType : 0;
            DWORD cbSize = (!lpData && lpcbData) ? *lpcbData : 0;
            const auto sized = QueryValueExW(
                hKey,
                valueName.Get(),
                nullptr,
                &dwType,
                nullptr,
                &cbSize
            );
            if (sized != ERROR_SUCCESS)
            {
                if (lpType)
                    *lpType = dwType;
                if (!lpData && lpcbData)
                    *lpcbData = cbSize;
                return sized;
            }

            if (!IsStringType(dwType))
                return QueryValueExW(
                    hKey,
                    valueName.Get(),
                    nullptr,
                    lpType,
                    lpData,
                    lpcbData
                );

            std::vector<std::byte> value;
            const auto read = QueryValue(
                hKey,
                valueName.Get(),
                &dwType,
                value
            );
            if (read != ERROR_SUCCESS)
                return read;

            const auto text = AnsiFromWide(
                reinterpret_cast<const wchar_t*>(value.data()),
                value.size() / sizeof(wchar_t)
            );
            const auto cbText = static_cast<DWORD>(text.size());
            LSTATUS status = ERROR_SUCCESS;
            if (lpData)
            {
                if (cbText > *lpcbData)
                {
                    status = ERROR_MORE_DATA;
                }
                else
                {
                    // Native reads the wide data into the caller's buffer when it fits there and narrows it in
                    // place, so the bytes past the text are the tail of the wide data; it also terminates text
                    // that has no terminator if there is room.
                    if (value.size() <= *lpcbData)
                        std::memcpy(lpData, value.data(), value.size());
                    if (cbText > 0)
                    {
                        std::memcpy(lpData, text.data(), cbText);
                        if (cbText < *lpcbData && lpData[cbText - 1] != 0)
                            lpData[cbText] = 0;
                    }
                }
            }

            if (lpType)
                *lpType = dwType;
            if (lpcbData)
                *lpcbData = cbText;
            return status;
        }
    );
}

LSTATUS Registry::SetValueExA(
    const HKEY hKey,
    const LPCSTR lpValueName,
    const DWORD Reserved,
    const DWORD dwType,
    const BYTE* const lpData,
    const DWORD cbData
) noexcept
{
    return Guard(
        [&]
        {
            const WideArg valueName(lpValueName);
            if (!IsStringType(dwType) || !lpData)
                return SetValueExW(
                    hKey,
                    valueName.Get(),
                    Reserved,
                    dwType,
                    lpData,
                    cbData
                );

            auto text = WideFromAnsi(
                reinterpret_cast<const char*>(lpData),
                cbData
            );
            // Native reads the byte after unterminated data and keeps it as the terminator when it is zero, which it
            // nearly always is. Agent's choice: always terminate, so no byte past cbData is ever read; reading it,
            // as native does, can fault at the end of a page.
            if (!text.empty() && text.back() != L'\0')
                text.push_back(L'\0');

            return SetValueExW(
                hKey,
                valueName.Get(),
                Reserved,
                dwType,
                reinterpret_cast<const BYTE*>(text.data()),
                static_cast<DWORD>(text.size() * sizeof(wchar_t))
            );
        }
    );
}

LSTATUS Registry::DeleteValueA(
    const HKEY hKey,
    const LPCSTR lpValueName
) noexcept
{
    return Guard(
        [&]
        {
            const WideArg valueName(lpValueName);
            return DeleteValueW(
                hKey,
                valueName.Get()
            );
        }
    );
}

LSTATUS Registry::DeleteKeyExA(
    const HKEY hKey,
    const LPCSTR lpSubKey,
    const REGSAM samDesired,
    const DWORD Reserved
) noexcept
{
    return Guard(
        [&]
        {
            const WideArg subKey(lpSubKey);
            return DeleteKeyExW(
                hKey,
                subKey.Get(),
                samDesired,
                Reserved
            );
        }
    );
}

LSTATUS Registry::DeleteTreeA(
    const HKEY hKey,
    const LPCSTR lpSubKey
) noexcept
{
    return Guard(
        [&]
        {
            const WideArg subKey(lpSubKey);
            return DeleteTreeW(
                hKey,
                subKey.Get()
            );
        }
    );
}

LSTATUS Registry::EnumKeyExA(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPSTR lpName,
    const LPDWORD lpcchName,
    const LPDWORD lpReserved,
    const LPSTR lpClass,
    const LPDWORD lpcchClass,
    const PFILETIME lpftLastWriteTime
) noexcept
{
    if (!lpcchName || (lpClass && !lpcchClass))
        return ERROR_INVALID_PARAMETER;

    return Guard(
        [&]
        {
            const bool wantClass = lpClass || lpcchClass;
            DWORD cchName = kInitialNameChars;
            DWORD cchClass = wantClass ? kInitialNameChars : 0;
            std::wstring name;
            std::wstring className;
            FILETIME ftLastWrite{};
            LSTATUS status;
            for (;;)
            {
                name.assign(cchName, L'\0');
                className.assign(cchClass, L'\0');
                DWORD cchNameOut = cchName;
                DWORD cchClassOut = cchClass;
                status = EnumKeyExW(
                    hKey,
                    dwIndex,
                    name.data(),
                    &cchNameOut,
                    lpReserved,
                    wantClass ? className.data() : nullptr,
                    wantClass ? &cchClassOut : nullptr,
                    &ftLastWrite
                );
                if (status != ERROR_MORE_DATA || (cchName >= kMaxClassChars && cchClass >= kMaxClassChars))
                {
                    name.resize(cchNameOut);
                    className.resize(cchClassOut);
                    break;
                }
                cchName = std::min(cchName * 2, kMaxClassChars);
                if (wantClass)
                    cchClass = std::min(cchClass * 2, kMaxClassChars);
            }
            if (status != ERROR_SUCCESS)
                return status;

            if (lpftLastWriteTime)
                *lpftLastWriteTime = ftLastWrite;

            const auto nameA = AnsiFromWide(name.data(), name.size());
            if (!CopyAnsiName(nameA, lpName, *lpcchName))
                return ERROR_MORE_DATA;
            *lpcchName = static_cast<DWORD>(nameA.size());

            if (lpcchClass && !lpClass)
            {
                *lpcchClass = 0;
            }
            else if (lpcchClass)
            {
                const auto classA = AnsiFromWide(
                    className.data(),
                    className.size()
                );
                if (!CopyAnsiClass(classA, lpClass, *lpcchClass))
                    return ERROR_MORE_DATA;
                *lpcchClass = static_cast<DWORD>(classA.size());
            }

            return static_cast<LSTATUS>(ERROR_SUCCESS);
        }
    );
}

LSTATUS Registry::EnumValueA(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPSTR lpValueName,
    const LPDWORD lpcchValueName,
    const LPDWORD lpReserved,
    const LPDWORD lpType,
    const LPBYTE lpData,
    const LPDWORD lpcbData
) noexcept
{
    if (!lpValueName || !lpcchValueName || (lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    return Guard(
        [&]
        {
            std::wstring name;
            DWORD dwType{};
            std::vector<std::byte> value;
            const auto status = EnumValue(
                hKey,
                dwIndex,
                name,
                &dwType,
                lpcbData ? &value : nullptr
            );
            if (status != ERROR_SUCCESS)
                return status;

            const auto nameA = AnsiFromWide(name.data(), name.size());
            const bool isString = IsStringType(dwType);
            std::string text;
            DWORD cbValue = static_cast<DWORD>(value.size());
            if (lpcbData && isString)
            {
                text = AnsiFromWide(
                    reinterpret_cast<const wchar_t*>(value.data()),
                    value.size() / sizeof(wchar_t)
                );
                cbValue = static_cast<DWORD>(text.size());
            }

            // Native reports the type and size even when the call goes on to fail.
            if (lpType)
                *lpType = dwType;
            const DWORD cbBuffer = lpcbData ? *lpcbData : 0;
            if (lpcbData)
                *lpcbData = cbValue;
            if (lpData && cbValue > cbBuffer)
                return ERROR_MORE_DATA;

            if (nameA.size() > *lpcchValueName)
            {
                // Native terminates the text in the caller's buffer here, and writes nothing else.
                if (lpData && isString && cbValue > 0 && cbValue < cbBuffer)
                    lpData[cbValue] = 0;
                return ERROR_MORE_DATA;
            }

            const bool nameFits = CopyAnsiName(
                nameA,
                lpValueName,
                *lpcchValueName
            );
            if (lpData)
            {
                if (isString)
                {
                    if (value.size() <= cbBuffer)
                        std::memcpy(lpData, value.data(), value.size());
                    if (cbValue > 0)
                    {
                        std::memcpy(lpData, text.data(), cbValue);
                        if (cbValue < cbBuffer && lpData[cbValue - 1] != 0)
                            lpData[cbValue] = 0;
                    }
                }
                else
                {
                    std::memcpy(lpData, value.data(), cbValue);
                }
            }

            if (!nameFits)
                return ERROR_MORE_DATA;
            *lpcchValueName = static_cast<DWORD>(nameA.size());
            return static_cast<LSTATUS>(ERROR_SUCCESS);
        }
    );
}

LSTATUS Registry::QueryInfoKeyA(
    const HKEY hKey,
    const LPSTR lpClass,
    const LPDWORD lpcchClass,
    const LPDWORD lpReserved,
    const LPDWORD lpcSubKeys,
    const LPDWORD lpcbMaxSubKeyLen,
    const LPDWORD lpcbMaxClassLen,
    const LPDWORD lpcValues,
    const LPDWORD lpcbMaxValueNameLen,
    const LPDWORD lpcbMaxValueLen,
    const LPDWORD lpcbSecurityDescriptor,
    const PFILETIME lpftLastWriteTime
) noexcept
{
    if (lpClass && !lpcchClass)
        return ERROR_INVALID_PARAMETER;

    // Native answers success for a zero-length class buffer, without a class, but reports the class length
    // if the caller asked for the longest class.
    const bool wantClass = lpClass && *lpcchClass != 0;
    const bool wantClassLength = lpClass && !wantClass && lpcbMaxClassLen;
    if (!wantClass && !wantClassLength)
    {
        return QueryInfoKeyW(
            hKey,
            nullptr,
            lpClass ? nullptr : lpcchClass,
            lpReserved,
            lpcSubKeys,
            lpcbMaxSubKeyLen,
            lpcbMaxClassLen,
            lpcValues,
            lpcbMaxValueNameLen,
            lpcbMaxValueLen,
            lpcbSecurityDescriptor,
            lpftLastWriteTime
        );
    }

    return Guard(
        [&]
        {
            DWORD cchClass = kInitialNameChars;
            DWORD cchClassOut = 0;
            std::wstring className;
            LSTATUS status;
            for (;;)
            {
                className.assign(cchClass, L'\0');
                cchClassOut = cchClass;
                status = QueryInfoKeyW(
                    hKey,
                    className.data(),
                    &cchClassOut,
                    lpReserved,
                    lpcSubKeys,
                    lpcbMaxSubKeyLen,
                    lpcbMaxClassLen,
                    lpcValues,
                    lpcbMaxValueNameLen,
                    lpcbMaxValueLen,
                    lpcbSecurityDescriptor,
                    lpftLastWriteTime
                );
                if (status != ERROR_MORE_DATA || cchClass >= kMaxClassChars)
                {
                    className.resize(cchClassOut);
                    break;
                }
                cchClass = std::min(cchClass * 2, kMaxClassChars);
            }
            if (status != ERROR_SUCCESS)
            {
                if (cchClassOut != cchClass)
                    *lpcchClass = cchClassOut;
                return status;
            }

            const auto classA = AnsiFromWide(
                className.data(),
                className.size()
            );
            if (wantClassLength)
            {
                *lpcchClass = static_cast<DWORD>(classA.size());
                return static_cast<LSTATUS>(ERROR_SUCCESS);
            }
            if (!CopyAnsiClass(classA, lpClass, *lpcchClass))
            {
                *lpcchClass -= 1;
                return ERROR_MORE_DATA;
            }
            *lpcchClass = static_cast<DWORD>(classA.size());
            return static_cast<LSTATUS>(ERROR_SUCCESS);
        }
    );
}

LSTATUS Registry::LoadMUIStringA(
    const HKEY hKey,
    const LPCSTR pszValue,
    const LPSTR pszOutBuf,
    const DWORD cbOutBuf,
    const LPDWORD pcbData,
    const DWORD Flags,
    const LPCSTR pszDirectory
) noexcept
{
    (void)hKey;
    (void)pszValue;
    (void)pszOutBuf;
    (void)cbOutBuf;
    (void)pcbData;
    (void)Flags;
    (void)pszDirectory;
    return ERROR_CALL_NOT_IMPLEMENTED;
}

LSTATUS Registry::GetValueA(
    const HKEY hkey,
    const LPCSTR lpSubKey,
    const LPCSTR lpValue,
    const DWORD dwFlags,
    const LPDWORD pdwType,
    const PVOID pvData,
    const LPDWORD pcbData
) noexcept
{
    if (pvData && !pcbData)
        return ERROR_INVALID_PARAMETER;

    return Guard(
        [&]
        {
            const WideArg subKey(lpSubKey);
            const WideArg value(lpValue);
            const auto forwardToWide = [&]
            {
                return GetValueW(
                    hkey,
                    subKey.Get(),
                    value.Get(),
                    dwFlags,
                    pdwType,
                    pvData,
                    pcbData
                );
            };

            // Only string values are converted; anything else, and every failure to find the value, is what the
            // wide form answers.
            constexpr DWORD kWowFlags = RRF_SUBKEY_WOW6464KEY | RRF_SUBKEY_WOW6432KEY;
            const DWORD dwFlagsStored = (dwFlags & kWowFlags) | RRF_RT_ANY | RRF_NOEXPAND;
            DWORD dwStoredType{};
            const auto statusStoredType = GetValueW(
                hkey,
                subKey.Get(),
                value.Get(),
                dwFlagsStored,
                &dwStoredType,
                nullptr,
                nullptr
            );
            if (statusStoredType != ERROR_SUCCESS || !IsStringType(dwStoredType))
                return forwardToWide();

            // A value whose type the flags reject is still read and reported, with the failure status.
            const DWORD dwFlagsTyped = dwFlags & ~RRF_ZEROONFAILURE;
            const DWORD dwFlagsAnyType = (dwFlags & (kWowFlags | RRF_NOEXPAND)) | RRF_RT_ANY;
            DWORD dwType = 0;
            DWORD cbQuery = 0;
            const auto status = GetValueW(
                hkey,
                subKey.Get(),
                value.Get(),
                dwFlagsTyped,
                &dwType,
                nullptr,
                &cbQuery
            );
            const bool isMismatch = status == ERROR_UNSUPPORTED_TYPE;
            if (status != ERROR_SUCCESS && !isMismatch)
                return forwardToWide();
            const DWORD dwFlagsRead = isMismatch ? dwFlagsAnyType : dwFlagsTyped;
            if (isMismatch)
                GetValueW(
                    hkey,
                    subKey.Get(),
                    value.Get(),
                    dwFlagsAnyType,
                    &dwType,
                    nullptr,
                    &cbQuery
                );

            std::vector<std::byte> data;
            const auto statusRead = GetValue(
                hkey,
                subKey.Get(),
                value.Get(),
                dwFlagsRead,
                &dwType,
                data
            );
            if (statusRead != ERROR_SUCCESS)
                return statusRead;

            const bool isExpanded = dwStoredType == REG_EXPAND_SZ && !(dwFlags & RRF_NOEXPAND);
            std::vector<std::byte> stored;
            DWORD cbStoredRaw = 0;
            if (isExpanded)
            {
                DWORD dwReadType{};
                DWORD cbStoredQuery = 0;
                const auto statusStored = GetValue(
                    hkey,
                    subKey.Get(),
                    value.Get(),
                    dwFlagsStored,
                    &dwReadType,
                    stored
                );
                if (statusStored != ERROR_SUCCESS)
                    return statusStored;
                GetValueW(
                    hkey,
                    subKey.Get(),
                    value.Get(),
                    dwFlagsStored,
                    &dwReadType,
                    nullptr,
                    &cbStoredQuery
                );
                cbStoredRaw = cbStoredQuery - std::min<DWORD>(cbStoredQuery, 2);
            }

            const auto text
                = AnsiFromWide(
                    reinterpret_cast<const wchar_t*>(data.data()),
                    data.size() / sizeof(wchar_t)
                );
            const auto cbText = static_cast<DWORD>(text.size());
            // What a call that cannot return the text reports as its size: room for the terminator the wide
            // form reserves; for an expanded value, the larger of the unexpanded text and the expansion, and
            // a character more when the expansion is the larger.
            DWORD cbReported
                = cbText + (cbQuery > data.size() ? static_cast<DWORD>((cbQuery - data.size()) / sizeof(wchar_t)) : 0);
            if (isExpanded)
            {
                const auto storedChars = reinterpret_cast<const wchar_t*>(stored.data());
                const auto cbRawText
                    = static_cast<DWORD>(
                        AnsiFromWide(
                            storedChars,
                            cbStoredRaw / sizeof(wchar_t)
                        ).size()
                    );
                const auto ibNul = text.find('\0');
                const DWORD cbExpanded = ibNul == std::string::npos ? cbText : static_cast<DWORD>(ibNul + 1);
                cbReported = std::max(
                    cbRawText,
                    cbExpanded
                ) + (cbExpanded > cbRawText ? 1 : 0);
            }
            const DWORD cbBuffer = pcbData ? *pcbData : 0;
            const DWORD cbNeeded = isExpanded ? cbText + 1 : cbText;
            const LSTATUS statusMismatch = isMismatch ? status : static_cast<LSTATUS>(ERROR_SUCCESS);

            if (pdwType)
                *pdwType = dwType;

            if (!pvData)
            {
                if (pcbData)
                    *pcbData = cbReported;
                return statusMismatch;
            }

            if (cbBuffer < cbNeeded)
            {
                *pcbData = cbReported;
                if (isExpanded)
                {
                    // Native copies the unexpanded text, then clears its first character.
                    const auto storedText = AnsiFromWide(
                        reinterpret_cast<const wchar_t*>(stored.data()),
                        stored.size() / sizeof(wchar_t)
                    );
                    if (!storedText.empty() && storedText.size() <= cbBuffer)
                    {
                        if (cbStoredRaw <= cbBuffer)
                            std::memcpy(
                                pvData,
                                stored.data(),
                                std::min<std::size_t>(
                                    cbStoredRaw,
                                    stored.size()
                                )
                            );
                        std::memcpy(
                            pvData,
                            storedText.data(),
                            storedText.size()
                        );
                        *static_cast<char*>(pvData) = '\0';
                        *pcbData = cbNeeded;
                    }
                }
                if ((dwFlags & RRF_ZEROONFAILURE) && cbBuffer > 0)
                    std::memset(pvData, 0, cbBuffer);
                return isMismatch ? status : ERROR_MORE_DATA;
            }

            // Native reads the stored wide data into the caller's buffer when it fits there and narrows it in
            // place, so the bytes past the text are the tail of the stored data. The wide form reports two bytes
            // (four for a multi-string) more than are stored.
            auto* const pbData = static_cast<BYTE*>(pvData);
            if (isExpanded)
            {
                if (cbStoredRaw <= cbBuffer)
                    std::memcpy(
                        pbData,
                        stored.data(),
                        std::min<std::size_t>(cbStoredRaw, stored.size())
                    );
            }
            else
            {
                const DWORD cbStored = cbQuery - std::min<DWORD>(cbQuery, dwType == REG_MULTI_SZ ? 4 : 2);
                if (cbStored <= cbBuffer)
                    std::memcpy(
                        pbData,
                        data.data(),
                        std::min<std::size_t>(cbStored, data.size())
                    );
            }
            if (cbText > 0)
            {
                std::memcpy(pbData, text.data(), cbText);
                if (cbText < cbBuffer && pbData[cbText - 1] != 0)
                    pbData[cbText] = 0;
            }
            *pcbData = cbText;

            if (isMismatch && (dwFlags & RRF_ZEROONFAILURE) && cbBuffer > 0)
                std::memset(pvData, 0, cbBuffer);
            return statusMismatch;
        }
    );
}

LSTATUS Registry::OpenKeyA(
    const HKEY hKey,
    const LPCSTR lpSubKey,
    const PHKEY phkResult
) noexcept
{
    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return OpenKeyExA(
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );
}

LSTATUS Registry::OpenKeyW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const PHKEY phkResult
) noexcept
{
    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return OpenKeyExW(
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );
}

LSTATUS Registry::CreateKeyA(
    const HKEY hKey,
    const LPCSTR lpSubKey,
    const PHKEY phkResult
) noexcept
{
    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!IsPredefinedKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return CreateKeyExA(
        hKey,
        lpSubKey,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        MAXIMUM_ALLOWED,
        nullptr,
        phkResult,
        nullptr
    );
}

LSTATUS Registry::CreateKeyW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const PHKEY phkResult
) noexcept
{
    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!IsPredefinedKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return CreateKeyExW(
        hKey,
        lpSubKey,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        MAXIMUM_ALLOWED,
        nullptr,
        phkResult,
        nullptr
    );
}

LSTATUS Registry::QueryValueA(
    const HKEY hKey,
    const LPCSTR lpSubKey,
    const LPSTR lpData,
    const PLONG lpcbData
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = OpenKeyA(
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    DWORD dwType{};
    auto status = QueryValueExA(
        hSubKey,
        nullptr,
        nullptr,
        &dwType,
        reinterpret_cast<LPBYTE>(lpData),
        reinterpret_cast<LPDWORD>(lpcbData)
    );
    if (hSubKey != hKey)
        CloseKey(
            hSubKey
        );
    if (status == ERROR_SUCCESS && dwType != REG_SZ)
        status = ERROR_INVALID_DATA;

    if (status == ERROR_FILE_NOT_FOUND)
    {
        if (lpData)
            *lpData = '\0';
        if (lpcbData)
            *lpcbData = sizeof(char);
        status = ERROR_SUCCESS;
    }
    return status;
}

LSTATUS Registry::QueryValueW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const LPWSTR lpData,
    const PLONG lpcbData
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = OpenKeyW(
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    DWORD dwType{};
    auto status = QueryValueExW(
        hSubKey,
        nullptr,
        nullptr,
        &dwType,
        reinterpret_cast<LPBYTE>(lpData),
        reinterpret_cast<LPDWORD>(lpcbData)
    );
    if (hSubKey != hKey)
        CloseKey(
            hSubKey
        );
    if (status == ERROR_SUCCESS && dwType != REG_SZ)
        status = ERROR_INVALID_DATA;

    if (status == ERROR_FILE_NOT_FOUND)
    {
        if (lpData)
            *lpData = L'\0';
        if (lpcbData)
            *lpcbData = sizeof(wchar_t);
        status = ERROR_SUCCESS;
    }
    return status;
}

LSTATUS Registry::SetValueA(
    const HKEY hKey,
    const LPCSTR lpSubKey,
    const DWORD dwType,
    const LPCSTR lpData,
    const DWORD cbData
) noexcept
{
    // Native ignores the size and stores the string's own length.
    (void)cbData;
    if (dwType != REG_SZ || !lpData)
        return ERROR_INVALID_PARAMETER;

    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = CreateKeyA(
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    const auto status = SetValueExA(
        hSubKey,
        nullptr,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(lpData),
        static_cast<DWORD>(std::strlen(lpData) + 1)
    );
    if (hSubKey != hKey)
        CloseKey(
            hSubKey
        );
    return status;
}

LSTATUS Registry::SetValueW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const DWORD dwType,
    const LPCWSTR lpData,
    const DWORD cbData
) noexcept
{
    // Native ignores the size and stores the string's own length.
    (void)cbData;
    if (dwType != REG_SZ || !lpData)
        return ERROR_INVALID_PARAMETER;

    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = CreateKeyW(
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    const auto status = SetValueExW(
        hSubKey,
        nullptr,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(lpData),
        static_cast<DWORD>((std::wcslen(lpData) + 1) * sizeof(wchar_t))
    );
    if (hSubKey != hKey)
        CloseKey(
            hSubKey
        );
    return status;
}

LSTATUS Registry::DeleteKeyA(
    const HKEY hKey,
    const LPCSTR lpSubKey
) noexcept
{
    return DeleteKeyExA(
        hKey,
        lpSubKey,
        0,
        0
    );
}

LSTATUS Registry::DeleteKeyW(
    const HKEY hKey,
    const LPCWSTR lpSubKey
) noexcept
{
    return DeleteKeyExW(
        hKey,
        lpSubKey,
        0,
        0
    );
}

LSTATUS Registry::EnumKeyA(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPSTR lpName,
    DWORD cchName
) noexcept
{
    return EnumKeyExA(
        hKey,
        dwIndex,
        lpName,
        &cchName,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
}

LSTATUS Registry::EnumKeyW(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPWSTR lpName,
    DWORD cchName
) noexcept
{
    return EnumKeyExW(
        hKey,
        dwIndex,
        lpName,
        &cchName,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
}

// Agent's choice: the subkey of SetKeyValue and DeleteKeyValue is opened with KEY_SET_VALUE, the one right the
// call needs; the maximum allowed access, as the pre-Ex forms use, was passed over. Only a caller whose access
// is limited to something else could tell.
LSTATUS Registry::SetKeyValueA(
    const HKEY hKey,
    const LPCSTR lpSubKey,
    const LPCSTR lpValueName,
    const DWORD dwType,
    const LPCVOID lpData,
    const DWORD cbData
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = CreateKeyExA(
            hKey,
            lpSubKey,
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &hSubKey,
            nullptr
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    const auto status = SetValueExA(
        hSubKey,
        lpValueName,
        0,
        dwType,
        static_cast<const BYTE*>(lpData),
        cbData
    );
    if (hSubKey != hKey)
        CloseKey(
            hSubKey
        );
    return status;
}

LSTATUS Registry::SetKeyValueW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const LPCWSTR lpValueName,
    const DWORD dwType,
    const LPCVOID lpData,
    const DWORD cbData
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = CreateKeyExW(
            hKey,
            lpSubKey,
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &hSubKey,
            nullptr
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    const auto status = SetValueExW(
        hSubKey,
        lpValueName,
        0,
        dwType,
        static_cast<const BYTE*>(lpData),
        cbData
    );
    if (hSubKey != hKey)
        CloseKey(
            hSubKey
        );
    return status;
}

LSTATUS Registry::DeleteKeyValueA(
    const HKEY hKey,
    const LPCSTR lpSubKey,
    const LPCSTR lpValueName
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = OpenKeyExA(
            hKey,
            lpSubKey,
            0,
            KEY_SET_VALUE,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    const auto status = DeleteValueA(
        hSubKey,
        lpValueName
    );
    if (hSubKey != hKey)
        CloseKey(
            hSubKey
        );
    return status;
}

LSTATUS Registry::DeleteKeyValueW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const LPCWSTR lpValueName
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = OpenKeyExW(
            hKey,
            lpSubKey,
            0,
            KEY_SET_VALUE,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    const auto status = DeleteValueW(
        hSubKey,
        lpValueName
    );
    if (hSubKey != hKey)
        CloseKey(
            hSubKey
        );
    return status;
}

namespace
{
struct PlacedValue
{
    DWORD cbValue;
    DWORD dwType;
    DWORD ibValue;
};

DWORD AlignedToDword(const DWORD cb) noexcept
{
    return (cb + 3) & ~DWORD{3};
}
} // namespace

/// Native places each value on a `DWORD` boundary, reads the ones that fit
/// the buffer in order, and fills the entries only when all of them did.
LSTATUS Registry::QueryMultipleValuesW(
    const HKEY hKey,
    const PVALENTW val_list,
    const DWORD num_vals,
    const LPWSTR lpValueBuf,
    const LPDWORD ldwTotsize
) noexcept
{
    if (!val_list || !ldwTotsize)
        return ERROR_INVALID_PARAMETER;

    const DWORD cbBuffer = *ldwTotsize;
    *ldwTotsize = 0;
    if (!lpValueBuf)
        return ERROR_NOACCESS;

    return Guard(
        [&]
        {
            auto* const pbBuffer = reinterpret_cast<BYTE*>(lpValueBuf);
            std::vector<PlacedValue> placed(num_vals);
            for (DWORD i = 0; i < num_vals; ++i)
            {
                const auto status = QueryValueExW(
                    hKey,
                    val_list[i].ve_valuename,
                    nullptr,
                    nullptr,
                    nullptr,
                    &placed[i].cbValue
                );
                if (status != ERROR_SUCCESS)
                {
                    if (status == ERROR_INVALID_HANDLE)
                        *ldwTotsize = cbBuffer;
                    return status;
                }
            }

            DWORD cbEnd = 0;
            bool isComplete = true;
            for (DWORD i = 0; i < num_vals; ++i)
            {
                const DWORD cbValue = placed[i].cbValue;
                const DWORD ibValue = i == 0 ? 0 : AlignedToDword(cbEnd);
                if (ibValue > cbEnd && cbEnd < cbBuffer)
                    std::memset(
                        pbBuffer + cbEnd,
                        0,
                        std::min(ibValue, cbBuffer) - cbEnd
                    );

                placed[i].ibValue = ibValue;
                if (ibValue + cbValue <= cbBuffer)
                {
                    DWORD cbRead = cbValue;
                    const auto status = QueryValueExW(
                        hKey,
                        val_list[i].ve_valuename,
                        nullptr,
                        &placed[i].dwType,
                        pbBuffer + ibValue,
                        &cbRead
                    );
                    if (status != ERROR_SUCCESS)
                        return status;
                    placed[i].cbValue = cbRead;
                }
                else
                {
                    isComplete = false;
                }
                cbEnd = ibValue + cbValue;
            }

            *ldwTotsize = cbEnd;
            if (!isComplete)
                return ERROR_MORE_DATA;

            for (DWORD i = 0; i < num_vals; ++i)
            {
                val_list[i].ve_valuelen = placed[i].cbValue;
                val_list[i].ve_type = placed[i].dwType;
                val_list[i].ve_valueptr = reinterpret_cast<DWORD_PTR>(pbBuffer + placed[i].ibValue);
            }
            return static_cast<LSTATUS>(ERROR_SUCCESS);
        }
    );
}

/// Places the narrowed values like the wide form places the stored ones, but
/// leaves the gaps alone and rounds the total up. When they do not all fit,
/// native answers the wide form's total for the stored values.
LSTATUS Registry::QueryMultipleValuesA(
    const HKEY hKey,
    const PVALENTA val_list,
    const DWORD num_vals,
    const LPSTR lpValueBuf,
    const LPDWORD ldwTotsize
) noexcept
{
    if (!val_list || !ldwTotsize)
        return ERROR_INVALID_PARAMETER;
    if (!lpValueBuf)
        return ERROR_NOACCESS;

    return Guard(
        [&]
        {
            const DWORD cbBuffer = *ldwTotsize;
            auto* const pbBuffer = reinterpret_cast<BYTE*>(lpValueBuf);
            std::vector<PlacedValue> placed(num_vals);
            DWORD cbEnd = 0;
            for (DWORD i = 0; i < num_vals; ++i)
            {
                const auto status = QueryValueExA(
                    hKey,
                    val_list[i].ve_valuename,
                    nullptr,
                    nullptr,
                    nullptr,
                    &placed[i].cbValue
                );
                if (status != ERROR_SUCCESS)
                    return status;
                placed[i].ibValue = i == 0 ? 0 : AlignedToDword(cbEnd);
                cbEnd = placed[i].ibValue + placed[i].cbValue;
            }

            if (cbEnd > cbBuffer)
            {
                DWORD cbWideEnd = 0;
                for (DWORD i = 0; i < num_vals; ++i)
                {
                    const WideArg name(val_list[i].ve_valuename);
                    DWORD cbWide = 0;
                    const auto status = QueryValueExW(
                        hKey,
                        name.Get(),
                        nullptr,
                        nullptr,
                        nullptr,
                        &cbWide
                    );
                    if (status != ERROR_SUCCESS)
                        return status;
                    cbWideEnd = (i == 0 ? 0 : AlignedToDword(cbWideEnd)) + cbWide;
                }
                *ldwTotsize = cbWideEnd;
                return ERROR_MORE_DATA;
            }

            for (DWORD i = 0; i < num_vals; ++i)
            {
                DWORD cbRead = placed[i].cbValue;
                const auto status = QueryValueExA(
                    hKey,
                    val_list[i].ve_valuename,
                    nullptr,
                    &placed[i].dwType,
                    pbBuffer + placed[i].ibValue,
                    &cbRead
                );
                if (status != ERROR_SUCCESS)
                    return status;
                placed[i].cbValue = cbRead;
            }

            for (DWORD i = 0; i < num_vals; ++i)
            {
                val_list[i].ve_valuelen = placed[i].cbValue;
                val_list[i].ve_type = placed[i].dwType;
                val_list[i].ve_valueptr = reinterpret_cast<DWORD_PTR>(pbBuffer + placed[i].ibValue);
            }
            *ldwTotsize = AlignedToDword(cbEnd);
            return static_cast<LSTATUS>(ERROR_SUCCESS);
        }
    );
}

LSTATUS Registry::QueryValue(
    const HKEY hKey,
    const LPCWSTR lpValueName,
    const LPDWORD lpType,
    std::vector<std::byte>& data
) noexcept
{
    return Guard(
        [&]
        {
            DWORD dwType{};
            DWORD cbData = 0;
            const auto sized = QueryValueExW(
                hKey,
                lpValueName,
                nullptr,
                &dwType,
                nullptr,
                &cbData
            );
            if (sized != ERROR_SUCCESS)
                return sized;

            for (;;)
            {
                data.resize(cbData);
                DWORD cbRead = cbData;
                const auto status = QueryValueExW(
                    hKey,
                    lpValueName,
                    nullptr,
                    &dwType,
                    reinterpret_cast<LPBYTE>(data.data()),
                    &cbRead
                );
                if (status == ERROR_MORE_DATA)
                {
                    cbData = Grown(cbData, cbRead);
                    continue;
                }
                if (status == ERROR_SUCCESS)
                {
                    data.resize(cbRead);
                    if (lpType)
                        *lpType = dwType;
                }
                return status;
            }
        }
    );
}

LSTATUS Registry::GetValue(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const LPCWSTR lpValue,
    const DWORD dwFlags,
    const LPDWORD pdwType,
    std::vector<std::byte>& data
) noexcept
{
    return Guard(
        [&]
        {
            DWORD dwType{};
            DWORD cbData = 0;
            const auto sized = GetValueW(
                hKey,
                lpSubKey,
                lpValue,
                dwFlags,
                &dwType,
                nullptr,
                &cbData
            );
            if (sized != ERROR_SUCCESS)
                return sized;

            for (;;)
            {
                data.resize(cbData);
                DWORD cbRead = cbData;
                const auto status = GetValueW(
                    hKey,
                    lpSubKey,
                    lpValue,
                    dwFlags,
                    &dwType,
                    data.data(),
                    &cbRead
                );
                if (status == ERROR_MORE_DATA)
                {
                    cbData = Grown(cbData, cbRead);
                    continue;
                }
                if (status == ERROR_SUCCESS)
                {
                    data.resize(cbRead);
                    if (pdwType)
                        *pdwType = dwType;
                }
                return status;
            }
        }
    );
}

LSTATUS Registry::GetString(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const LPCWSTR lpValue,
    const DWORD dwFlags,
    std::wstring& text
) noexcept
{
    return Guard(
        [&]
        {
            const DWORD dwTypeFlags = RRF_RT_REG_SZ | ((dwFlags & RRF_NOEXPAND) ? RRF_RT_REG_EXPAND_SZ : 0);
            std::vector<std::byte> data;
            const auto status = GetValue(
                hKey,
                lpSubKey,
                lpValue,
                (dwFlags & ~RRF_RT_ANY) | dwTypeFlags,
                nullptr,
                data
            );
            if (status != ERROR_SUCCESS)
                return status;

            std::size_t cch = data.size() / sizeof(wchar_t);
            const auto* const pch = reinterpret_cast<const wchar_t*>(data.data());
            if (cch > 0 && pch[cch - 1] == L'\0')
                --cch;
            text.assign(pch, cch);
            return static_cast<LSTATUS>(ERROR_SUCCESS);
        }
    );
}

LSTATUS Registry::EnumKey(
    const HKEY hKey,
    const DWORD dwIndex,
    std::wstring& name
) noexcept
{
    return Guard(
        [&]
        {
            DWORD cchName = kInitialNameChars;
            for (;;)
            {
                name.assign(cchName, L'\0');
                DWORD cchRead = cchName;
                const auto status = EnumKeyExW(
                    hKey,
                    dwIndex,
                    name.data(),
                    &cchRead,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr
                );
                if (status == ERROR_MORE_DATA && cchName < kMaxClassChars)
                {
                    cchName = std::min(Grown(cchName, cchRead), kMaxClassChars);
                    continue;
                }
                if (status == ERROR_SUCCESS)
                    name.resize(cchRead);
                return status;
            }
        }
    );
}

LSTATUS Registry::EnumValue(
    const HKEY hKey,
    const DWORD dwIndex,
    std::wstring& name,
    const LPDWORD lpType,
    std::vector<std::byte>* const pData
) noexcept
{
    return Guard(
        [&]
        {
            DWORD cchName = kInitialNameChars;
            DWORD cbData = 0;
            bool isSized = !pData;
            for (;;)
            {
                name.assign(cchName, L'\0');
                DWORD cchRead = cchName;
                DWORD cbRead = cbData;
                DWORD dwType{};
                const auto status = EnumValueW(
                    hKey,
                    dwIndex,
                    name.data(),
                    &cchRead,
                    nullptr,
                    &dwType,
                    (pData && isSized) ? reinterpret_cast<LPBYTE>(pData->data()) : nullptr,
                    pData ? &cbRead : nullptr
                );
                if (status == ERROR_MORE_DATA)
                {
                    cchName = std::min(
                        Grown(cchName, cchRead + 1),
                        kMaxValueNameChars
                    );
                    if (pData)
                        cbData = Grown(cbData, cbRead);
                    if (pData && isSized)
                        pData->resize(cbData);
                    continue;
                }
                if (status == ERROR_SUCCESS && !isSized)
                {
                    cbData = cbRead;
                    pData->resize(cbData);
                    isSized = true;
                    continue;
                }
                if (status == ERROR_SUCCESS)
                {
                    name.resize(cchRead);
                    if (pData)
                        pData->resize(cbRead);
                    if (lpType)
                        *lpType = dwType;
                }
                return status;
            }
        }
    );
}

LSTATUS Registry::LoadMUIString(
    const HKEY hKey,
    const LPCWSTR pszValue,
    const DWORD Flags,
    const LPCWSTR pszDirectory,
    std::wstring& text
) noexcept
{
    return Guard(
        [&]
        {
            DWORD cbBuffer = 256 * sizeof(wchar_t);
            for (;;)
            {
                text.assign(cbBuffer / sizeof(wchar_t), L'\0');
                DWORD cbData = 0;
                const auto status = LoadMUIStringW(
                    hKey,
                    pszValue,
                    text.data(),
                    cbBuffer,
                    &cbData,
                    Flags,
                    pszDirectory
                );
                if (status == ERROR_MORE_DATA)
                {
                    cbBuffer = Grown(cbBuffer, cbData);
                    continue;
                }
                if (status == ERROR_SUCCESS)
                {
                    std::size_t cch = cbData / sizeof(wchar_t);
                    if (cch > 0 && text[cch - 1] == L'\0')
                        --cch;
                    text.resize(cch);
                }
                return status;
            }
        }
    );
}
} // namespace VReg
