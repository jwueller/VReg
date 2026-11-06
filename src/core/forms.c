#include <VReg/registry.h>

#include <VReg/handle.h>

#include "core/alloc.h"
#include "core/ansi.h"
#include "core/buffer.h"
#include "core/owned_reads.h"
#include "core/retry.h"
#include "core/rrf.h"
#include "core/value_type.h"

/// @file
/// @brief The forms Win32 builds from the wide `Ex` ones: ANSI, pre-`Ex` and
///  the conveniences. Each calls its slot when the registry fills it and
///  derives the call from the wide forms otherwise; ANSI forms widen and
///  narrow through @ref CP_ACP in both directions, as the native ANSI
///  functions do. Every one of them answers what the native function answers
///  for the same input: status, out-sizes and bytes.

typedef struct VReg__PlacedValue
{
    DWORD cbValue;
    DWORD dwType;
    DWORD ibValue;
} VReg__PlacedValue;

static DWORD VReg__AlignedToDword(const DWORD cb)
{
    return (cb + 3) & ~(DWORD)3;
}

static SIZE_T VReg__MinSize(const SIZE_T a, const SIZE_T b)
{
    return a < b ? a : b;
}

static SIZE_T VReg__AnsiLength(_In_z_ const char* const psz)
{
    SIZE_T cch = 0;
    while (psz[cch] != '\0')
        ++cch;

    return cch;
}

/// Copies @p cb bytes; a null @p pSource is legal only for none.
static void VReg__CopyBytes(
    _Out_writes_bytes_(cb) void* const pDestination,
    _In_reads_bytes_opt_(cb) const void* const pSource,
    const SIZE_T cb
)
{
    if (cb == 0)
        return;

    _Analysis_assume_(pSource != NULL);
    CopyMemory(pDestination, pSource, cb);
}

/// Copies @p pText and a terminator into a name buffer of @p cchBuffer bytes.
/// False if it does not fit: native then writes nothing unless the buffer is
/// exactly the text's length, in which case it writes the text with its last
/// character replaced by the terminator.
static BOOL VReg__CopyAnsiName(
    _In_ const VReg__Buffer* const pText,
    _Out_writes_opt_(cchBuffer) const LPSTR pszBuffer,
    const DWORD cchBuffer
)
{
    if (!pszBuffer)
        return FALSE;

    if (pText->cb < cchBuffer)
    {
        VReg__CopyBytes(pszBuffer, pText->pb, pText->cb);
        pszBuffer[pText->cb] = '\0';
        return TRUE;
    }

    if (pText->cb == cchBuffer && cchBuffer > 0)
    {
        VReg__CopyBytes(pszBuffer, pText->pb, pText->cb - 1);
        pszBuffer[pText->cb - 1] = '\0';
    }
    return FALSE;
}

/// Like @ref VReg__CopyAnsiName for a class buffer, which native fills with as
/// much of the text as fits (and a terminator) whenever it has room for one.
static BOOL VReg__CopyAnsiClass(
    _In_ const VReg__Buffer* const pText,
    _Out_writes_(cchBuffer) const LPSTR pszBuffer,
    const DWORD cchBuffer
)
{
    SIZE_T cchCopied;
    if (cchBuffer == 0)
        return FALSE;

    cchCopied = VReg__MinSize(pText->cb, cchBuffer - 1);
    VReg__CopyBytes(pszBuffer, pText->pb, cchCopied);
    pszBuffer[cchCopied] = '\0';
    return pText->cb < cchBuffer;
}

LSTATUS WINAPI VRegOpenKeyExA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    if (pRegistry->lpVtbl->RegOpenKeyExA)
        return pRegistry->lpVtbl->RegOpenKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg subKey = {FALSE, {NULL, 0, 0}};
    HKEY hPrevious;
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    // Deliberately reads the _Out_ parameter, to restore it below.
#pragma warning(suppress: 6001)
    hPrevious = phkResult ? *phkResult : NULL;
    status = VRegOpenKeyExW(
        pRegistry,
        hKey,
        VReg__WideArgText(&subKey),
        ulOptions,
        samDesired,
        phkResult
    );
    // The native ANSI form leaves the result alone for an invalid handle; the
    // wide form nulls it.
    if (status == ERROR_INVALID_HANDLE && phkResult)
        *phkResult = hPrevious;

done:
    VReg__WideArgFree(pAllocContext, &subKey);
    return status;}

LSTATUS WINAPI VRegCreateKeyExA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _Reserved_ const DWORD Reserved,
    _In_opt_ const LPSTR lpClass,
    _In_ const DWORD dwOptions,
    _In_ const REGSAM samDesired,
    _In_opt_ const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ const PHKEY phkResult,
    _Out_opt_ const LPDWORD lpdwDisposition
)
{
    if (pRegistry->lpVtbl->RegCreateKeyExA)
        return pRegistry->lpVtbl->RegCreateKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            Reserved,
            lpClass,
            dwOptions,
            samDesired,
            lpSecurityAttributes,
            phkResult,
            lpdwDisposition
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg subKey = {FALSE, {NULL, 0, 0}};
    VReg__WideArg className = {FALSE, {NULL, 0, 0}};
    HKEY hPrevious;
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__WideArgInit(pAllocContext, lpClass, &className);
    if (status != ERROR_SUCCESS)
        goto done;

    // Deliberately reads the _Out_ parameter, to restore it below.
#pragma warning(suppress: 6001)
    hPrevious = phkResult ? *phkResult : NULL;
    // The annotation excludes a null subkey; one that is null anyway reaches
    // the wide form unchanged, which answers for it.
#pragma warning(suppress: 6387)
    status = VRegCreateKeyExW(
        pRegistry,
        hKey,
        VReg__WideArgText(&subKey),
        Reserved,
        VReg__WideArgMutable(&className),
        dwOptions,
        samDesired,
        lpSecurityAttributes,
        phkResult,
        lpdwDisposition
    );
    if (status == ERROR_INVALID_HANDLE && phkResult)
        *phkResult = hPrevious;

done:
    VReg__WideArgFree(pAllocContext, &className);
    VReg__WideArgFree(pAllocContext, &subKey);
    return status;}

