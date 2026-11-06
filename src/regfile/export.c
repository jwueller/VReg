#include "regfile/script.h"

#include "core/alloc.h"
#include "core/hive.h"
#include "core/node.h"
#include "core/owned_reads.h"

/// A value read from a key, held until the key's values are all known and can
/// be appended in name order.
typedef struct VReg__ValueEntry
{
    VReg__WString name;
    DWORD dwType;
    VReg__Buffer data;
    BOOL isTombstone;
} VReg__ValueEntry;

/// Inserts @p pEntry into @p pValues, an array of @ref VReg__ValueEntry sorted
/// by name; the array owns the entry from then on.
static BOOL VReg__InsertValueSorted(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Array* const pValues,
    _In_ const VReg__ValueEntry* const pEntry
)
{
    const VReg__WStringView name = VReg__WStringViewOf(&pEntry->name);
    SIZE_T lo = 0;
    SIZE_T hi = pValues->count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        const VReg__ValueEntry* const pMid = &VREG__AT(pValues, VReg__ValueEntry, mid);
        if (VReg__CompareNames(VReg__WStringViewOf(&pMid->name), name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    return VReg__ArrayInsert(pAllocContext, pValues, sizeof(*pEntry), lo, pEntry);
}

static void VReg__FreeValues(_In_opt_ void* const pAllocContext, _Inout_ VReg__Array* const pValues)
{
    SIZE_T i;
    for (i = 0; i < pValues->count; ++i)
    {
        VReg__ValueEntry* const pEntry = &VREG__AT(pValues, VReg__ValueEntry, i);
        VReg__WStringFree(pAllocContext, &pEntry->name);
        VReg__BufferFree(pAllocContext, &pEntry->data);
    }

    pValues->count = 0;
}

/// Appends the sorted values of a key as operations and empties the array.
static LSTATUS VReg__FlushValues(
    _Inout_ VRegScript* const pScript,
    _In_ const HKEY hRoot,
    const VReg__WStringView path,
    _Inout_ VReg__Array* const pValues
)
{
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T i;
    for (i = 0; status == ERROR_SUCCESS && i < pValues->count; ++i)
    {
        const VReg__ValueEntry* const pEntry = &VREG__AT(pValues, VReg__ValueEntry, i);
        status = VReg__ScriptAppend(
            pScript,
            pEntry->isTombstone ? VREG_OPERATION_DELETE_VALUE : VREG_OPERATION_SET_VALUE,
            hRoot,
            path,
            VReg__WStringViewOf(&pEntry->name),
            pEntry->dwType,
            pEntry->data.pb,
            (DWORD)pEntry->data.cb,
            0
        );
    }

    VReg__FreeValues(pScript->pAllocContext, pValues);
    return status;
}

/// Appends @p path to @p pPrefix with a backslash between, into @p pFull.
static BOOL VReg__JoinKeyPath(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pFull,
    const VReg__WStringView prefix,
    const VReg__WStringView path
)
{
    if (!VReg__WStringAssign(pAllocContext, pFull, prefix.pch, prefix.cch))
        return FALSE;
    if (path.cch == 0)
        return TRUE;
    if (pFull->cch != 0 && !VReg__WStringAppendChar(pAllocContext, pFull, L'\\'))
        return FALSE;

    return VReg__WStringAppend(pAllocContext, pFull, path.pch, path.cch);
}

// --- Snapshot ---

/// Appends the key @p hKey at @p pPath below the root, then its values, then
/// its subkeys in order.
static LSTATUS VReg__WalkKey(
    _Inout_ VRegScript* const pScript,
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hRoot,
    _In_ const HKEY hKey,
    _In_ const VReg__WString* const pPath,
    const unsigned depth
)
{
    void* const pAllocContext = pScript->pAllocContext;
    VReg__Array values = {NULL, 0, 0};
    VReg__Array subkeys = {NULL, 0, 0};
    VReg__WString name = {NULL, 0, 0};
    VReg__WString childPath = {NULL, 0, 0};
    DWORD dwIndex;
    SIZE_T i;
    LSTATUS status;

    // The registry nests at most 512 levels; anything deeper is a cycle
    // through some other registry.
    if (depth > 512)
        return ERROR_CANTREAD;

    status = VReg__ScriptAppend(
        pScript,
        VREG_OPERATION_CREATE_KEY,
        hRoot,
        VReg__WStringViewOf(pPath),
        VReg__ViewOf(L""),
        0,
        NULL,
        0,
        0
    );
    if (status != ERROR_SUCCESS)
        goto done;

    for (dwIndex = 0;; ++dwIndex)
    {
        VReg__ValueEntry entry;
        ZeroMemory(&entry, sizeof(entry));
        status = VReg__EnumValueOwned(pRegistry, hKey, dwIndex, pAllocContext, &entry.name, &entry.dwType, &entry.data);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            VReg__WStringFree(pAllocContext, &entry.name);
            VReg__BufferFree(pAllocContext, &entry.data);
            status = ERROR_SUCCESS;
            break;
        }
        if (status == ERROR_SUCCESS && !VReg__InsertValueSorted(pAllocContext, &values, &entry))
            status = ERROR_OUTOFMEMORY;
        if (status != ERROR_SUCCESS)
        {
            VReg__WStringFree(pAllocContext, &entry.name);
            VReg__BufferFree(pAllocContext, &entry.data);
            goto done;
        }
    }

    status = VReg__FlushValues(pScript, hRoot, VReg__WStringViewOf(pPath), &values);
    if (status != ERROR_SUCCESS)
        goto done;

    for (dwIndex = 0;; ++dwIndex)
    {
        status = VReg__EnumKeyOwned(pRegistry, hKey, dwIndex, pAllocContext, &name);
        if (status == ERROR_NO_MORE_ITEMS)
        {
            status = ERROR_SUCCESS;
            break;
        }
        if (status != ERROR_SUCCESS)
            goto done;
        if (!VReg__WStringArrayInsertSorted(pAllocContext, &subkeys, VReg__WStringViewOf(&name)))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    for (i = 0; i < subkeys.count; ++i)
    {
        const VReg__WString* const pSubkey = &VREG__AT(&subkeys, VReg__WString, i);
        HKEY hSubkey = NULL;

        // A symbolic link is exported as the key it is (its SymbolicLinkValue),
        // never followed: following one can leave the subtree or loop.
        status = VRegOpenKeyExW(
            pRegistry,
            hKey,
            VReg__WStringText(pSubkey),
            REG_OPTION_OPEN_LINK,
            KEY_READ,
            &hSubkey
        );
        if (status != ERROR_SUCCESS)
            goto done;

        if (!VReg__JoinKeyPath(pAllocContext, &childPath, VReg__WStringViewOf(pPath), VReg__WStringViewOf(pSubkey)))
            status = ERROR_OUTOFMEMORY;
        else
            status = VReg__WalkKey(pScript, pRegistry, hRoot, hSubkey, &childPath, depth + 1);

        VRegCloseKey(
            pRegistry,
            hSubkey
        );
        if (status != ERROR_SUCCESS)
            goto done;
    }

done:
    VReg__FreeValues(pAllocContext, &values);
    VReg__ArrayFree(pAllocContext, &values);
    VReg__WStringArrayFree(pAllocContext, &subkeys);
    VReg__WStringFree(pAllocContext, &name);
    VReg__WStringFree(pAllocContext, &childPath);
    return status;
}

LSTATUS WINAPI VRegSnapshotScript(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hRoot,
    _In_opt_z_ const LPCWSTR pszPath,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegScript** const ppScript
)
{
    VRegScript* pScript = NULL;
    VReg__WString path = {NULL, 0, 0};
    HKEY hKey = NULL;
    LSTATUS status;

    *ppScript = NULL;
    if (!VReg__RootName(hRoot))
        return ERROR_INVALID_HANDLE;

    status = VReg__ScriptCreate(pAllocContext, &pScript);
    if (status != ERROR_SUCCESS)
        return status;

    status = VRegOpenKeyExW(
        pRegistry,
        hRoot,
        pszPath ? pszPath : L"",
        0,
        KEY_READ,
        &hKey
    );
    if (status != ERROR_SUCCESS)
        goto done;

    if (!VReg__WStringAssign(pAllocContext, &path, pszPath, VReg__ViewOf(pszPath).cch))
        status = ERROR_OUTOFMEMORY;
    else
        status = VReg__WalkKey(pScript, pRegistry, hRoot, hKey, &path, 0);

    VRegCloseKey(
        pRegistry,
        hKey
    );

done:
    VReg__WStringFree(pAllocContext, &path);
    if (status != ERROR_SUCCESS)
    {
        VRegDestroyScript(pScript);
        return status;
    }

    *ppScript = pScript;
    return ERROR_SUCCESS;
}

// --- Delta ---

/// The state of the walk over a hive's upper layer: the key whose values are
/// being collected, and the values so far.
typedef struct VReg__DeltaWalk
{
    VRegScript* pScript;
    HKEY hRoot;
    VReg__WStringView prefix;

    /// The full path of the key the pending values belong to.
    VReg__WString keyPath;
    VReg__Array values;
} VReg__DeltaWalk;

static LSTATUS VReg__VisitUpper(_In_ void* const pContext, _In_ const VReg__UpperRecord* const pRecord)
{
    VReg__DeltaWalk* const pWalk = (VReg__DeltaWalk*)pContext;
    void* const pAllocContext = pWalk->pScript->pAllocContext;
    LSTATUS status;

    if (pRecord->kind == VREG__UPPER_VALUE)
    {
        VReg__ValueEntry entry;
        ZeroMemory(&entry, sizeof(entry));
        entry.dwType = pRecord->dwType;
        entry.isTombstone = pRecord->isTombstone;
        if (
            !VReg__WStringAssign(pAllocContext, &entry.name, pRecord->name.pch, pRecord->name.cch)
            || !VReg__BufferAssign(pAllocContext, &entry.data, pRecord->pData, pRecord->cbData)
            || !VReg__InsertValueSorted(pAllocContext, &pWalk->values, &entry)
        )
        {
            VReg__WStringFree(pAllocContext, &entry.name);
            VReg__BufferFree(pAllocContext, &entry.data);
            return ERROR_OUTOFMEMORY;
        }

        return ERROR_SUCCESS;
    }

    status = VReg__FlushValues(pWalk->pScript, pWalk->hRoot, VReg__WStringViewOf(&pWalk->keyPath), &pWalk->values);
    if (status != ERROR_SUCCESS)
        return status;
    if (!VReg__JoinKeyPath(pAllocContext, &pWalk->keyPath, pWalk->prefix, pRecord->path))
        return ERROR_OUTOFMEMORY;

    if (pRecord->isTombstone || pRecord->isOpaque)
    {
        status = VReg__ScriptAppend(
            pWalk->pScript,
            VREG_OPERATION_DELETE_KEY,
            pWalk->hRoot,
            VReg__WStringViewOf(&pWalk->keyPath),
            VReg__ViewOf(L""),
            0,
            NULL,
            0,
            0
        );
        if (status != ERROR_SUCCESS)
            return status;
    }
    if (pRecord->isTombstone)
        return ERROR_SUCCESS;

    return VReg__ScriptAppend(
        pWalk->pScript,
        VREG_OPERATION_CREATE_KEY,
        pWalk->hRoot,
        VReg__WStringViewOf(&pWalk->keyPath),
        VReg__ViewOf(L""),
        0,
        NULL,
        0,
        0
    );
}

LSTATUS WINAPI VRegDeltaScript(
    _In_ VRegHive* const pHive,
    _In_ const HKEY hRoot,
    _In_opt_z_ const LPCWSTR pszPath,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegScript** const ppScript
)
{
    VReg__DeltaWalk walk;
    LSTATUS status;

    *ppScript = NULL;
    if (!VReg__RootName(hRoot))
        return ERROR_INVALID_HANDLE;

    ZeroMemory(&walk, sizeof(walk));
    walk.hRoot = hRoot;
    walk.prefix = VReg__ViewOf(pszPath);
    status = VReg__ScriptCreate(pAllocContext, &walk.pScript);
    if (status != ERROR_SUCCESS)
        return status;

    status = VReg__HiveSnapshotUpper(pHive, &VReg__VisitUpper, &walk);
    if (status == ERROR_SUCCESS)
        status = VReg__FlushValues(walk.pScript, hRoot, VReg__WStringViewOf(&walk.keyPath), &walk.values);

    VReg__FreeValues(pAllocContext, &walk.values);
    VReg__ArrayFree(pAllocContext, &walk.values);
    VReg__WStringFree(pAllocContext, &walk.keyPath);
    if (status != ERROR_SUCCESS)
    {
        VRegDestroyScript(walk.pScript);
        return status;
    }

    *ppScript = walk.pScript;
    return ERROR_SUCCESS;
}
