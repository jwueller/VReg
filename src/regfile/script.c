#include "regfile/script.h"

#include "core/alloc.h"

/// Every root a `.reg` file can name, the one list import and export share.
static const VReg__RootKey VREG__ROOT_KEYS[] = {
    {L"HKEY_CLASSES_ROOT", L"HKCR", HKEY_CLASSES_ROOT},
    {L"HKEY_CURRENT_USER", L"HKCU", HKEY_CURRENT_USER},
    {L"HKEY_LOCAL_MACHINE", L"HKLM", HKEY_LOCAL_MACHINE},
    {L"HKEY_USERS", L"HKU", HKEY_USERS},
    {L"HKEY_CURRENT_CONFIG", L"HKCC", HKEY_CURRENT_CONFIG},
};

static wchar_t VReg__LowerAscii(const wchar_t ch)
{
    return (ch >= L'A' && ch <= L'Z') ? (wchar_t)(ch - L'A' + L'a') : ch;
}

static BOOL VReg__EqualsIgnoreAsciiCase(const VReg__WStringView left, const VReg__WStringView right)
{
    SIZE_T i;
    if (left.cch != right.cch)
        return FALSE;

    for (i = 0; i < left.cch; ++i)
    {
        if (VReg__LowerAscii(left.pch[i]) != VReg__LowerAscii(right.pch[i]))
            return FALSE;
    }

    return TRUE;
}

VREGDEF _Ret_maybenull_z_ const wchar_t* VReg__RootName(_In_opt_ const HKEY hRoot)
{
    SIZE_T i;
    for (i = 0; i < sizeof(VREG__ROOT_KEYS) / sizeof(VREG__ROOT_KEYS[0]); ++i)
    {
        if (VREG__ROOT_KEYS[i].hKey == hRoot)
            return VREG__ROOT_KEYS[i].pszLong;
    }

    return NULL;
}

VREGDEF BOOL VReg__FindRoot(const VReg__WStringView name, _Out_ PHKEY const phRoot)
{
    SIZE_T i;
    *phRoot = NULL;
    for (i = 0; i < sizeof(VREG__ROOT_KEYS) / sizeof(VREG__ROOT_KEYS[0]); ++i)
    {
        const VReg__RootKey* const pRoot = &VREG__ROOT_KEYS[i];
        if (
            VReg__EqualsIgnoreAsciiCase(name, VReg__ViewOf(pRoot->pszLong))
            || VReg__EqualsIgnoreAsciiCase(name, VReg__ViewOf(pRoot->pszShort))
        )
        {
            *phRoot = pRoot->hKey;
            return TRUE;
        }
    }

    return FALSE;
}

VREGDEF LSTATUS VReg__ScriptCreate(_In_opt_ void* const pAllocContext, _Outptr_ VRegScript** const ppScript)
{
    VRegScript* const pScript = (VRegScript*)VReg__AllocZeroed(pAllocContext, sizeof(VRegScript));
    *ppScript = NULL;
    if (!pScript)
        return ERROR_OUTOFMEMORY;

    pScript->pAllocContext = pAllocContext;
    *ppScript = pScript;
    return ERROR_SUCCESS;
}

VREGDEF LSTATUS VReg__ScriptAppend(
    _Inout_ VRegScript* const pScript,
    const VRegOperationKind kind,
    _In_ const HKEY hRoot,
    const VReg__WStringView path,
    const VReg__WStringView name,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData,
    const SIZE_T line
)
{
    void* const pAllocContext = pScript->pAllocContext;
    VReg__ScriptEntry entry;
    VRegOperation operation;
    ZeroMemory(&entry, sizeof(entry));
    if (
        !VReg__WStringAssign(pAllocContext, &entry.path, path.pch, path.cch)
        || !VReg__WStringAssign(pAllocContext, &entry.name, name.pch, name.cch)
        || !VReg__BufferAssign(pAllocContext, &entry.data, pData, cbData)
        || !VReg__ArrayReserve(pAllocContext, &pScript->operations, sizeof(operation), pScript->operations.count + 1)
        || !VReg__ArrayAppend(pAllocContext, &pScript->entries, sizeof(entry), &entry)
    )
    {
        VReg__WStringFree(pAllocContext, &entry.path);
        VReg__WStringFree(pAllocContext, &entry.name);
        VReg__BufferFree(pAllocContext, &entry.data);
        return ERROR_OUTOFMEMORY;
    }

    // The entry's storage is on the heap, so the pointers stay valid when the
    // array of entries moves.
    operation.kind = kind;
    operation.hRoot = hRoot;
    operation.pszPath = VReg__WStringText(&entry.path);
    operation.pszName = VReg__WStringText(&entry.name);
    operation.dwType = dwType;
    operation.pData = entry.data.pb;
    operation.cbData = cbData;
    operation.line = line;
    VReg__ArrayAppend(pAllocContext, &pScript->operations, sizeof(operation), &operation);
    return ERROR_SUCCESS;
}

VREGDEF void WINAPI VRegDestroyScript(_In_opt_ VRegScript* const pScript)
{
    SIZE_T i;
    if (!pScript)
        return;

    for (i = 0; i < pScript->entries.count; ++i)
    {
        VReg__ScriptEntry* const pEntry = &VREG__AT(&pScript->entries, VReg__ScriptEntry, i);
        VReg__WStringFree(pScript->pAllocContext, &pEntry->path);
        VReg__WStringFree(pScript->pAllocContext, &pEntry->name);
        VReg__BufferFree(pScript->pAllocContext, &pEntry->data);
    }

    VReg__ArrayFree(pScript->pAllocContext, &pScript->entries);
    VReg__ArrayFree(pScript->pAllocContext, &pScript->operations);
    VReg__Free(pScript->pAllocContext, pScript);
}

VREGDEF _Ret_notnull_ const VRegOperation* WINAPI VRegGetScriptOperations(
    _In_ const VRegScript* const pScript,
    _Out_ SIZE_T* const pcOperations
)
{
    static const VRegOperation NONE = {VREG_OPERATION_CREATE_KEY, NULL, L"", L"", 0, NULL, 0, 0};
    *pcOperations = pScript->operations.count;
    return pScript->operations.count == 0 ? &NONE : (const VRegOperation*)pScript->operations.p;
}

VREGDEF void WINAPI VRegFreeScriptBytes(_In_ const VRegScript* const pScript, _In_opt_ void* const pBytes)
{
    VReg__Free(pScript->pAllocContext, pBytes);
}
