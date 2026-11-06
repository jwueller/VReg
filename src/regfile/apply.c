#include "regfile/script.h"

#include "core/alloc.h"
#include "core/value_type.h"

static LSTATUS VReg__NotFoundIsFine(const LSTATUS status)
{
    return status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND ? ERROR_SUCCESS : status;
}

static void VReg__ApplyCloseKey(_In_ VRegRegistry* const pRegistry, _In_opt_ const HKEY hKey)
{
    if (hKey)
        VRegCloseKey(
            pRegistry,
            hKey
        );
}

static LSTATUS VReg__PerformSetValue(_In_ const VRegOperation* const pOperation, _In_ VRegRegistry* const pRegistry)
{
    const BYTE* const pData = pOperation->pData;
    const DWORD cbData = pOperation->cbData;
    const BOOL isString = VReg__IsStringType(pOperation->dwType);
    const BOOL isTerminated = cbData >= sizeof(wchar_t) && pData[cbData - 1] == 0 && pData[cbData - 2] == 0;
    HKEY hKey = NULL;
    LSTATUS status = VRegOpenKeyExW(
        pRegistry,
        pOperation->hRoot,
        pOperation->pszPath,
        0,
        KEY_SET_VALUE,
        &hKey
    );
    if (status != ERROR_SUCCESS)
        return status;

    // Native reads past unterminated string data for a terminator and may
    // store one more wchar if it finds a zero there, so such data goes in
    // with nonzero bytes after it to store exactly what the file says.
    if (isString && !isTerminated)
    {
        static const BYTE PADDING[4] = {0x11, 0x11, 0x11, 0x11};
        VReg__Buffer padded = {NULL, 0, 0};
        if (
            !VReg__BufferAssign(pRegistry->pAllocContext, &padded, pData, cbData)
            || !VReg__BufferAppend(pRegistry->pAllocContext, &padded, PADDING, sizeof(PADDING))
        )
        {
            status = ERROR_OUTOFMEMORY;
        }
        else
        {
            status = VRegSetValueExW(
                pRegistry,
                hKey,
                pOperation->pszName,
                0,
                pOperation->dwType,
                padded.pb,
                cbData
            );
        }

        VReg__BufferFree(pRegistry->pAllocContext, &padded);
    }
    else
    {
        status = VRegSetValueExW(
            pRegistry,
            hKey,
            pOperation->pszName,
            0,
            pOperation->dwType,
            pData,
            cbData
        );
    }

    VReg__ApplyCloseKey(pRegistry, hKey);
    return status;
}

static LSTATUS VReg__Perform(_In_ const VRegOperation* const pOperation, _In_ VRegRegistry* const pRegistry)
{
    HKEY hKey = NULL;
    LSTATUS status;

    // A script can be built by hand, so the parser's guarantee is checked
    // again: an empty path would delete a whole root.
    if (pOperation->kind == VREG_OPERATION_DELETE_KEY && pOperation->pszPath[0] == L'\0')
        return ERROR_INVALID_PARAMETER;

    switch (pOperation->kind)
    {
    case VREG_OPERATION_CREATE_KEY:
        status = VRegCreateKeyExW(
            pRegistry,
            pOperation->hRoot,
            pOperation->pszPath,
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_CREATE_SUB_KEY,
            NULL,
            &hKey,
            NULL
        );
        VReg__ApplyCloseKey(pRegistry, hKey);
        return status;

    case VREG_OPERATION_DELETE_KEY:
        status = VReg__NotFoundIsFine(
            VRegDeleteTreeW(
                pRegistry,
                pOperation->hRoot,
                pOperation->pszPath
            )
        );
        if (status != ERROR_SUCCESS)
            return status;

        return VReg__NotFoundIsFine(
            VRegDeleteKeyExW(
                pRegistry,
                pOperation->hRoot,
                pOperation->pszPath,
                0,
                0
            )
        );

    case VREG_OPERATION_SET_VALUE:
        return VReg__PerformSetValue(pOperation, pRegistry);

    case VREG_OPERATION_DELETE_VALUE:
        status = VRegOpenKeyExW(
            pRegistry,
            pOperation->hRoot,
            pOperation->pszPath,
            0,
            KEY_SET_VALUE,
            &hKey
        );
        if (status != ERROR_SUCCESS)
            return VReg__NotFoundIsFine(status);

        status = VReg__NotFoundIsFine(
            VRegDeleteValueW(
                pRegistry,
                hKey,
                pOperation->pszName
            )
        );
        VReg__ApplyCloseKey(pRegistry, hKey);
        return status;

    default:
        return ERROR_INVALID_PARAMETER;
    }
}

_Success_(return == ERROR_SUCCESS)
VREGDEF LSTATUS WINAPI VRegApplyOperations(
    _In_reads_(cOperations) const VRegOperation* const pOperations,
    _In_ const SIZE_T cOperations,
    _In_ VRegRegistry* const pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* const pFailedIndex
)
{
    SIZE_T i;
    for (i = 0; i < cOperations; ++i)
    {
        const LSTATUS status = VReg__Perform(&pOperations[i], pRegistry);
        if (status != ERROR_SUCCESS)
        {
            if (pFailedIndex)
                *pFailedIndex = i;
            return status;
        }
    }

    return ERROR_SUCCESS;
}

_Success_(return == ERROR_SUCCESS)
VREGDEF LSTATUS WINAPI VRegApplyScript(
    _In_ const VRegScript* const pScript,
    _In_ VRegRegistry* const pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* const pFailedIndex
)
{
    const VRegOperation* const pOperations = (const VRegOperation*)pScript->operations.p;
    return VRegApplyOperations(pOperations, pScript->operations.count, pRegistry, pFailedIndex);
}
