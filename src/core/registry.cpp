#include <VReg/registry.h>

#include <VReg/handle.h>
#include <VReg/owned_values.h>

#include "ansi.h"
#include "retry.h"
#include "rrf.h"
#include "status.h"
#include "value_type.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <stdexcept>

namespace VReg
{
namespace
{
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

    const auto cchCopied = (std::min<std::size_t>)(text.size(), cchBuffer - 1);
    std::memcpy(pszBuffer, text.data(), cchCopied);
    pszBuffer[cchCopied] = '\0';
    return text.size() < cchBuffer;
}
} // namespace

LSTATUS Registry::RegOpenKeyExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
) noexcept
try
{
    const WideArg subKey(lpSubKey);
    // Deliberately reads the _Out_ parameter, to restore it below.
#pragma warning(suppress: 6001)
    const HKEY hPrevious = phkResult ? *phkResult : nullptr;
    const auto status = RegOpenKeyExW(
        hKey,
        subKey.Get(),
        ulOptions,
        samDesired,
        phkResult
    );
    // The native ANSI form leaves the result alone for an invalid handle; the
    // wide form nulls it.
    if (status == ERROR_INVALID_HANDLE && phkResult)
        *phkResult = hPrevious;
    return status;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Registry::RegCreateKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _Reserved_ const DWORD Reserved,
    _In_opt_ const LPSTR lpClass,
    _In_ const DWORD dwOptions,
    _In_ const REGSAM samDesired,
    _In_opt_ const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ const PHKEY phkResult,
    _Out_opt_ const LPDWORD lpdwDisposition
) noexcept
try
{
    const WideArg subKey(lpSubKey);
    WideArg className(lpClass);
    // Deliberately reads the _Out_ parameter, to restore it below.
#pragma warning(suppress: 6001)
    const HKEY hPrevious = phkResult ? *phkResult : nullptr;
    const auto status = RegCreateKeyExW(
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
catch (...)
{
    return StatusFromException();
}

LSTATUS Registry::RegQueryValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
) const noexcept
try
{
    if ((lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    const WideArg valueName(lpValueName);
    if (!lpData && !lpcbData)
        return RegQueryValueExW(
            hKey,
            valueName.Get(),
            nullptr,
            lpType,
            nullptr,
            nullptr
        );

    // Deliberately reads the _Out_ parameter: a failure below writes it back.
#pragma warning(suppress: 6001)
    DWORD dwType = lpType ? *lpType : 0;
    DWORD cbSize = (!lpData && lpcbData) ? *lpcbData : 0;
    const auto sized = RegQueryValueExW(
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
        return RegQueryValueExW(
            hKey,
            valueName.Get(),
            nullptr,
            lpType,
            lpData,
            lpcbData
        );

    std::vector<std::byte> value;
    const auto read = QueryValue(
        *this,
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
            // Native reads the wide data into the caller's buffer when it fits
            // there and narrows it in place, so the bytes past the text are the
            // tail of the wide data; it also terminates text that has no
            // terminator if there is room.
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
catch (...)
{
    return StatusFromException();
}

LSTATUS Registry::RegSetValueExA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
) noexcept
try
{
    const WideArg valueName(lpValueName);
    if (!IsStringType(dwType) || !lpData)
        return RegSetValueExW(
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
    // Native reads the byte after unterminated data and keeps it as the
    // terminator when it is zero, which it nearly always is. Agent's choice:
    // always terminate, so no byte past cbData is ever read; reading it, as
    // native does, can fault at the end of a page.
    if (!text.empty() && text.back() != L'\0')
        text.push_back(L'\0');

    return RegSetValueExW(
        hKey,
        valueName.Get(),
        Reserved,
        dwType,
        reinterpret_cast<const BYTE*>(text.data()),
        static_cast<DWORD>(text.size() * sizeof(wchar_t))
    );
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Registry::RegDeleteValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName
) noexcept
try
{
    const WideArg valueName(lpValueName);
    return RegDeleteValueW(
        hKey,
        valueName.Get()
    );
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Registry::RegDeleteKeyExA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
) noexcept
try
{
    const WideArg subKey(lpSubKey);
    return RegDeleteKeyExW(
        hKey,
        subKey.Get(),
        samDesired,
        Reserved
    );
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Registry::RegDeleteTreeA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
) noexcept
try
{
    const WideArg subKey(lpSubKey);
    return RegDeleteTreeW(
        hKey,
        subKey.Get()
    );
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Registry::RegEnumKeyExA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) const LPSTR lpName,
    _Inout_ const LPDWORD lpcchName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPSTR lpClass,
    _Inout_opt_ const LPDWORD lpcchClass,
    _Out_opt_ const PFILETIME lpftLastWriteTime
) const noexcept
try
{
    if (!lpcchName || (lpClass && !lpcchClass))
        return ERROR_INVALID_PARAMETER;

    const bool wantClass = lpClass || lpcchClass;
    DWORD cchName = INITIAL_NAME_CHARS;
    DWORD cchClass = wantClass ? INITIAL_NAME_CHARS : 0;
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
        status = RegEnumKeyExW(
            hKey,
            dwIndex,
            name.data(),
            &cchNameOut,
            lpReserved,
            wantClass ? className.data() : nullptr,
            wantClass ? &cchClassOut : nullptr,
            &ftLastWrite
        );
        if (
            status != ERROR_MORE_DATA
            || (cchName >= MAX_CLASS_CHARS && cchClass >= MAX_CLASS_CHARS)
        )
        {
            name.resize(cchNameOut);
            className.resize(cchClassOut);
            break;
        }
        cchName = (std::min)(cchName * 2, MAX_CLASS_CHARS);
        if (wantClass)
            cchClass = (std::min)(cchClass * 2, MAX_CLASS_CHARS);
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
catch (...)
{
    return StatusFromException();
}

LSTATUS Registry::RegEnumValueA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) const LPSTR lpValueName,
    _Inout_ const LPDWORD lpcchValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _Inout_opt_ const LPDWORD lpcbData
) const noexcept
try
{
    if (!lpValueName || !lpcchValueName || (lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    std::wstring name;
    DWORD dwType{};
    std::vector<std::byte> value;
    const auto status = EnumValue(
        *this,
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
        // Native terminates the text in the caller's buffer here, and writes
        // nothing else.
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
                // The capacity is the caller's *lpcbData, which was overwritten
                // above; cbBuffer holds it.
#pragma warning(suppress: 6386)
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
catch (...)
{
    return StatusFromException();
}

// Native answers success without writing lpClass when only the class length is
// asked for, which the annotation's terminator guarantee does not allow for.
#pragma warning(suppress: 6054)
LSTATUS Registry::RegQueryInfoKeyA(
    _In_ const HKEY hKey,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPSTR lpClass,
    _Inout_opt_ const LPDWORD lpcchClass,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpcSubKeys,
    _Out_opt_ const LPDWORD lpcbMaxSubKeyLen,
    _Out_opt_ const LPDWORD lpcbMaxClassLen,
    _Out_opt_ const LPDWORD lpcValues,
    _Out_opt_ const LPDWORD lpcbMaxValueNameLen,
    _Out_opt_ const LPDWORD lpcbMaxValueLen,
    _Out_opt_ const LPDWORD lpcbSecurityDescriptor,
    _Out_opt_ const PFILETIME lpftLastWriteTime
) const noexcept
try
{
    if (lpClass && !lpcchClass)
        return ERROR_INVALID_PARAMETER;

    // Native answers success for a zero-length class buffer, without a class,
    // but reports the class length if the caller asked for the longest class.
    const bool wantClass = lpClass && *lpcchClass != 0;
    const bool wantClassLength = lpClass && !wantClass && lpcbMaxClassLen;
    if (!wantClass && !wantClassLength)
    {
        return RegQueryInfoKeyW(
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

    DWORD cchClass = INITIAL_NAME_CHARS;
    DWORD cchClassOut = 0;
    std::wstring className;
    LSTATUS status;
    for (;;)
    {
        className.assign(cchClass, L'\0');
        cchClassOut = cchClass;
        status = RegQueryInfoKeyW(
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
        if (status != ERROR_MORE_DATA || cchClass >= MAX_CLASS_CHARS)
        {
            className.resize(cchClassOut);
            break;
        }
        cchClass = (std::min)(cchClass * 2, MAX_CLASS_CHARS);
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
catch (...)
{
    return StatusFromException();
}

LSTATUS Registry::RegLoadMUIStringA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCSTR pszDirectory
) const noexcept
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

// The SDK's _Post_z_ holds on every return, truncated reads and failures
// included; the analyzer cannot see the terminators written at run time.
#pragma warning(suppress: 6054)
LSTATUS Registry::RegGetValueA(
    _In_ const HKEY hkey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValue,
    _In_ const DWORD dwFlags,
    _Out_opt_ const LPDWORD pdwType,
    _When_(
        (dwFlags & 0x7F) == RRF_RT_REG_SZ
        || (dwFlags & 0x7F) == RRF_RT_REG_EXPAND_SZ
        || (dwFlags & 0x7F) == (RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ)
        || *pdwType == REG_SZ
        || *pdwType == REG_EXPAND_SZ,
        _Post_z_
    )
    _When_(
        (dwFlags & 0x7F) == RRF_RT_REG_MULTI_SZ
        || *pdwType == REG_MULTI_SZ,
        _Post_ _NullNull_terminated_
    )
    _Out_writes_bytes_to_opt_(*pcbData, *pcbData) const PVOID pvData,
    _Inout_opt_ const LPDWORD pcbData
) const noexcept
try
{
    if (pvData && !pcbData)
        return ERROR_INVALID_PARAMETER;

    const WideArg subKey(lpSubKey);
    const WideArg value(lpValue);
    const auto forwardToWide = [&]
    {
        return RegGetValueW(
            hkey,
            subKey.Get(),
            value.Get(),
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );
    };

    // Only string values are converted; anything else, and every failure to
    // find the value, is what the wide form answers.
    const DWORD dwFlagsStored =
        (dwFlags & RRF_WOW_SUBKEYS)
        | RRF_RT_ANY
        | RRF_NOEXPAND;
    DWORD dwStoredType{};
    const auto statusStoredType = RegGetValueW(
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

    // A value whose type the flags reject is still read and reported, with the
    // failure status.
    const DWORD dwFlagsTyped = dwFlags & ~RRF_ZEROONFAILURE;
    const DWORD dwFlagsAnyType =
        (dwFlags & (RRF_WOW_SUBKEYS | RRF_NOEXPAND))
        | RRF_RT_ANY;
    DWORD dwType = 0;
    DWORD cbQuery = 0;
    const auto status = RegGetValueW(
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
        RegGetValueW(
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
        *this,
        hkey,
        subKey.Get(),
        value.Get(),
        dwFlagsRead,
        &dwType,
        data
    );
    if (statusRead != ERROR_SUCCESS)
        return statusRead;

    const bool isExpanded =
        dwStoredType == REG_EXPAND_SZ
        && !(dwFlags & RRF_NOEXPAND);
    std::vector<std::byte> stored;
    DWORD cbStoredRaw = 0;
    if (isExpanded)
    {
        DWORD dwReadType{};
        DWORD cbStoredQuery = 0;
        const auto statusStored = GetValue(
            *this,
            hkey,
            subKey.Get(),
            value.Get(),
            dwFlagsStored,
            &dwReadType,
            stored
        );
        if (statusStored != ERROR_SUCCESS)
            return statusStored;
        RegGetValueW(
            hkey,
            subKey.Get(),
            value.Get(),
            dwFlagsStored,
            &dwReadType,
            nullptr,
            &cbStoredQuery
        );
        cbStoredRaw = cbStoredQuery - (std::min<DWORD>)(cbStoredQuery, 2);
    }

    const auto text
        = AnsiFromWide(
            reinterpret_cast<const wchar_t*>(data.data()),
            data.size() / sizeof(wchar_t)
        );
    const auto cbText = static_cast<DWORD>(text.size());
    // What a call that cannot return the text reports as its size: room for the
    // terminator the wide form reserves; for an expanded value, the larger of
    // the unexpanded text and the expansion, and a character more when the
    // expansion is the larger.
    DWORD cbReported = cbText + (
        cbQuery > data.size()
            ? static_cast<DWORD>((cbQuery - data.size()) / sizeof(wchar_t))
            : 0
    );
    if (isExpanded)
    {
        const auto storedChars = reinterpret_cast<const wchar_t*>(
            stored.data()
        );
        const auto cbRawText
            = static_cast<DWORD>(
                AnsiFromWide(
                    storedChars,
                    cbStoredRaw / sizeof(wchar_t)
                ).size()
            );
        const auto ibNul = text.find('\0');
        const DWORD cbExpanded =
            ibNul == std::string::npos
                ? cbText
                : static_cast<DWORD>(ibNul + 1);
        cbReported = (std::max)(
            cbRawText,
            cbExpanded
        ) + (cbExpanded > cbRawText ? 1 : 0);
    }
    const DWORD cbBuffer = pcbData ? *pcbData : 0;
    const DWORD cbNeeded = isExpanded ? cbText + 1 : cbText;
    const LSTATUS statusMismatch =
        isMismatch
            ? status
            : static_cast<LSTATUS>(ERROR_SUCCESS);

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
            // Native copies the unexpanded text, then clears its first
            // character.
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
                        (std::min<std::size_t>)(
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

    // Native reads the stored wide data into the caller's buffer when it fits
    // there and narrows it in place, so the bytes past the text are the tail of
    // the stored data. The wide form reports two bytes (four for a
    // multi-string) more than are stored.
    auto* const pbData = static_cast<BYTE*>(pvData);
    if (isExpanded)
    {
        if (cbStoredRaw <= cbBuffer)
            std::memcpy(
                pbData,
                stored.data(),
                (std::min<std::size_t>)(cbStoredRaw, stored.size())
            );
    }
    else
    {
        const DWORD cbStored =
            cbQuery
            - (std::min<DWORD>)(cbQuery, dwType == REG_MULTI_SZ ? 4 : 2);
        if (cbStored <= cbBuffer)
            std::memcpy(
                pbData,
                data.data(),
                (std::min<std::size_t>)(cbStored, data.size())
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
catch (...)
{
    return StatusFromException();
}

LSTATUS Registry::RegOpenKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
) noexcept
{
    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return RegOpenKeyExA(
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );
}

LSTATUS Registry::RegOpenKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
) noexcept
{
    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return RegOpenKeyExW(
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );
}

LSTATUS Registry::RegCreateKeyA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
) noexcept
{
    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!IsPredefinedHKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return RegCreateKeyExA(
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

LSTATUS Registry::RegCreateKeyW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
) noexcept
{
    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!IsPredefinedHKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return RegCreateKeyExW(
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

LSTATUS Registry::RegQueryValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
    _Inout_opt_ const PLONG lpcbData
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = RegOpenKeyA(
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    DWORD dwType{};
    auto status = RegQueryValueExA(
        hSubKey,
        nullptr,
        nullptr,
        &dwType,
        reinterpret_cast<LPBYTE>(lpData),
        reinterpret_cast<LPDWORD>(lpcbData)
    );
    if (hSubKey != hKey)
        RegCloseKey(
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

LSTATUS Registry::RegQueryValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
    _Inout_opt_ const PLONG lpcbData
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = RegOpenKeyW(
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    DWORD dwType{};
    auto status = RegQueryValueExW(
        hSubKey,
        nullptr,
        nullptr,
        &dwType,
        reinterpret_cast<LPBYTE>(lpData),
        reinterpret_cast<LPDWORD>(lpcbData)
    );
    if (hSubKey != hKey)
        RegCloseKey(
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

LSTATUS Registry::RegSetValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
    _In_ const DWORD cbData
) noexcept
{
    // Native ignores the size and stores the string's own length.
    (void)cbData;
    if (dwType != REG_SZ || !lpData)
        return ERROR_INVALID_PARAMETER;

    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = RegCreateKeyA(
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    const auto status = RegSetValueExA(
        hSubKey,
        nullptr,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(lpData),
        static_cast<DWORD>(std::strlen(lpData) + 1)
    );
    if (hSubKey != hKey)
        RegCloseKey(
            hSubKey
        );
    return status;
}

LSTATUS Registry::RegSetValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
    _In_ const DWORD cbData
) noexcept
{
    // Native ignores the size and stores the string's own length.
    (void)cbData;
    if (dwType != REG_SZ || !lpData)
        return ERROR_INVALID_PARAMETER;

    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = RegCreateKeyW(
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    const auto status = RegSetValueExW(
        hSubKey,
        nullptr,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(lpData),
        static_cast<DWORD>((std::wcslen(lpData) + 1) * sizeof(wchar_t))
    );
    if (hSubKey != hKey)
        RegCloseKey(
            hSubKey
        );
    return status;
}

LSTATUS Registry::RegDeleteKeyA(
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey
) noexcept
{
    return RegDeleteKeyExA(
        hKey,
        lpSubKey,
        0,
        0
    );
}

LSTATUS Registry::RegDeleteKeyW(
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey
) noexcept
{
    return RegDeleteKeyExW(
        hKey,
        lpSubKey,
        0,
        0
    );
}

LSTATUS Registry::RegEnumKeyA(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPSTR lpName,
    _In_ DWORD cchName
) const noexcept
{
    return RegEnumKeyExA(
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

LSTATUS Registry::RegEnumKeyW(
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPWSTR lpName,
    _In_ DWORD cchName
) const noexcept
{
    return RegEnumKeyExW(
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

// The subkey of SetKeyValue and DeleteKeyValue is opened with KEY_SET_VALUE,
// the one right the call needs; the maximum allowed access, as the pre-Ex
// forms use, was passed over. Only a caller whose access is limited to
// something else could tell.
LSTATUS Registry::RegSetKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = RegCreateKeyExA(
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

    const auto status = RegSetValueExA(
        hSubKey,
        lpValueName,
        0,
        dwType,
        static_cast<const BYTE*>(lpData),
        cbData
    );
    if (hSubKey != hKey)
        RegCloseKey(
            hSubKey
        );
    return status;
}

LSTATUS Registry::RegSetKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = RegCreateKeyExW(
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

    const auto status = RegSetValueExW(
        hSubKey,
        lpValueName,
        0,
        dwType,
        static_cast<const BYTE*>(lpData),
        cbData
    );
    if (hSubKey != hKey)
        RegCloseKey(
            hSubKey
        );
    return status;
}

LSTATUS Registry::RegDeleteKeyValueA(
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = RegOpenKeyExA(
            hKey,
            lpSubKey,
            0,
            KEY_SET_VALUE,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    const auto status = RegDeleteValueA(
        hSubKey,
        lpValueName
    );
    if (hSubKey != hKey)
        RegCloseKey(
            hSubKey
        );
    return status;
}

LSTATUS Registry::RegDeleteKeyValueW(
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName
) noexcept
{
    HKEY hSubKey = hKey;
    if (lpSubKey && *lpSubKey)
    {
        const auto status = RegOpenKeyExW(
            hKey,
            lpSubKey,
            0,
            KEY_SET_VALUE,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    const auto status = RegDeleteValueW(
        hSubKey,
        lpValueName
    );
    if (hSubKey != hKey)
        RegCloseKey(
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

/// Native places each value on a @ref DWORD boundary, reads the ones that fit
/// the buffer in order, and fills the entries only when all of them did.
LSTATUS Registry::RegQueryMultipleValuesW(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTW val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
) const noexcept
try
{
    if (!val_list || !ldwTotsize)
        return ERROR_INVALID_PARAMETER;

    const DWORD cbBuffer = *ldwTotsize;
    *ldwTotsize = 0;
    if (!lpValueBuf)
        return ERROR_NOACCESS;

    auto* const pbBuffer = reinterpret_cast<BYTE*>(lpValueBuf);
    std::vector<PlacedValue> placed(num_vals);
    for (DWORD i = 0; i < num_vals; ++i)
    {
        const auto status = RegQueryValueExW(
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
                (std::min)(ibValue, cbBuffer) - cbEnd
            );

        placed[i].ibValue = ibValue;
        if (ibValue + cbValue <= cbBuffer)
        {
            DWORD cbRead = cbValue;
            const auto status = RegQueryValueExW(
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
        val_list[i].ve_valueptr = reinterpret_cast<DWORD_PTR>(
            pbBuffer + placed[i].ibValue
        );
    }
    return static_cast<LSTATUS>(ERROR_SUCCESS);
}
catch (...)
{
    return StatusFromException();
}

/// Places the narrowed values like the wide form places the stored ones, but
/// leaves the gaps alone and rounds the total up. When they do not all fit,
/// native answers the wide form's total for the stored values.
// The SDK types lpValueBuf as a string, but it holds binary values.
#pragma warning(suppress: 6054)
LSTATUS Registry::RegQueryMultipleValuesA(
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTA val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
) const noexcept
try
{
    if (!val_list || !ldwTotsize)
        return ERROR_INVALID_PARAMETER;
    if (!lpValueBuf)
        return ERROR_NOACCESS;

    const DWORD cbBuffer = *ldwTotsize;
    auto* const pbBuffer = reinterpret_cast<BYTE*>(lpValueBuf);
    std::vector<PlacedValue> placed(num_vals);
    DWORD cbEnd = 0;
    for (DWORD i = 0; i < num_vals; ++i)
    {
        const auto status = RegQueryValueExA(
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
            const auto status = RegQueryValueExW(
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
        const auto status = RegQueryValueExA(
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
        val_list[i].ve_valueptr = reinterpret_cast<DWORD_PTR>(
            pbBuffer + placed[i].ibValue
        );
    }
    *ldwTotsize = AlignedToDword(cbEnd);
    return static_cast<LSTATUS>(ERROR_SUCCESS);
}
catch (...)
{
    return StatusFromException();
}
} // namespace VReg