LSTATUS WINAPI VRegQueryValueExA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    if (pRegistry->lpVtbl->RegQueryValueExA)
        return pRegistry->lpVtbl->RegQueryValueExA(
            pRegistry,
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg valueName = {FALSE, {NULL, 0, 0}};
    VReg__Buffer value = {NULL, 0, 0};
    VReg__Buffer text = {NULL, 0, 0};
    DWORD dwType;
    DWORD cbSize;
    DWORD cbText;
    LSTATUS sized;
    LSTATUS status;

    if ((lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    status = VReg__WideArgInit(pAllocContext, lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!lpData && !lpcbData)
    {
        status = VRegQueryValueExW(
            pRegistry,
            hKey,
            VReg__WideArgText(&valueName),
            NULL,
            lpType,
            NULL,
            NULL
        );
        goto done;
    }

    // Deliberately reads the _Out_ parameter: a failure below writes it back.
#pragma warning(suppress: 6001)
    dwType = lpType ? *lpType : 0;
    cbSize = (!lpData && lpcbData) ? *lpcbData : 0;
    sized = VRegQueryValueExW(
        pRegistry,
        hKey,
        VReg__WideArgText(&valueName),
        NULL,
        &dwType,
        NULL,
        &cbSize
    );
    if (sized != ERROR_SUCCESS)
    {
        if (lpType)
            *lpType = dwType;
        if (!lpData && lpcbData)
            *lpcbData = cbSize;
        status = sized;
        goto done;
    }

    if (!VReg__IsStringType(dwType))
    {
        status = VRegQueryValueExW(
            pRegistry,
            hKey,
            VReg__WideArgText(&valueName),
            NULL,
            lpType,
            lpData,
            lpcbData
        );
        goto done;
    }

    status = VReg__QueryValueOwned(
        pRegistry,
        hKey,
        VReg__WideArgText(&valueName),
        pAllocContext,
        &dwType,
        &value
    );
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__AnsiFromWide(pAllocContext, (const wchar_t*)value.pb, value.cb / sizeof(wchar_t), &text);
    if (status != ERROR_SUCCESS)
        goto done;

    cbText = (DWORD)text.cb;
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
            if (value.cb <= *lpcbData)
                VReg__CopyBytes(lpData, value.pb, value.cb);
            if (cbText > 0)
            {
                CopyMemory(lpData, text.pb, cbText);
                if (cbText < *lpcbData && lpData[cbText - 1] != 0)
                    lpData[cbText] = 0;
            }
        }
    }

    if (lpType)
        *lpType = dwType;
    if (lpcbData)
        *lpcbData = cbText;

done:
    VReg__BufferFree(pAllocContext, &text);
    VReg__BufferFree(pAllocContext, &value);
    VReg__WideArgFree(pAllocContext, &valueName);
    return status;}

LSTATUS WINAPI VRegSetValueExA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    if (pRegistry->lpVtbl->RegSetValueExA)
        return pRegistry->lpVtbl->RegSetValueExA(
            pRegistry,
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg valueName = {FALSE, {NULL, 0, 0}};
    VReg__WString text = {NULL, 0, 0};
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VReg__IsStringType(dwType) || !lpData)
    {
        status = VRegSetValueExW(
            pRegistry,
            hKey,
            VReg__WideArgText(&valueName),
            Reserved,
            dwType,
            lpData,
            cbData
        );
        goto done;
    }

    status = VReg__WideFromAnsi(pAllocContext, (const char*)lpData, cbData, &text);
    if (status != ERROR_SUCCESS)
        goto done;

    // Native reads the byte after unterminated data and keeps it as the
    // terminator when it is zero, which it nearly always is. Agent's choice:
    // always terminate, so no byte past cbData is ever read; reading it, as
    // native does, can fault at the end of a page.
    if (text.cch > 0 && text.psz[text.cch - 1] != L'\0')
    {
        if (!VReg__WStringAppendChar(pAllocContext, &text, L'\0'))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    status = VRegSetValueExW(
        pRegistry,
        hKey,
        VReg__WideArgText(&valueName),
        Reserved,
        dwType,
        (const BYTE*)VReg__WStringText(&text),
        (DWORD)(text.cch * sizeof(wchar_t))
    );

done:
    VReg__WStringFree(pAllocContext, &text);
    VReg__WideArgFree(pAllocContext, &valueName);
    return status;}

LSTATUS WINAPI VRegDeleteValueA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    if (pRegistry->lpVtbl->RegDeleteValueA)
        return pRegistry->lpVtbl->RegDeleteValueA(
            pRegistry,
            hKey,
            lpValueName
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg valueName = {FALSE, {NULL, 0, 0}};
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpValueName, &valueName);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegDeleteValueW(
        pRegistry,
        hKey,
        VReg__WideArgText(&valueName)
    );

done:
    VReg__WideArgFree(pAllocContext, &valueName);
    return status;}

LSTATUS WINAPI VRegDeleteKeyExA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    if (pRegistry->lpVtbl->RegDeleteKeyExA)
        return pRegistry->lpVtbl->RegDeleteKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg subKey = {FALSE, {NULL, 0, 0}};
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    // The annotation excludes a null subkey; one that is null anyway reaches
    // the wide form unchanged, which answers for it.
#pragma warning(suppress: 6387)
    status = VRegDeleteKeyExW(
        pRegistry,
        hKey,
        VReg__WideArgText(&subKey),
        samDesired,
        Reserved
    );

done:
    VReg__WideArgFree(pAllocContext, &subKey);
    return status;}

