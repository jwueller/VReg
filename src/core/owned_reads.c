#include "core/owned_reads.h"

#include "core/retry.h"

VREGDEF DWORD VReg__Grown(const DWORD cbOld, const DWORD cbRequired)
{
    return cbRequired > cbOld ? cbRequired : cbOld * 2 + 16;
}

VREGDEF LSTATUS VReg__QueryValueOwned(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_z_ const LPCWSTR lpValueName,
    _In_opt_ void* const pAllocContext,
    _Out_opt_ const LPDWORD lpType,
    _Inout_ VReg__Buffer* const pData
)
{
    DWORD dwType = 0;
    DWORD cbData = 0;
    const LSTATUS sized = VRegQueryValueExW(
        pRegistry,
        hKey,
        lpValueName,
        NULL,
        &dwType,
        NULL,
        &cbData
    );
    if (sized != ERROR_SUCCESS)
        return sized;

    for (;;)
    {
        DWORD cbRead = cbData;
        LSTATUS status;
        if (!VReg__BufferResize(pAllocContext, pData, cbData))
            return ERROR_OUTOFMEMORY;

        status = VRegQueryValueExW(
            pRegistry,
            hKey,
            lpValueName,
            NULL,
            &dwType,
            pData->pb,
            &cbRead
        );
        if (status == ERROR_MORE_DATA)
        {
            cbData = VReg__Grown(cbData, cbRead);
            continue;
        }
        if (status == ERROR_SUCCESS)
        {
            pData->cb = cbRead;
            if (lpType)
                *lpType = dwType;
        }
        return status;
    }
}

VREGDEF LSTATUS VReg__GetValueOwned(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_opt_z_ const LPCWSTR lpSubKey,
    _In_opt_z_ const LPCWSTR lpValue,
    _In_ const DWORD dwFlags,
    _In_opt_ void* const pAllocContext,
    _Out_opt_ const LPDWORD pdwType,
    _Inout_ VReg__Buffer* const pData
)
{
    DWORD dwType = 0;
    DWORD cbData = 0;
    const LSTATUS sized = VRegGetValueW(
        pRegistry,
        hKey,
        lpSubKey,
        lpValue,
        dwFlags,
        &dwType,
        NULL,
        &cbData
    );
    if (sized != ERROR_SUCCESS)
        return sized;

    for (;;)
    {
        DWORD cbRead = cbData;
        LSTATUS status;
        if (!VReg__BufferResize(pAllocContext, pData, cbData))
            return ERROR_OUTOFMEMORY;

        status = VRegGetValueW(
            pRegistry,
            hKey,
            lpSubKey,
            lpValue,
            dwFlags,
            &dwType,
            pData->pb,
            &cbRead
        );
        if (status == ERROR_MORE_DATA)
        {
            cbData = VReg__Grown(cbData, cbRead);
            continue;
        }
        if (status == ERROR_SUCCESS)
        {
            pData->cb = cbRead;
            if (pdwType)
                *pdwType = dwType;
        }
        return status;
    }
}

VREGDEF LSTATUS VReg__EnumKeyOwned(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pName
)
{
    DWORD cchName = VREG__INITIAL_NAME_CHARS;
    for (;;)
    {
        DWORD cchRead = cchName;
        LSTATUS status;
        if (!VReg__WStringResize(pAllocContext, pName, cchName))
            return ERROR_OUTOFMEMORY;

        status = VRegEnumKeyExW(
            pRegistry,
            hKey,
            dwIndex,
            pName->psz,
            &cchRead,
            NULL,
            NULL,
            NULL,
            NULL
        );
        if (status == ERROR_MORE_DATA && cchName < VREG__MAX_CLASS_CHARS)
        {
            cchName = VReg__MinDword(VReg__Grown(cchName, cchRead), VREG__MAX_CLASS_CHARS);
            continue;
        }
        if (status == ERROR_SUCCESS)
            VReg__WStringTruncate(pName, cchRead);
        return status;
    }
}

VREGDEF LSTATUS VReg__EnumValueOwned(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pName,
    _Out_opt_ const LPDWORD lpType,
    _Inout_opt_ VReg__Buffer* const pData
)
{
    DWORD cchName = VREG__INITIAL_NAME_CHARS;
    DWORD cbData = 0;
    BOOL isSized = pData == NULL;
    for (;;)
    {
        DWORD cchRead = cchName;
        DWORD cbRead = cbData;
        DWORD dwType = 0;
        LSTATUS status;
        if (!VReg__WStringResize(pAllocContext, pName, cchName))
            return ERROR_OUTOFMEMORY;

        status = VRegEnumValueW(
            pRegistry,
            hKey,
            dwIndex,
            pName->psz,
            &cchRead,
            NULL,
            &dwType,
            (pData && isSized) ? pData->pb : NULL,
            pData ? &cbRead : NULL
        );
        if (status == ERROR_MORE_DATA)
        {
            cchName = VReg__MinDword(VReg__Grown(cchName, cchRead + 1), VREG__MAX_VALUE_NAME_CHARS);
            if (pData)
            {
                cbData = VReg__Grown(cbData, cbRead);
                if (isSized && !VReg__BufferResize(pAllocContext, pData, cbData))
                    return ERROR_OUTOFMEMORY;
            }
            continue;
        }
        if (status == ERROR_SUCCESS && !isSized)
        {
            cbData = cbRead;
            if (!VReg__BufferResize(pAllocContext, pData, cbData))
                return ERROR_OUTOFMEMORY;

            isSized = TRUE;
            continue;
        }
        if (status == ERROR_SUCCESS)
        {
            VReg__WStringTruncate(pName, cchRead);
            if (pData)
                pData->cb = cbRead;
            if (lpType)
                *lpType = dwType;
        }
        return status;
    }
}