LSTATUS WINAPI VRegDeleteTreeA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey
)
{
    if (pRegistry->lpVtbl->RegDeleteTreeA)
        return pRegistry->lpVtbl->RegDeleteTreeA(
            pRegistry,
            hKey,
            lpSubKey
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg subKey = {FALSE, {NULL, 0, 0}};
    LSTATUS status = VReg__WideArgInit(pAllocContext, lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VRegDeleteTreeW(
        pRegistry,
        hKey,
        VReg__WideArgText(&subKey)
    );

done:
    VReg__WideArgFree(pAllocContext, &subKey);
    return status;}

LSTATUS WINAPI VRegEnumKeyExA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) const LPSTR lpName,
    _Inout_ const LPDWORD lpcchName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPSTR lpClass,
    _Inout_opt_ const LPDWORD lpcchClass,
    _Out_opt_ const PFILETIME lpftLastWriteTime
)
{
    if (pRegistry->lpVtbl->RegEnumKeyExA)
        return pRegistry->lpVtbl->RegEnumKeyExA(
            pRegistry,
            hKey,
            dwIndex,
            lpName,
            lpcchName,
            lpReserved,
            lpClass,
            lpcchClass,
            lpftLastWriteTime
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WString name = {NULL, 0, 0};
    VReg__WString className = {NULL, 0, 0};
    VReg__Buffer nameA = {NULL, 0, 0};
    VReg__Buffer classA = {NULL, 0, 0};
    FILETIME ftLastWrite;
    BOOL wantClass;
    DWORD cchName;
    DWORD cchClass;
    DWORD cchNameOut;
    DWORD cchClassOut;
    LSTATUS status;

    if (!lpcchName || (lpClass && !lpcchClass))
        return ERROR_INVALID_PARAMETER;

    wantClass = lpClass || lpcchClass;
    cchName = VREG__INITIAL_NAME_CHARS;
    cchClass = wantClass ? VREG__INITIAL_NAME_CHARS : 0;
    ZeroMemory(&ftLastWrite, sizeof(ftLastWrite));
    for (;;)
    {
        if (!VReg__WStringResize(pAllocContext, &name, cchName)
            || !VReg__WStringResize(pAllocContext, &className, cchClass))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        cchNameOut = cchName;
        cchClassOut = cchClass;
        status = VRegEnumKeyExW(
            pRegistry,
            hKey,
            dwIndex,
            name.psz,
            &cchNameOut,
            lpReserved,
            wantClass ? className.psz : NULL,
            wantClass ? &cchClassOut : NULL,
            &ftLastWrite
        );
        if (
            status != ERROR_MORE_DATA
            || (cchName >= VREG__MAX_CLASS_CHARS && cchClass >= VREG__MAX_CLASS_CHARS)
        )
            break;

        cchName = VReg__MinDword(cchName * 2, VREG__MAX_CLASS_CHARS);
        if (wantClass)
            cchClass = VReg__MinDword(cchClass * 2, VREG__MAX_CLASS_CHARS);
    }
    if (status != ERROR_SUCCESS)
        goto done;

    VReg__WStringTruncate(&name, cchNameOut);
    VReg__WStringTruncate(&className, cchClassOut);
    if (lpftLastWriteTime)
        *lpftLastWriteTime = ftLastWrite;

    status = VReg__AnsiFromWide(pAllocContext, VReg__WStringText(&name), name.cch, &nameA);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VReg__CopyAnsiName(&nameA, lpName, *lpcchName))
    {
        status = ERROR_MORE_DATA;
        goto done;
    }
    *lpcchName = (DWORD)nameA.cb;

    if (lpcchClass && !lpClass)
    {
        *lpcchClass = 0;
    }
    else if (lpcchClass)
    {
        status = VReg__AnsiFromWide(pAllocContext, VReg__WStringText(&className), className.cch, &classA);
        if (status != ERROR_SUCCESS)
            goto done;

        if (!VReg__CopyAnsiClass(&classA, lpClass, *lpcchClass))
        {
            status = ERROR_MORE_DATA;
            goto done;
        }
        *lpcchClass = (DWORD)classA.cb;
    }

    status = ERROR_SUCCESS;

done:
    VReg__BufferFree(pAllocContext, &classA);
    VReg__BufferFree(pAllocContext, &nameA);
    VReg__WStringFree(pAllocContext, &className);
    VReg__WStringFree(pAllocContext, &name);
    return status;}

LSTATUS WINAPI VRegEnumValueA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) const LPSTR lpValueName,
    _Inout_ const LPDWORD lpcchValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _Inout_opt_ const LPDWORD lpcbData
)
{
    if (pRegistry->lpVtbl->RegEnumValueA)
        return pRegistry->lpVtbl->RegEnumValueA(
            pRegistry,
            hKey,
            dwIndex,
            lpValueName,
            lpcchValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WString name = {NULL, 0, 0};
    VReg__Buffer value = {NULL, 0, 0};
    VReg__Buffer nameA = {NULL, 0, 0};
    VReg__Buffer text = {NULL, 0, 0};
    DWORD dwType = 0;
    DWORD cbValue;
    DWORD cbBuffer;
    BOOL isString;
    BOOL nameFits;
    LSTATUS status;

    if (!lpValueName || !lpcchValueName || (lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;

    status = VReg__EnumValueOwned(
        pRegistry,
        hKey,
        dwIndex,
        pAllocContext,
        &name,
        &dwType,
        lpcbData ? &value : NULL
    );
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__AnsiFromWide(pAllocContext, VReg__WStringText(&name), name.cch, &nameA);
    if (status != ERROR_SUCCESS)
        goto done;

    isString = VReg__IsStringType(dwType);
    cbValue = (DWORD)value.cb;
    if (lpcbData && isString)
    {
        status = VReg__AnsiFromWide(pAllocContext, (const wchar_t*)value.pb, value.cb / sizeof(wchar_t), &text);
        if (status != ERROR_SUCCESS)
            goto done;

        cbValue = (DWORD)text.cb;
    }

    // Native reports the type and size even when the call goes on to fail.
    if (lpType)
        *lpType = dwType;
    cbBuffer = lpcbData ? *lpcbData : 0;
    if (lpcbData)
        *lpcbData = cbValue;
    if (lpData && cbValue > cbBuffer)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    if (nameA.cb > *lpcchValueName)
    {
        // Native terminates the text in the caller's buffer here, and writes
        // nothing else.
        if (lpData && isString && cbValue > 0 && cbValue < cbBuffer)
            lpData[cbValue] = 0;
        status = ERROR_MORE_DATA;
        goto done;
    }

    nameFits = VReg__CopyAnsiName(&nameA, lpValueName, *lpcchValueName);
    if (lpData)
    {
        if (isString)
        {
            if (value.cb <= cbBuffer)
                VReg__CopyBytes(lpData, value.pb, value.cb);
            if (cbValue > 0)
            {
                CopyMemory(lpData, text.pb, cbValue);
                // The capacity is the caller's *lpcbData, which was overwritten
                // above; cbBuffer holds it.
#pragma warning(suppress: 6386)
                if (cbValue < cbBuffer && lpData[cbValue - 1] != 0)
                    lpData[cbValue] = 0;
            }
        }
        else
        {
            VReg__CopyBytes(lpData, value.pb, cbValue);
        }
    }

    if (!nameFits)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }
    *lpcchValueName = (DWORD)nameA.cb;
    status = ERROR_SUCCESS;

done:
    VReg__BufferFree(pAllocContext, &text);
    VReg__BufferFree(pAllocContext, &nameA);
    VReg__BufferFree(pAllocContext, &value);
    VReg__WStringFree(pAllocContext, &name);
    return status;}

// Native answers success without writing lpClass when only the class length is
// asked for, which the annotation's terminator guarantee does not allow for.
#pragma warning(suppress: 6054)
LSTATUS WINAPI VRegQueryInfoKeyA(
    _In_ VRegRegistry* const pRegistry,
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
)
{
    if (pRegistry->lpVtbl->RegQueryInfoKeyA)
        return pRegistry->lpVtbl->RegQueryInfoKeyA(
            pRegistry,
            hKey,
            lpClass,
            lpcchClass,
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

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WString className = {NULL, 0, 0};
    VReg__Buffer classA = {NULL, 0, 0};
    BOOL wantClass;
    BOOL wantClassLength;
    DWORD cchClass;
    DWORD cchClassOut = 0;
    LSTATUS status;

    if (lpClass && !lpcchClass)
        return ERROR_INVALID_PARAMETER;

    // Native answers success for a zero-length class buffer, without a class,
    // but reports the class length if the caller asked for the longest class.
    wantClass = lpClass && *lpcchClass != 0;
    wantClassLength = lpClass && !wantClass && lpcbMaxClassLen;
    if (!wantClass && !wantClassLength)
    {
        return VRegQueryInfoKeyW(
            pRegistry,
            hKey,
            NULL,
            lpClass ? NULL : lpcchClass,
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

    cchClass = VREG__INITIAL_NAME_CHARS;
    for (;;)
    {
        if (!VReg__WStringResize(pAllocContext, &className, cchClass))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        cchClassOut = cchClass;
        status = VRegQueryInfoKeyW(
            pRegistry,
            hKey,
            className.psz,
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
        if (status != ERROR_MORE_DATA || cchClass >= VREG__MAX_CLASS_CHARS)
            break;

        cchClass = VReg__MinDword(cchClass * 2, VREG__MAX_CLASS_CHARS);
    }
    if (status != ERROR_SUCCESS)
    {
        if (cchClassOut != cchClass)
            *lpcchClass = cchClassOut;
        goto done;
    }

    VReg__WStringTruncate(&className, cchClassOut);
    status = VReg__AnsiFromWide(pAllocContext, VReg__WStringText(&className), className.cch, &classA);
    if (status != ERROR_SUCCESS)
        goto done;

    if (wantClassLength)
    {
        *lpcchClass = (DWORD)classA.cb;
        status = ERROR_SUCCESS;
        goto done;
    }
    if (!VReg__CopyAnsiClass(&classA, lpClass, *lpcchClass))
    {
        *lpcchClass -= 1;
        status = ERROR_MORE_DATA;
        goto done;
    }
    *lpcchClass = (DWORD)classA.cb;
    status = ERROR_SUCCESS;

done:
    VReg__BufferFree(pAllocContext, &classA);
    VReg__WStringFree(pAllocContext, &className);
    return status;}

LSTATUS WINAPI VRegLoadMUIStringA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCSTR pszDirectory
)
{
    if (pRegistry->lpVtbl->RegLoadMUIStringA)
        return pRegistry->lpVtbl->RegLoadMUIStringA(
            pRegistry,
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );

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
LSTATUS WINAPI VRegGetValueA(
    _In_ VRegRegistry* const pRegistry,
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
)
{
    if (pRegistry->lpVtbl->RegGetValueA)
        return pRegistry->lpVtbl->RegGetValueA(
            pRegistry,
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__WideArg subKey = {FALSE, {NULL, 0, 0}};
    VReg__WideArg value = {FALSE, {NULL, 0, 0}};
    VReg__Buffer data = {NULL, 0, 0};
    VReg__Buffer stored = {NULL, 0, 0};
    VReg__Buffer text = {NULL, 0, 0};
    VReg__Buffer rawText = {NULL, 0, 0};
    VReg__Buffer storedText = {NULL, 0, 0};
    DWORD dwFlagsStored;
    DWORD dwFlagsTyped;
    DWORD dwFlagsAnyType;
    DWORD dwFlagsRead;
    DWORD dwStoredType;
    DWORD dwType;
    DWORD dwReadType;
    DWORD cbQuery;
    DWORD cbStoredQuery;
    DWORD cbStoredRaw;
    DWORD cbStored;
    DWORD cbText;
    DWORD cbRawText;
    DWORD cbExpanded;
    DWORD cbReported;
    DWORD cbBuffer;
    DWORD cbNeeded;
    SIZE_T ibNul;
    BYTE* pbData;
    BOOL isMismatch;
    BOOL isExpanded;
    LSTATUS statusStoredType;
    LSTATUS statusSized;
    LSTATUS statusMismatch;
    LSTATUS status;

    if (pvData && !pcbData)
        return ERROR_INVALID_PARAMETER;

    status = VReg__WideArgInit(pAllocContext, lpSubKey, &subKey);
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__WideArgInit(pAllocContext, lpValue, &value);
    if (status != ERROR_SUCCESS)
        goto done;

    // Only string values are converted; anything else, and every failure to
    // find the value, is what the wide form answers.
    dwFlagsStored =
        (dwFlags & VREG__RRF_WOW_SUBKEYS)
        | RRF_RT_ANY
        | RRF_NOEXPAND;
    dwStoredType = 0;
    statusStoredType = VRegGetValueW(
        pRegistry,
        hkey,
        VReg__WideArgText(&subKey),
        VReg__WideArgText(&value),
        dwFlagsStored,
        &dwStoredType,
        NULL,
        NULL
    );
    if (statusStoredType != ERROR_SUCCESS || !VReg__IsStringType(dwStoredType))
        goto forward;

    // A value whose type the flags reject is still read and reported, with the
    // failure status.
    dwFlagsTyped = dwFlags & ~RRF_ZEROONFAILURE;
    dwFlagsAnyType =
        (dwFlags & (VREG__RRF_WOW_SUBKEYS | RRF_NOEXPAND))
        | RRF_RT_ANY;
    dwType = 0;
    cbQuery = 0;
    statusSized = VRegGetValueW(
        pRegistry,
        hkey,
        VReg__WideArgText(&subKey),
        VReg__WideArgText(&value),
        dwFlagsTyped,
        &dwType,
        NULL,
        &cbQuery
    );
    isMismatch = statusSized == ERROR_UNSUPPORTED_TYPE;
    if (statusSized != ERROR_SUCCESS && !isMismatch)
        goto forward;

    dwFlagsRead = isMismatch ? dwFlagsAnyType : dwFlagsTyped;
    if (isMismatch)
    {
        VRegGetValueW(
            pRegistry,
            hkey,
            VReg__WideArgText(&subKey),
            VReg__WideArgText(&value),
            dwFlagsAnyType,
            &dwType,
            NULL,
            &cbQuery
        );
    }

    status = VReg__GetValueOwned(
        pRegistry,
        hkey,
        VReg__WideArgText(&subKey),
        VReg__WideArgText(&value),
        dwFlagsRead,
        pAllocContext,
        &dwType,
        &data
    );
    if (status != ERROR_SUCCESS)
        goto done;

    isExpanded =
        dwStoredType == REG_EXPAND_SZ
        && !(dwFlags & RRF_NOEXPAND);
    cbStoredRaw = 0;
    if (isExpanded)
    {
        dwReadType = 0;
        cbStoredQuery = 0;
        status = VReg__GetValueOwned(
            pRegistry,
            hkey,
            VReg__WideArgText(&subKey),
            VReg__WideArgText(&value),
            dwFlagsStored,
            pAllocContext,
            &dwReadType,
            &stored
        );
        if (status != ERROR_SUCCESS)
            goto done;

        VRegGetValueW(
            pRegistry,
            hkey,
            VReg__WideArgText(&subKey),
            VReg__WideArgText(&value),
            dwFlagsStored,
            &dwReadType,
            NULL,
            &cbStoredQuery
        );
        cbStoredRaw = cbStoredQuery - VReg__MinDword(cbStoredQuery, 2);
    }

    status = VReg__AnsiFromWide(pAllocContext, (const wchar_t*)data.pb, data.cb / sizeof(wchar_t), &text);
    if (status != ERROR_SUCCESS)
        goto done;

    cbText = (DWORD)text.cb;
    // What a call that cannot return the text reports as its size: room for the
    // terminator the wide form reserves; for an expanded value, the larger of
    // the unexpanded text and the expansion, and a character more when the
    // expansion is the larger.
    cbReported = cbText + (
        cbQuery > data.cb
            ? (DWORD)((cbQuery - data.cb) / sizeof(wchar_t))
            : 0
    );
    if (isExpanded)
    {
        status = VReg__AnsiFromWide(
            pAllocContext,
            (const wchar_t*)stored.pb,
            cbStoredRaw / sizeof(wchar_t),
            &rawText
        );
        if (status != ERROR_SUCCESS)
            goto done;

        cbRawText = (DWORD)rawText.cb;
        ibNul = 0;
        while (ibNul < text.cb && text.pb[ibNul] != 0)
            ++ibNul;
        cbExpanded = ibNul == text.cb ? cbText : (DWORD)(ibNul + 1);
        cbReported = VReg__MaxDword(cbRawText, cbExpanded) + (cbExpanded > cbRawText ? 1 : 0);
    }
    cbBuffer = pcbData ? *pcbData : 0;
    cbNeeded = isExpanded ? cbText + 1 : cbText;
    statusMismatch = isMismatch ? statusSized : ERROR_SUCCESS;

    if (pdwType)
        *pdwType = dwType;

    if (!pvData)
    {
        if (pcbData)
            *pcbData = cbReported;
        status = statusMismatch;
        goto done;
    }

    if (cbBuffer < cbNeeded)
    {
        *pcbData = cbReported;
        if (isExpanded)
        {
            // Native copies the unexpanded text, then clears its first
            // character.
            status = VReg__AnsiFromWide(
                pAllocContext,
                (const wchar_t*)stored.pb,
                stored.cb / sizeof(wchar_t),
                &storedText
            );
            if (status != ERROR_SUCCESS)
                goto done;

            if (storedText.cb > 0 && storedText.cb <= cbBuffer)
            {
                if (cbStoredRaw <= cbBuffer)
                    VReg__CopyBytes(pvData, stored.pb, VReg__MinSize(cbStoredRaw, stored.cb));
                CopyMemory(pvData, storedText.pb, storedText.cb);
                *(char*)pvData = '\0';
                *pcbData = cbNeeded;
            }
        }
        if ((dwFlags & RRF_ZEROONFAILURE) && cbBuffer > 0)
            ZeroMemory(pvData, cbBuffer);
        status = isMismatch ? statusSized : ERROR_MORE_DATA;
        goto done;
    }

    // Native reads the stored wide data into the caller's buffer when it fits
    // there and narrows it in place, so the bytes past the text are the tail of
    // the stored data. The wide form reports two bytes (four for a
    // multi-string) more than are stored.
    pbData = (BYTE*)pvData;
    if (isExpanded)
    {
        if (cbStoredRaw <= cbBuffer)
            VReg__CopyBytes(pbData, stored.pb, VReg__MinSize(cbStoredRaw, stored.cb));
    }
    else
    {
        cbStored = cbQuery - VReg__MinDword(cbQuery, dwType == REG_MULTI_SZ ? 4 : 2);
        if (cbStored <= cbBuffer)
            VReg__CopyBytes(pbData, data.pb, VReg__MinSize(cbStored, data.cb));
    }
    if (cbText > 0)
    {
        CopyMemory(pbData, text.pb, cbText);
        if (cbText < cbBuffer && pbData[cbText - 1] != 0)
            pbData[cbText] = 0;
    }
    *pcbData = cbText;

    if (isMismatch && (dwFlags & RRF_ZEROONFAILURE) && cbBuffer > 0)
        ZeroMemory(pvData, cbBuffer);
    status = statusMismatch;
    goto done;

forward:
    status = VRegGetValueW(
        pRegistry,
        hkey,
        VReg__WideArgText(&subKey),
        VReg__WideArgText(&value),
        dwFlags,
        pdwType,
        pvData,
        pcbData
    );

done:
    VReg__BufferFree(pAllocContext, &storedText);
    VReg__BufferFree(pAllocContext, &rawText);
    VReg__BufferFree(pAllocContext, &text);
    VReg__BufferFree(pAllocContext, &stored);
    VReg__BufferFree(pAllocContext, &data);
    VReg__WideArgFree(pAllocContext, &value);
    VReg__WideArgFree(pAllocContext, &subKey);
    return status;}

LSTATUS WINAPI VRegOpenKeyA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (pRegistry->lpVtbl->RegOpenKeyA)
        return pRegistry->lpVtbl->RegOpenKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VRegOpenKeyExA(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );}

LSTATUS WINAPI VRegOpenKeyW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (pRegistry->lpVtbl->RegOpenKeyW)
        return pRegistry->lpVtbl->RegOpenKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VRegOpenKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        MAXIMUM_ALLOWED,
        phkResult
    );}

LSTATUS WINAPI VRegCreateKeyA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (pRegistry->lpVtbl->RegCreateKeyA)
        return pRegistry->lpVtbl->RegCreateKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!VRegIsPredefinedHKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VRegCreateKeyExA(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        MAXIMUM_ALLOWED,
        NULL,
        phkResult,
        NULL
    );}

LSTATUS WINAPI VRegCreateKeyW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_ const PHKEY phkResult
)
{
    if (pRegistry->lpVtbl->RegCreateKeyW)
        return pRegistry->lpVtbl->RegCreateKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    if (!lpSubKey || !*lpSubKey)
    {
        if (!VRegIsPredefinedHKey(hKey))
            return ERROR_BADKEY;
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }
    return VRegCreateKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        MAXIMUM_ALLOWED,
        NULL,
        phkResult,
        NULL
    );}

LSTATUS WINAPI VRegQueryValueA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    if (pRegistry->lpVtbl->RegQueryValueA)
        return pRegistry->lpVtbl->RegQueryValueA(
            pRegistry,
            hKey,
            lpSubKey,
            lpData,
            lpcbData
        );

    HKEY hSubKey = hKey;
    DWORD dwType = 0;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegOpenKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegQueryValueExA(
        pRegistry,
        hSubKey,
        NULL,
        NULL,
        &dwType,
        (LPBYTE)lpData,
        (LPDWORD)lpcbData
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
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
    return status;}

LSTATUS WINAPI VRegQueryValueW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPWSTR lpData,
    _Inout_opt_ const PLONG lpcbData
)
{
    if (pRegistry->lpVtbl->RegQueryValueW)
        return pRegistry->lpVtbl->RegQueryValueW(
            pRegistry,
            hKey,
            lpSubKey,
            lpData,
            lpcbData
        );

    HKEY hSubKey = hKey;
    DWORD dwType = 0;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegOpenKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegQueryValueExW(
        pRegistry,
        hSubKey,
        NULL,
        NULL,
        &dwType,
        (LPBYTE)lpData,
        (LPDWORD)lpcbData
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
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
    return status;}

LSTATUS WINAPI VRegSetValueA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCSTR lpData,
    _In_ const DWORD cbData
)
{
    if (pRegistry->lpVtbl->RegSetValueA)
        return pRegistry->lpVtbl->RegSetValueA(
            pRegistry,
            hKey,
            lpSubKey,
            dwType,
            lpData,
            cbData
        );

    HKEY hSubKey = hKey;
    LSTATUS status;

    // Native ignores the size and stores the string's own length.
    (void)cbData;
    if (dwType != REG_SZ || !lpData)
        return ERROR_INVALID_PARAMETER;

    if (lpSubKey && *lpSubKey)
    {
        status = VRegCreateKeyA(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegSetValueExA(
        pRegistry,
        hSubKey,
        NULL,
        0,
        REG_SZ,
        (const BYTE*)lpData,
        (DWORD)(VReg__AnsiLength(lpData) + 1)
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

LSTATUS WINAPI VRegSetValueW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCWSTR lpData,
    _In_ const DWORD cbData
)
{
    if (pRegistry->lpVtbl->RegSetValueW)
        return pRegistry->lpVtbl->RegSetValueW(
            pRegistry,
            hKey,
            lpSubKey,
            dwType,
            lpData,
            cbData
        );

    HKEY hSubKey = hKey;
    LSTATUS status;

    // Native ignores the size and stores the string's own length.
    (void)cbData;
    if (dwType != REG_SZ || !lpData)
        return ERROR_INVALID_PARAMETER;

    if (lpSubKey && *lpSubKey)
    {
        status = VRegCreateKeyW(
            pRegistry,
            hKey,
            lpSubKey,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegSetValueExW(
        pRegistry,
        hSubKey,
        NULL,
        0,
        REG_SZ,
        (const BYTE*)lpData,
        (DWORD)((VReg__ViewOf(lpData).cch + 1) * sizeof(wchar_t))
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

LSTATUS WINAPI VRegDeleteKeyA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCSTR lpSubKey
)
{
    if (pRegistry->lpVtbl->RegDeleteKeyA)
        return pRegistry->lpVtbl->RegDeleteKeyA(
            pRegistry,
            hKey,
            lpSubKey
        );

    return VRegDeleteKeyExA(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        0
    );}

LSTATUS WINAPI VRegDeleteKeyW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey
)
{
    if (pRegistry->lpVtbl->RegDeleteKeyW)
        return pRegistry->lpVtbl->RegDeleteKeyW(
            pRegistry,
            hKey,
            lpSubKey
        );

    return VRegDeleteKeyExW(
        pRegistry,
        hKey,
        lpSubKey,
        0,
        0
    );}

LSTATUS WINAPI VRegEnumKeyA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPSTR lpName,
    _In_ const DWORD cchName
)
{
    if (pRegistry->lpVtbl->RegEnumKeyA)
        return pRegistry->lpVtbl->RegEnumKeyA(
            pRegistry,
            hKey,
            dwIndex,
            lpName,
            cchName
        );

    DWORD cchNameInOut = cchName;
    return VRegEnumKeyExA(
        pRegistry,
        hKey,
        dwIndex,
        lpName,
        &cchNameInOut,
        NULL,
        NULL,
        NULL,
        NULL
    );}

LSTATUS WINAPI VRegEnumKeyW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_opt_(cchName) const LPWSTR lpName,
    _In_ const DWORD cchName
)
{
    if (pRegistry->lpVtbl->RegEnumKeyW)
        return pRegistry->lpVtbl->RegEnumKeyW(
            pRegistry,
            hKey,
            dwIndex,
            lpName,
            cchName
        );

    DWORD cchNameInOut = cchName;
    return VRegEnumKeyExW(
        pRegistry,
        hKey,
        dwIndex,
        lpName,
        &cchNameInOut,
        NULL,
        NULL,
        NULL,
        NULL
    );}

// The subkey of SetKeyValue and DeleteKeyValue is opened with KEY_SET_VALUE,
// the one right the call needs; the maximum allowed access, as the pre-Ex
// forms use, was passed over. Only a caller whose access is limited to
// something else could tell.
LSTATUS WINAPI VRegSetKeyValueA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    if (pRegistry->lpVtbl->RegSetKeyValueA)
        return pRegistry->lpVtbl->RegSetKeyValueA(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName,
            dwType,
            lpData,
            cbData
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegCreateKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            0,
            NULL,
            0,
            KEY_SET_VALUE,
            NULL,
            &hSubKey,
            NULL
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegSetValueExA(
        pRegistry,
        hSubKey,
        lpValueName,
        0,
        dwType,
        (const BYTE*)lpData,
        cbData
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

LSTATUS WINAPI VRegSetKeyValueW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const LPCVOID lpData,
    _In_ const DWORD cbData
)
{
    if (pRegistry->lpVtbl->RegSetKeyValueW)
        return pRegistry->lpVtbl->RegSetKeyValueW(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName,
            dwType,
            lpData,
            cbData
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegCreateKeyExW(
            pRegistry,
            hKey,
            lpSubKey,
            0,
            NULL,
            0,
            KEY_SET_VALUE,
            NULL,
            &hSubKey,
            NULL
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegSetValueExW(
        pRegistry,
        hSubKey,
        lpValueName,
        0,
        dwType,
        (const BYTE*)lpData,
        cbData
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

LSTATUS WINAPI VRegDeleteKeyValueA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCSTR lpSubKey,
    _In_opt_ const LPCSTR lpValueName
)
{
    if (pRegistry->lpVtbl->RegDeleteKeyValueA)
        return pRegistry->lpVtbl->RegDeleteKeyValueA(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegOpenKeyExA(
            pRegistry,
            hKey,
            lpSubKey,
            0,
            KEY_SET_VALUE,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegDeleteValueA(
        pRegistry,
        hSubKey,
        lpValueName
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

LSTATUS WINAPI VRegDeleteKeyValueW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    if (pRegistry->lpVtbl->RegDeleteKeyValueW)
        return pRegistry->lpVtbl->RegDeleteKeyValueW(
            pRegistry,
            hKey,
            lpSubKey,
            lpValueName
        );

    HKEY hSubKey = hKey;
    LSTATUS status;
    if (lpSubKey && *lpSubKey)
    {
        status = VRegOpenKeyExW(
            pRegistry,
            hKey,
            lpSubKey,
            0,
            KEY_SET_VALUE,
            &hSubKey
        );
        if (status != ERROR_SUCCESS)
            return status;
    }

    status = VRegDeleteValueW(
        pRegistry,
        hSubKey,
        lpValueName
    );
    if (hSubKey != hKey)
    {
        VRegCloseKey(
            pRegistry,
            hSubKey
        );
    }
    return status;}

/// Places the narrowed values like the wide form places the stored ones, but
/// leaves the gaps alone and rounds the total up. When they do not all fit,
/// native answers the wide form's total for the stored values.
// The SDK types lpValueBuf as a string, but it holds binary values.
#pragma warning(suppress: 6054)
LSTATUS WINAPI VRegQueryMultipleValuesA(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTA val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    if (pRegistry->lpVtbl->RegQueryMultipleValuesA)
        return pRegistry->lpVtbl->RegQueryMultipleValuesA(
            pRegistry,
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__PlacedValue* pPlaced = NULL;
    VReg__WideArg name = {FALSE, {NULL, 0, 0}};
    BYTE* pbBuffer;
    DWORD cbBuffer;
    DWORD cbEnd = 0;
    DWORD cbWideEnd;
    DWORD cbWide;
    DWORD cbRead;
    DWORD i;
    LSTATUS status;

    if (!val_list || !ldwTotsize)
        return ERROR_INVALID_PARAMETER;
    if (!lpValueBuf)
        return ERROR_NOACCESS;

    cbBuffer = *ldwTotsize;
    pbBuffer = (BYTE*)lpValueBuf;
    if (num_vals > 0)
    {
        pPlaced = (VReg__PlacedValue*)VReg__AllocZeroed(pAllocContext, (SIZE_T)num_vals * sizeof(*pPlaced));
        if (!pPlaced)
            return ERROR_OUTOFMEMORY;
    }

    for (i = 0; i < num_vals; ++i)
    {
        status = VRegQueryValueExA(
            pRegistry,
            hKey,
            val_list[i].ve_valuename,
            NULL,
            NULL,
            NULL,
            &pPlaced[i].cbValue
        );
        if (status != ERROR_SUCCESS)
            goto done;

        pPlaced[i].ibValue = i == 0 ? 0 : VReg__AlignedToDword(cbEnd);
        cbEnd = pPlaced[i].ibValue + pPlaced[i].cbValue;
    }

    if (cbEnd > cbBuffer)
    {
        cbWideEnd = 0;
        for (i = 0; i < num_vals; ++i)
        {
            cbWide = 0;
            status = VReg__WideArgInit(pAllocContext, val_list[i].ve_valuename, &name);
            if (status == ERROR_SUCCESS)
            {
                status = VRegQueryValueExW(
                    pRegistry,
                    hKey,
                    VReg__WideArgText(&name),
                    NULL,
                    NULL,
                    NULL,
                    &cbWide
                );
            }
            VReg__WideArgFree(pAllocContext, &name);
            if (status != ERROR_SUCCESS)
                goto done;

            cbWideEnd = (i == 0 ? 0 : VReg__AlignedToDword(cbWideEnd)) + cbWide;
        }
        *ldwTotsize = cbWideEnd;
        status = ERROR_MORE_DATA;
        goto done;
    }

    for (i = 0; i < num_vals; ++i)
    {
        cbRead = pPlaced[i].cbValue;
        status = VRegQueryValueExA(
            pRegistry,
            hKey,
            val_list[i].ve_valuename,
            NULL,
            &pPlaced[i].dwType,
            pbBuffer + pPlaced[i].ibValue,
            &cbRead
        );
        if (status != ERROR_SUCCESS)
            goto done;

        pPlaced[i].cbValue = cbRead;
    }

    for (i = 0; i < num_vals; ++i)
    {
        val_list[i].ve_valuelen = pPlaced[i].cbValue;
        val_list[i].ve_type = pPlaced[i].dwType;
        val_list[i].ve_valueptr = (DWORD_PTR)(pbBuffer + pPlaced[i].ibValue);
    }
    *ldwTotsize = VReg__AlignedToDword(cbEnd);
    status = ERROR_SUCCESS;

done:
    VReg__Free(pAllocContext, pPlaced);
    return status;}

/// Native places each value on a @ref DWORD boundary, reads the ones that fit
/// the buffer in order, and fills the entries only when all of them did.
LSTATUS WINAPI VRegQueryMultipleValuesW(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _Out_writes_(num_vals) const PVALENTW val_list,
    _In_ const DWORD num_vals,
    _Out_writes_bytes_to_opt_(*ldwTotsize, *ldwTotsize) __out_data_source(REGISTRY) const LPWSTR lpValueBuf,
    _Inout_opt_ const LPDWORD ldwTotsize
)
{
    if (pRegistry->lpVtbl->RegQueryMultipleValuesW)
        return pRegistry->lpVtbl->RegQueryMultipleValuesW(
            pRegistry,
            hKey,
            val_list,
            num_vals,
            lpValueBuf,
            ldwTotsize
        );

    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__PlacedValue* pPlaced = NULL;
    BYTE* pbBuffer;
    DWORD cbBuffer;
    DWORD cbEnd = 0;
    DWORD cbValue;
    DWORD cbRead;
    DWORD ibValue;
    DWORD i;
    BOOL isComplete = TRUE;
    LSTATUS status;

    if (!val_list || !ldwTotsize)
        return ERROR_INVALID_PARAMETER;

    cbBuffer = *ldwTotsize;
    *ldwTotsize = 0;
    if (!lpValueBuf)
        return ERROR_NOACCESS;

    pbBuffer = (BYTE*)lpValueBuf;
    if (num_vals > 0)
    {
        pPlaced = (VReg__PlacedValue*)VReg__AllocZeroed(pAllocContext, (SIZE_T)num_vals * sizeof(*pPlaced));
        if (!pPlaced)
            return ERROR_OUTOFMEMORY;
    }

    for (i = 0; i < num_vals; ++i)
    {
        status = VRegQueryValueExW(
            pRegistry,
            hKey,
            val_list[i].ve_valuename,
            NULL,
            NULL,
            NULL,
            &pPlaced[i].cbValue
        );
        if (status != ERROR_SUCCESS)
        {
            if (status == ERROR_INVALID_HANDLE)
                *ldwTotsize = cbBuffer;
            goto done;
        }
    }

    for (i = 0; i < num_vals; ++i)
    {
        cbValue = pPlaced[i].cbValue;
        ibValue = i == 0 ? 0 : VReg__AlignedToDword(cbEnd);
        if (ibValue > cbEnd && cbEnd < cbBuffer)
            ZeroMemory(pbBuffer + cbEnd, VReg__MinDword(ibValue, cbBuffer) - cbEnd);

        pPlaced[i].ibValue = ibValue;
        if (ibValue + cbValue <= cbBuffer)
        {
            cbRead = cbValue;
            status = VRegQueryValueExW(
                pRegistry,
                hKey,
                val_list[i].ve_valuename,
                NULL,
                &pPlaced[i].dwType,
                pbBuffer + ibValue,
                &cbRead
            );
            if (status != ERROR_SUCCESS)
                goto done;

            pPlaced[i].cbValue = cbRead;
        }
        else
        {
            isComplete = FALSE;
        }
        cbEnd = ibValue + cbValue;
    }

    *ldwTotsize = cbEnd;
    if (!isComplete)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    for (i = 0; i < num_vals; ++i)
    {
        val_list[i].ve_valuelen = pPlaced[i].cbValue;
        val_list[i].ve_type = pPlaced[i].dwType;
        val_list[i].ve_valueptr = (DWORD_PTR)(pbBuffer + pPlaced[i].ibValue);
    }
    status = ERROR_SUCCESS;

done:
    VReg__Free(pAllocContext, pPlaced);
    return status;
}
