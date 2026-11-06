#include "core/hive.h"

#include "core/alloc.h"
#include "core/handle_space.h"
#include "core/node.h"
#include "core/owned_reads.h"
#include "core/path.h"
#include "core/retry.h"

struct VRegHive
{
    void* pAllocContext;

    /// Null for a hive held entirely in memory.
    VRegRegistry* pLower;
    HKEY hLowerRoot;
    VReg__WString lowerPath;
    REGSAM samLowerView;

    SRWLOCK lock;
    volatile LONG isFrozen;

    /// Fixed for the life of the hive.
    VReg__Node* pRoot;

    /// `VReg__Node*`: nodes are created here and freed only with the hive; a
    /// deleted node stays allocated (marked deleted) so its handle keeps
    /// answering @ref ERROR_KEY_DELETED. Destroying the hive unbinds every
    /// handle it issued.
    VReg__Array nodes;
};

#define VREG__NODE_AT(pArray, index) VREG__AT(pArray, VReg__Node*, index)
#define VREG__VALUE_AT(pArray, index) VREG__AT(pArray, VReg__Value, index)
#define VREG__WATCH_AT(pArray, index) VREG__AT(pArray, VReg__ChangeWatch, index)
#define VREG__NAME_AT(pArray, index) VREG__AT(pArray, VReg__WString, index)

static const VReg__WStringView VREG__NO_NAME = {NULL, 0};

VREGDEF int VReg__CompareNames(const VReg__WStringView left, const VReg__WStringView right)
{
    if (left.cch == 0 || right.cch == 0)
        return (left.cch == 0) == (right.cch == 0) ? 0 : (left.cch == 0 ? -1 : 1);

    return CompareStringOrdinal(left.pch, (int)left.cch, right.pch, (int)right.cch, TRUE) - CSTR_EQUAL;
}

/// Whether @p status says the thing asked about exists: found, or found but
/// not readable this way.
static BOOL VReg__StatusExists(const LSTATUS status)
{
    return status == ERROR_SUCCESS || status == ERROR_MORE_DATA || status == ERROR_ACCESS_DENIED;
}

// --- Locking ---

static BOOL VReg__HiveIsFrozen(_In_ VRegHive* const pHive)
{
    return ReadAcquire(&pHive->isFrozen) != 0;
}

/// Takes the lock shared, or nothing once frozen; answers whether it did.
static BOOL VReg__HiveLockRead(_In_ VRegHive* const pHive)
{
    if (VReg__HiveIsFrozen(pHive))
        return FALSE;

    AcquireSRWLockShared(&pHive->lock);
    return TRUE;
}

static void VReg__HiveUnlockRead(_In_ VRegHive* const pHive, const BOOL isLocked)
{
    if (isLocked)
        ReleaseSRWLockShared(&pHive->lock);
}

/// Takes the lock exclusive, or nothing once frozen; answers whether it did.
/// A check of @ref VReg__HiveIsFrozen made before can be stale, since a
/// writer may wait here while @ref VRegFreezeHive runs, so only this decides a
/// write.
static BOOL VReg__HiveLockWrite(_In_ VRegHive* const pHive)
{
    AcquireSRWLockExclusive(&pHive->lock);
    if (!VReg__HiveIsFrozen(pHive))
        return TRUE;

    ReleaseSRWLockExclusive(&pHive->lock);
    return FALSE;
}

static void VReg__HiveUnlockWrite(_In_ VRegHive* const pHive)
{
    ReleaseSRWLockExclusive(&pHive->lock);
}

// --- Nodes, values and watches ---

static void VReg__ValueFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__Value* const pValue)
{
    VReg__WStringFree(pAllocContext, &pValue->name);
    VReg__BufferFree(pAllocContext, &pValue->data);
}

static void VReg__NodeClearValues(_In_opt_ void* const pAllocContext, _Inout_ VReg__Node* const pNode)
{
    SIZE_T i;
    for (i = 0; i < pNode->values.count; ++i)
        VReg__ValueFree(pAllocContext, &VREG__VALUE_AT(&pNode->values, i));

    pNode->values.count = 0;
}

static void VReg__NodeFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__Node* const pNode)
{
    VReg__NodeClearValues(pAllocContext, pNode);
    VReg__ArrayFree(pAllocContext, &pNode->values);
    VReg__ArrayFree(pAllocContext, &pNode->children);
    VReg__ArrayFree(pAllocContext, &pNode->watches);
    VReg__WStringFree(pAllocContext, &pNode->name);
    VReg__WStringFree(pAllocContext, &pNode->className);
    VReg__Free(pAllocContext, pNode);
}

/// Stamps @p pNode as written now.
static void VReg__HiveTouch(_Inout_ VReg__Node* const pNode)
{
    GetSystemTimeAsFileTime(&pNode->lastWrite);
}

/// Makes @p pNode and its transparent ancestors upper keys, as a write to a
/// key below them needs. The root stays as it is: it is the mount point.
static void VReg__HiveMaterialize(_Inout_ VReg__Node* const pNode)
{
    VReg__Node* pCurrent;
    for (pCurrent = pNode; pCurrent->pParent && pCurrent->isTransparent; pCurrent = pCurrent->pParent)
        pCurrent->isTransparent = FALSE;
}

static void VReg__HiveDropWatches(_Inout_ VReg__Node* const pNode)
{
    SIZE_T i;
    for (i = 0; i < pNode->watches.count; ++i)
    {
        // The array's count bounds its initialized elements, which the
        // analyzer cannot see past the reallocation.
#pragma warning(suppress: 6001)
        CloseHandle(VREG__WATCH_AT(&pNode->watches, i).hEvent);
    }

    pNode->watches.count = 0;
}

/// Signals and drops the watches of @p pNode that @p dwFilter selects;
/// @p isInherited limits them to those watching their subtree.
static void VReg__HiveFire(_Inout_ VReg__Node* const pNode, const DWORD dwFilter, const BOOL isInherited)
{
    SIZE_T i = 0;
    while (i < pNode->watches.count)
    {
        const VReg__ChangeWatch* const pWatch = &VREG__WATCH_AT(&pNode->watches, i);
        HANDLE hEvent;
        if (!(pWatch->dwFilter & dwFilter) || (isInherited && !pWatch->isSubtree))
        {
            ++i;
            continue;
        }

        // The array's count bounds its initialized elements, which the
        // analyzer cannot see past the reallocation.
#pragma warning(suppress: 6001)
        hEvent = pWatch->hEvent;
        VReg__ArrayRemoveAt(&pNode->watches, sizeof(*pWatch), i);
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
}

/// Fires and drops the watches a change of kind @p dwFilter (a
/// `REG_NOTIFY_CHANGE_*` bit) on @p pNode reaches: its own, and those of its
/// ancestors that watch their subtree. Neither @ref SetEvent nor
/// @ref CloseHandle can wait on the loader lock or on anything a caller of
/// the hive could hold, so they run under the write lock, which the caller
/// must hold; deferring them would need an allocation after the change is
/// made.
static void VReg__HiveNotify(_Inout_ VReg__Node* const pNode, const DWORD dwFilter)
{
    VReg__Node* pAncestor;
    VReg__HiveFire(pNode, dwFilter, FALSE);
    for (pAncestor = pNode->pParent; pAncestor; pAncestor = pAncestor->pParent)
        VReg__HiveFire(pAncestor, dwFilter, TRUE);
}

/// The child of @p pParent named @p name, or null; @p pIndex, if given,
/// receives where it is or where it would be inserted.
static _Ret_maybenull_ VReg__Node* VReg__HiveFindChild(
    _In_ const VReg__Node* const pParent,
    const VReg__WStringView name,
    _Out_opt_ SIZE_T* const pIndex
)
{
    SIZE_T lo = 0;
    SIZE_T hi = pParent->children.count;
    while (lo < hi)
    {
        const SIZE_T mid = lo + (hi - lo) / 2;
        if (VReg__CompareNames(VReg__WStringViewOf(&VREG__NODE_AT(&pParent->children, mid)->name), name) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (pIndex)
        *pIndex = lo;
    if (lo < pParent->children.count)
    {
        VReg__Node* const pChild = VREG__NODE_AT(&pParent->children, lo);
        if (VReg__CompareNames(VReg__WStringViewOf(&pChild->name), name) == 0)
            return pChild;
    }

    return NULL;
}

static _Ret_maybenull_ VReg__Value* VReg__HiveFindValue(_In_ VReg__Node* const pNode, const VReg__WStringView name)
{
    SIZE_T i;
    for (i = 0; i < pNode->values.count; ++i)
    {
        VReg__Value* const pValue = &VREG__VALUE_AT(&pNode->values, i);
        if (VReg__CompareNames(VReg__WStringViewOf(&pValue->name), name) == 0)
            return pValue;
    }

    return NULL;
}

/// A new child of @p pParent, with its handle. The write lock must be held.
static LSTATUS VReg__HiveAddChild(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pParent,
    const VReg__WStringView name,
    const VReg__WStringView className,
    const BOOL isVolatile,
    _Outptr_ VReg__Node** const ppChild
)
{
    void* const pAllocContext = pHive->pAllocContext;
    SIZE_T index = 0;
    VReg__Node* const pNode = (VReg__Node*)VReg__AllocZeroed(pAllocContext, sizeof(VReg__Node));
    if (!pNode)
        return ERROR_OUTOFMEMORY;

    pNode->pHive = pHive;
    pNode->pParent = pParent;
    pNode->isVolatile = isVolatile;
    VReg__HiveTouch(pNode);
    VReg__HiveFindChild(pParent, name, &index);

    // Every allocation happens before the handle is bound, so a failure
    // leaves no handle pointing at a freed node.
    if (
        !VReg__WStringAssign(pAllocContext, &pNode->name, name.pch, name.cch)
        || !VReg__WStringAssign(pAllocContext, &pNode->className, className.pch, className.cch)
        || !VReg__ArrayReserve(pAllocContext, &pHive->nodes, sizeof(pNode), pHive->nodes.count + 1)
        || !VReg__ArrayReserve(pAllocContext, &pParent->children, sizeof(pNode), pParent->children.count + 1)
    )
    {
        VReg__NodeFree(pAllocContext, pNode);
        return ERROR_OUTOFMEMORY;
    }

    pNode->hKey = VReg__HandleSpaceAllocate(VReg__ProcessHandleSpace(), pNode);
    if (!pNode->hKey)
    {
        VReg__NodeFree(pAllocContext, pNode);
        return ERROR_OUTOFMEMORY;
    }

    VReg__ArrayInsert(pAllocContext, &pParent->children, sizeof(pNode), index, &pNode);
    VReg__ArrayAppend(pAllocContext, &pHive->nodes, sizeof(pNode), &pNode);
    VReg__HiveTouch(pParent);
    *ppChild = pNode;
    return ERROR_SUCCESS;
}

/// Children of a deleted key answer as deleted too.
static void VReg__HiveMarkDeleted(_Inout_ VReg__Node* const pNode)
{
    SIZE_T i;
    pNode->isDeleted = TRUE;
    VReg__HiveFire(pNode, ~(DWORD)0, FALSE);
    for (i = 0; i < pNode->children.count; ++i)
        VReg__HiveMarkDeleted(VREG__NODE_AT(&pNode->children, i));
}

/// Marks @p pNode and everything below it deleted, unlinks it from its
/// parent and fires the watches on it and below it, and the name watches
/// on its parent. The write lock must be held.
static void VReg__HiveRemove(_Inout_ VReg__Node* const pNode)
{
    if (pNode->pParent)
    {
        SIZE_T index = 0;
        VReg__HiveFindChild(pNode->pParent, VReg__WStringViewOf(&pNode->name), &index);
        VReg__ArrayRemoveAt(&pNode->pParent->children, sizeof(pNode), index);
        VReg__HiveTouch(pNode->pParent);
    }

    VReg__HiveMarkDeleted(pNode);
    if (pNode->pParent)
        VReg__HiveNotify(pNode->pParent, REG_NOTIFY_CHANGE_NAME);
}

// --- The lower registry ---

/// Appends @p cNames path components to @p pPath, separated by backslashes.
static BOOL VReg__HiveJoinPath(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pPath,
    _In_reads_(cNames) const VReg__WStringView* const pNames,
    const SIZE_T cNames
)
{
    SIZE_T i;
    for (i = 0; i < cNames; ++i)
    {
        if (pPath->cch != 0 && !VReg__WStringAppendChar(pAllocContext, pPath, L'\\'))
            return FALSE;
        if (!VReg__WStringAppend(pAllocContext, pPath, pNames[i].pch, pNames[i].cch))
            return FALSE;
    }

    return TRUE;
}

static BOOL VReg__HiveAppendNodePath(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pPath,
    _In_ const VReg__Node* const pNode
)
{
    if (!pNode->pParent)
        return TRUE;
    if (!VReg__HiveAppendNodePath(pAllocContext, pPath, pNode->pParent))
        return FALSE;
    if (pPath->cch != 0 && !VReg__WStringAppendChar(pAllocContext, pPath, L'\\'))
        return FALSE;

    return VReg__WStringAppend(pAllocContext, pPath, VReg__WStringText(&pNode->name), pNode->name.cch);
}

/// Whether @p pNode's lower key shows through, and its path (read under the
/// lock, used after it is released). Never visible without a lower registry.
static LSTATUS VReg__HiveViewOf(
    _In_ VRegHive* const pHive,
    _In_ const VReg__Node* const pNode,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pPath
)
{
    const VReg__Node* pCurrent;
    *pIsVisible = FALSE;
    if (!pHive->pLower)
        return ERROR_SUCCESS;

    for (pCurrent = pNode; pCurrent; pCurrent = pCurrent->pParent)
    {
        if (pCurrent->isOpaque)
            return ERROR_SUCCESS;
    }

    if (
        !VReg__WStringAssign(pHive->pAllocContext, pPath, VReg__WStringText(&pHive->lowerPath), pHive->lowerPath.cch)
        || !VReg__HiveAppendNodePath(pHive->pAllocContext, pPath, pNode)
    )
        return ERROR_OUTOFMEMORY;

    *pIsVisible = TRUE;
    return ERROR_SUCCESS;
}

static LSTATUS VReg__HiveOpenLower(
    _In_ VRegHive* const pHive,
    _In_ const VReg__WString* const pPath,
    const REGSAM samDesired,
    _Out_ PHKEY const phKey
)
{
    return VRegOpenKeyExW(
        pHive->pLower,
        pHive->hLowerRoot,
        pPath->cch == 0 ? NULL : pPath->psz,
        0,
        samDesired | pHive->samLowerView,
        phKey
    );
}

static void VReg__HiveCloseLower(_In_ VRegHive* const pHive, _In_opt_ const HKEY hKey)
{
    if (hKey)
        VRegCloseKey(
            pHive->pLower,
            hKey
        );
}

/// The class of the lower key at @p pPath; empty when there is none.
static LSTATUS VReg__HiveQueryLowerClass(
    _In_ VRegHive* const pHive,
    _In_ const VReg__WString* const pPath,
    _Inout_ VReg__WString* const pClassName
)
{
    HKEY hLower = NULL;
    DWORD cchBuffer = VREG__INITIAL_NAME_CHARS;
    LSTATUS status;

    if (!VReg__WStringResize(pHive->pAllocContext, pClassName, 0))
        return ERROR_OUTOFMEMORY;

    status = VReg__HiveOpenLower(pHive, pPath, KEY_QUERY_VALUE, &hLower);
    if (status != ERROR_SUCCESS)
        return status == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : status;

    for (;;)
    {
        DWORD cchClass = cchBuffer;
        if (!VReg__WStringResize(pHive->pAllocContext, pClassName, cchBuffer))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }

        status = VRegQueryInfoKeyW(
            pHive->pLower,
            hLower,
            pClassName->psz,
            &cchClass,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
        );
        if (status == ERROR_MORE_DATA || status == ERROR_INSUFFICIENT_BUFFER)
        {
            cchBuffer *= 2;
            continue;
        }

        if (status == ERROR_SUCCESS)
            VReg__WStringTruncate(pClassName, cchClass);
        break;
    }

    VReg__HiveCloseLower(pHive, hLower);
    return status;
}

// --- Creation and destruction ---

static LSTATUS VReg__HiveCreate(
    _In_opt_ VRegRegistry* const pLower,
    _In_opt_ const HKEY hLowerRoot,
    const VReg__WStringView lowerPath,
    const REGSAM samLowerView,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegHive** const ppHive
)
{
    VReg__Node* pRoot;
    VRegHive* const pHive = (VRegHive*)VReg__AllocZeroed(pAllocContext, sizeof(VRegHive));
    *ppHive = NULL;
    if (!pHive)
        return ERROR_OUTOFMEMORY;

    pHive->pAllocContext = pAllocContext;
    pHive->pLower = pLower;
    pHive->hLowerRoot = hLowerRoot;
    pHive->samLowerView = samLowerView;
    InitializeSRWLock(&pHive->lock);

    pRoot = (VReg__Node*)VReg__AllocZeroed(pAllocContext, sizeof(VReg__Node));
    if (
        !VReg__WStringAssign(pAllocContext, &pHive->lowerPath, lowerPath.pch, lowerPath.cch)
        || !pRoot
        || !VReg__ArrayAppend(pAllocContext, &pHive->nodes, sizeof(pRoot), &pRoot)
    )
    {
        VReg__Free(pAllocContext, pRoot);
        VRegDestroyHive(pHive);
        return ERROR_OUTOFMEMORY;
    }

    pRoot->pHive = pHive;
    pRoot->hKey = VReg__HandleSpaceAllocate(VReg__ProcessHandleSpace(), pRoot);
    if (!pRoot->hKey)
    {
        VRegDestroyHive(pHive);
        return ERROR_OUTOFMEMORY;
    }

    VReg__HiveTouch(pRoot);
    if (pLower)
    {
        pRoot->isTransparent = TRUE;
        pRoot->isLowerBacked = TRUE;
    }

    pHive->pRoot = pRoot;
    *ppHive = pHive;
    return ERROR_SUCCESS;
}

VREGDEF LSTATUS WINAPI VRegCreateHive(_In_opt_ void* const pAllocContext, _Outptr_ VRegHive** const ppHive)
{
    return VReg__HiveCreate(NULL, NULL, VREG__NO_NAME, 0, pAllocContext, ppHive);
}

VREGDEF LSTATUS WINAPI VRegCreateLayeredHive(
    _In_ VRegRegistry* const pLower,
    _In_ const HKEY hLowerRoot,
    _In_opt_z_ const LPCWSTR pszLowerPath,
    _In_ const REGSAM samLowerView,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegHive** const ppHive
)
{
    return VReg__HiveCreate(pLower, hLowerRoot, VReg__ViewOf(pszLowerPath), samLowerView, pAllocContext, ppHive);
}

VREGDEF void WINAPI VRegDestroyHive(_In_opt_ VRegHive* const pHive)
{
    SIZE_T i;
    if (!pHive)
        return;

    for (i = 0; i < pHive->nodes.count; ++i)
    {
        VReg__Node* const pNode = VREG__NODE_AT(&pHive->nodes, i);
        VReg__HiveDropWatches(pNode);
        VReg__HandleSpaceRelease(VReg__ProcessHandleSpace(), pNode->hKey);
        VReg__NodeFree(pHive->pAllocContext, pNode);
    }

    VReg__ArrayFree(pHive->pAllocContext, &pHive->nodes);
    VReg__WStringFree(pHive->pAllocContext, &pHive->lowerPath);
    VReg__Free(pHive->pAllocContext, pHive);
}

VREGDEF void WINAPI VRegFreezeHive(_In_ VRegHive* const pHive)
{
    SIZE_T i;

    // Taking the lock once more waits out every writer still inside; readers
    // that see the flag afterwards skip the lock and see all their writes.
    AcquireSRWLockExclusive(&pHive->lock);
    WriteRelease(&pHive->isFrozen, 1);
    for (i = 0; i < pHive->nodes.count; ++i)
        VReg__HiveDropWatches(VREG__NODE_AT(&pHive->nodes, i));

    ReleaseSRWLockExclusive(&pHive->lock);
}

VREGDEF BOOL WINAPI VRegIsHiveFrozen(_In_ VRegHive* const pHive)
{
    return VReg__HiveIsFrozen(pHive);
}

VREGDEF _Ret_notnull_ VReg__Node* VReg__HiveRoot(_In_ VRegHive* const pHive)
{
    return pHive->pRoot;
}

VREGDEF _Ret_maybenull_ VRegRegistry* VReg__HiveLower(_In_ VRegHive* const pHive)
{
    return pHive->pLower;
}

VREGDEF _Ret_maybenull_ void* VReg__HiveAllocContext(_In_ VRegHive* const pHive)
{
    return pHive->pAllocContext;
}

VREGDEF HKEY VReg__NodeHandle(_In_ const VReg__Node* const pNode)
{
    return pNode->hKey;
}

VREGDEF _Ret_maybenull_ VReg__Node* VReg__NodeOf(_In_opt_ const HKEY hKey)
{
    return (VReg__Node*)VReg__HandleSpaceFind(VReg__ProcessHandleSpace(), hKey);
}

VREGDEF _Ret_notnull_ VRegHive* VReg__NodeHive(_In_ const VReg__Node* const pNode)
{
    return pNode->pHive;
}

// --- Keys ---

/// Follows @p pPath through the upper layer from @p pParent as far as it
/// goes. @p pcMatched receives how many components were found and
/// @p ppCurrent the node they led to; when the path goes on from there,
/// @p pIsVisible and @p pLowerPath describe that node's lower key.
static LSTATUS VReg__HiveWalkUpper(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pParent,
    _In_reads_(cPath) const VReg__WStringView* const pPath,
    const SIZE_T cPath,
    _Outptr_ VReg__Node** const ppCurrent,
    _Out_ SIZE_T* const pcMatched,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pLowerPath
)
{
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    VReg__Node* pCurrent = pParent;
    SIZE_T i = 0;

    *pIsVisible = FALSE;
    if (pParent->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        for (; i < cPath; ++i)
        {
            VReg__Node* const pChild = VReg__HiveFindChild(pCurrent, pPath[i], NULL);
            if (!pChild)
                break;
            if (pChild->isTombstone)
            {
                status = ERROR_FILE_NOT_FOUND;
                break;
            }

            pCurrent = pChild;
        }

        if (status == ERROR_SUCCESS && i < cPath)
            status = VReg__HiveViewOf(pHive, pCurrent, pIsVisible, pLowerPath);
    }

    VReg__HiveUnlockRead(pHive, isLocked);
    *ppCurrent = pCurrent;
    *pcMatched = i;
    return status;
}

VREGDEF LSTATUS VReg__HiveOpenKey(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pParent,
    _In_reads_(cPath) const VReg__WStringView* const pPath,
    const SIZE_T cPath,
    const REGSAM samDesired,
    _Out_ VReg__OpenResult* const pResult
)
{
    VReg__Node* pCurrent = pParent;
    SIZE_T cMatched = 0;
    BOOL isVisible = FALSE;
    VReg__WString lowerPath = {NULL, 0, 0};
    HKEY hLower = NULL;
    REGSAM samLower;
    LSTATUS status;

    pResult->pNode = NULL;
    pResult->hLower = NULL;

    status = VReg__HiveWalkUpper(pHive, pParent, pPath, cPath, &pCurrent, &cMatched, &isVisible, &lowerPath);
    if (status != ERROR_SUCCESS)
        goto done;
    if (cMatched == cPath)
    {
        pResult->pNode = pCurrent;
        goto done;
    }
    if (!isVisible)
    {
        status = ERROR_FILE_NOT_FOUND;
        goto done;
    }

    // Nothing of the upper layer is at or below the key, so it is the lower
    // key's to answer.
    if (!VReg__HiveJoinPath(pHive->pAllocContext, &lowerPath, pPath + cMatched, cPath - cMatched))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    samLower = VReg__HiveIsFrozen(pHive) ? samDesired : KEY_QUERY_VALUE | (samDesired & KEY_WOW64_RES);
    status = VReg__HiveOpenLower(pHive, &lowerPath, samLower, &hLower);
    if (status != ERROR_SUCCESS)
        goto done;

    if (VReg__HiveIsFrozen(pHive))
    {
        pResult->hLower = hLower;
        hLower = NULL;
        goto done;
    }

    // Writable: every open inside the mount answers with a node, or a write
    // through a lower handle handed out now would miss a later copy-up.
    if (!VReg__HiveLockWrite(pHive))
    {
        // Frozen since the lower open: the frozen answer is a lower handle.
        VReg__HiveCloseLower(pHive, hLower);
        hLower = NULL;
        status = VReg__HiveOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
        goto done;
    }

    if (pCurrent->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        SIZE_T i;
        for (i = cMatched; i < cPath; ++i)
        {
            VReg__Node* pChild = VReg__HiveFindChild(pCurrent, pPath[i], NULL);
            if (pChild)
            {
                if (pChild->isTombstone)
                {
                    status = ERROR_FILE_NOT_FOUND;
                    break;
                }

                pCurrent = pChild;
                continue;
            }

            status = VReg__HiveAddChild(pHive, pCurrent, pPath[i], VREG__NO_NAME, FALSE, &pChild);
            if (status != ERROR_SUCCESS)
                break;

            pChild->isTransparent = TRUE;
            pChild->isLowerBacked = TRUE;
            pCurrent = pChild;
        }

        if (status == ERROR_SUCCESS)
            pResult->pNode = pCurrent;
    }

    VReg__HiveUnlockWrite(pHive);

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringFree(pHive->pAllocContext, &lowerPath);
    return status;
}

/// How far @p pPath exists in the upper layer below @p pParent, for
/// @ref VReg__HiveCreateKey: @p pcUpper components; @p pIsBehindTombstone
/// when a tombstone stopped the walk; otherwise the lower view of the last
/// node reached, when the path goes on.
static LSTATUS VReg__HiveMeasureUpper(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pParent,
    _In_reads_(cPath) const VReg__WStringView* const pPath,
    const SIZE_T cPath,
    _Out_ SIZE_T* const pcUpper,
    _Out_ BOOL* const pIsBehindTombstone,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath
)
{
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    const VReg__Node* pCurrent = pParent;
    SIZE_T cUpper = 0;

    *pIsBehindTombstone = FALSE;
    *pIsVisible = FALSE;
    if (pParent->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        for (; cUpper < cPath; ++cUpper)
        {
            const VReg__Node* const pChild = VReg__HiveFindChild(pCurrent, pPath[cUpper], NULL);
            if (!pChild)
                break;
            if (pChild->isTombstone)
            {
                *pIsBehindTombstone = TRUE;
                break;
            }

            pCurrent = pChild;
        }

        if (cUpper < cPath && !*pIsBehindTombstone)
            status = VReg__HiveViewOf(pHive, pCurrent, pIsVisible, pViewPath);
    }

    VReg__HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREGDEF LSTATUS VReg__HiveCreateKey(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pParent,
    _In_reads_(cPath) const VReg__WStringView* const pPath,
    const SIZE_T cPath,
    _In_z_ const LPCWSTR pszClassName,
    const DWORD dwOptions,
    const REGSAM samDesired,
    _Out_ VReg__OpenResult* const pResult,
    _Out_ LPDWORD const pdwDisposition
)
{
    const VReg__WStringView className = VReg__ViewOf(pszClassName);
    const BOOL isVolatile = (dwOptions & REG_OPTION_VOLATILE) != 0;
    SIZE_T cUpper = 0;
    SIZE_T cLower;
    BOOL isBehindTombstone = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    VReg__WString probePath = {NULL, 0, 0};
    VReg__Node* pCurrent;
    SIZE_T i;
    LSTATUS status;

    pResult->pNode = NULL;
    pResult->hLower = NULL;
    *pdwDisposition = REG_OPENED_EXISTING_KEY;

    if (VReg__HiveIsFrozen(pHive))
    {
        status = VReg__HiveOpenKey(pHive, pParent, pPath, cPath, samDesired, pResult);
        return status == ERROR_FILE_NOT_FOUND ? ERROR_ACCESS_DENIED : status;
    }

    // How far the path exists: in the upper layer first, then below it.
    status = VReg__HiveMeasureUpper(pHive, pParent, pPath, cPath, &cUpper, &isBehindTombstone, &isVisible, &viewPath);
    if (status != ERROR_SUCCESS)
        goto done;

    cLower = cUpper;
    if (isVisible)
    {
        for (; cLower < cPath; ++cLower)
        {
            HKEY hLower = NULL;
            if (
                !VReg__WStringAssign(pHive->pAllocContext, &probePath, viewPath.psz, viewPath.cch)
                || !VReg__HiveJoinPath(pHive->pAllocContext, &probePath, pPath + cUpper, cLower + 1 - cUpper)
            )
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }

            status = VReg__HiveOpenLower(pHive, &probePath, KEY_QUERY_VALUE, &hLower);
            VReg__HiveCloseLower(pHive, status == ERROR_SUCCESS ? hLower : NULL);
            if (status == ERROR_FILE_NOT_FOUND)
            {
                status = ERROR_SUCCESS;
                break;
            }
            if (!VReg__StatusExists(status))
                goto done;

            status = ERROR_SUCCESS;
        }
    }

    if (!VReg__HiveLockWrite(pHive))
    {
        status = VReg__HiveCreateKey(
            pHive,
            pParent,
            pPath,
            cPath,
            pszClassName,
            dwOptions,
            samDesired,
            pResult,
            pdwDisposition
        );
        goto done;
    }

    if (pParent->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        pCurrent = pParent;
        for (i = 0; i < cPath; ++i)
        {
            const BOOL isLast = i + 1 == cPath;
            const BOOL isInLower = i >= cUpper && i < cLower;
            VReg__Node* pChild = VReg__HiveFindChild(pCurrent, pPath[i], NULL);
            VReg__Node* pNew;
            if (pChild && !pChild->isTombstone)
            {
                pCurrent = pChild;
                continue;
            }

            if (pChild)
            {
                // Created again over a deleted lower key: nothing below shows.
                const VReg__WStringView newClass = isLast ? className : VREG__NO_NAME;
                if (!VReg__WStringAssign(pHive->pAllocContext, &pChild->className, newClass.pch, newClass.cch))
                {
                    status = ERROR_OUTOFMEMORY;
                    break;
                }

                pChild->isTombstone = FALSE;
                pChild->isOpaque = TRUE;
                VReg__HiveTouch(pChild);
                VReg__HiveMaterialize(pCurrent);
                VReg__HiveNotify(pCurrent, REG_NOTIFY_CHANGE_NAME);
                pCurrent = pChild;
                if (isLast)
                    *pdwDisposition = REG_CREATED_NEW_KEY;
                continue;
            }

            if (!isInLower && pCurrent->isVolatile && !isVolatile)
            {
                status = ERROR_CHILD_MUST_BE_VOLATILE;
                break;
            }

            status = VReg__HiveAddChild(
                pHive,
                pCurrent,
                pPath[i],
                isLast && !isInLower ? className : VREG__NO_NAME,
                !isInLower && isVolatile,
                &pNew
            );
            if (status != ERROR_SUCCESS)
                break;

            if (isInLower)
            {
                pNew->isTransparent = TRUE;
                pNew->isLowerBacked = TRUE;
            }
            else
            {
                VReg__HiveMaterialize(pCurrent);
                VReg__HiveNotify(pCurrent, REG_NOTIFY_CHANGE_NAME);
                if (isLast)
                    *pdwDisposition = REG_CREATED_NEW_KEY;
            }

            pCurrent = pNew;
        }

        if (status == ERROR_SUCCESS)
            pResult->pNode = pCurrent;
    }

    VReg__HiveUnlockWrite(pHive);

done:
    VReg__WStringFree(pHive->pAllocContext, &viewPath);
    VReg__WStringFree(pHive->pAllocContext, &probePath);
    return status;
}

/// What @ref VReg__HiveDeleteKey needs to know about @p pNode before it calls
/// the lower registry: the names of its tombstone children (a child that is
/// not one fails the call), its view and its parent's, and its name.
static LSTATUS VReg__HiveInspectForDelete(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _Inout_ VReg__Array* const pTombstones,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath,
    _Out_ BOOL* const pIsParentVisible,
    _Inout_ VReg__WString* const pParentViewPath,
    _Inout_ VReg__WString* const pKeyName
)
{
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T i;

    *pIsVisible = FALSE;
    *pIsParentVisible = FALSE;
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;
    else if (!pNode->pParent)
        status = ERROR_ACCESS_DENIED;

    for (i = 0; status == ERROR_SUCCESS && i < pNode->children.count; ++i)
    {
        const VReg__Node* const pChild = VREG__NODE_AT(&pNode->children, i);
        if (!pChild->isTombstone)
            status = ERROR_ACCESS_DENIED;
        else if (!VReg__WStringArrayInsertSorted(pHive->pAllocContext, pTombstones, VReg__WStringViewOf(&pChild->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS)
        status = VReg__HiveViewOf(pHive, pNode, pIsVisible, pViewPath);
    if (status == ERROR_SUCCESS)
        status = VReg__HiveViewOf(pHive, pNode->pParent, pIsParentVisible, pParentViewPath);
    if (status == ERROR_SUCCESS)
    {
        const VReg__WStringView keyName = VReg__WStringViewOf(&pNode->name);
        if (!VReg__WStringAssign(pHive->pAllocContext, pKeyName, keyName.pch, keyName.cch))
            status = ERROR_OUTOFMEMORY;
    }

    VReg__HiveUnlockRead(pHive, isLocked);
    return status;
}

VREGDEF LSTATUS VReg__HiveDeleteKey(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pParent,
    _In_reads_(cPath) const VReg__WStringView* const pPath,
    const SIZE_T cPath
)
{
    void* const pAllocContext = pHive->pAllocContext;
    VReg__OpenResult opened;
    VReg__Node* pNode;
    VReg__Array tombstones = {NULL, 0, 0};
    BOOL isVisible = FALSE;
    BOOL isParentVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    VReg__WString parentViewPath = {NULL, 0, 0};
    VReg__WString keyName = {NULL, 0, 0};
    VReg__WString lowerName = {NULL, 0, 0};
    HKEY hLower = NULL;
    BOOL hasLower = FALSE;
    SIZE_T i;
    LSTATUS status;

    if (VReg__HiveIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;

    status = VReg__HiveOpenKey(pHive, pParent, pPath, cPath, KEY_READ, &opened);
    if (status != ERROR_SUCCESS)
        return status;

    // Frozen since the check above, so the open answered with a lower key.
    if (!opened.pNode)
    {
        VReg__HiveCloseLower(pHive, opened.hLower);
        return ERROR_ACCESS_DENIED;
    }

    pNode = opened.pNode;
    status = VReg__HiveInspectForDelete(
        pHive,
        pNode,
        &tombstones,
        &isVisible,
        &viewPath,
        &isParentVisible,
        &parentViewPath,
        &keyName
    );
    if (status != ERROR_SUCCESS)
        goto done;

    // Whether a lower key sits at the node's path decides if a tombstone must
    // replace the node, even when the node is opaque and hides what is below
    // it. A lower subkey that no tombstone hides still makes the key non-empty.
    if (isParentVisible)
    {
        const VReg__WStringView keyNameView = VReg__WStringViewOf(&keyName);
        if (!VReg__HiveJoinPath(pAllocContext, &parentViewPath, &keyNameView, 1))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        status = VReg__HiveOpenLower(pHive, &parentViewPath, KEY_ENUMERATE_SUB_KEYS, &hLower);
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
            goto done;

        hasLower = status == ERROR_SUCCESS;
        status = ERROR_SUCCESS;
        for (i = 0; hasLower && isVisible; ++i)
        {
            const LSTATUS enumStatus = VReg__EnumKeyOwned(pHive->pLower, hLower, (DWORD)i, pAllocContext, &lowerName);
            if (enumStatus == ERROR_NO_MORE_ITEMS)
                break;
            if (enumStatus != ERROR_SUCCESS)
            {
                status = enumStatus;
                goto done;
            }
            if (!VReg__WStringArrayContainsSorted(&tombstones, VReg__WStringViewOf(&lowerName)))
            {
                status = ERROR_ACCESS_DENIED;
                goto done;
            }
        }
    }

    if (!VReg__HiveLockWrite(pHive))
    {
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        VReg__Node* const pNodeParent = pNode->pParent;
        for (i = 0; i < pNode->children.count; ++i)
        {
            if (!VREG__NODE_AT(&pNode->children, i)->isTombstone)
            {
                status = ERROR_ACCESS_DENIED;
                break;
            }
        }

        if (status == ERROR_SUCCESS)
        {
            VReg__HiveRemove(pNode);
            if (hasLower)
            {
                VReg__Node* pTombstone;
                status = VReg__HiveAddChild(
                    pHive,
                    pNodeParent,
                    VReg__WStringViewOf(&keyName),
                    VREG__NO_NAME,
                    FALSE,
                    &pTombstone
                );
                if (status == ERROR_SUCCESS)
                    pTombstone->isTombstone = TRUE;
            }

            VReg__HiveMaterialize(pNodeParent);
        }
    }

    VReg__HiveUnlockWrite(pHive);

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringArrayFree(pAllocContext, &tombstones);
    VReg__WStringFree(pAllocContext, &viewPath);
    VReg__WStringFree(pAllocContext, &parentViewPath);
    VReg__WStringFree(pAllocContext, &keyName);
    VReg__WStringFree(pAllocContext, &lowerName);
    return status;
}

VREGDEF LSTATUS VReg__HiveDeleteTree(_In_ VRegHive* const pHive, _Inout_ VReg__Node* const pNode)
{
    if (!VReg__HiveLockWrite(pHive))
        return ERROR_ACCESS_DENIED;
    if (pNode->isDeleted)
    {
        VReg__HiveUnlockWrite(pHive);
        return ERROR_KEY_DELETED;
    }

    while (pNode->children.count != 0)
        VReg__HiveRemove(VREG__NODE_AT(&pNode->children, pNode->children.count - 1));

    VReg__NodeClearValues(pHive->pAllocContext, pNode);
    pNode->isOpaque = pHive->pLower != NULL;
    pNode->isTransparent = FALSE;
    pNode->isLowerBacked = FALSE;
    VReg__HiveMaterialize(pNode);
    VReg__HiveTouch(pNode);
    VReg__HiveNotify(pNode, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
    VReg__HiveUnlockWrite(pHive);
    return ERROR_SUCCESS;
}

// --- Values ---

/// Reads value @p name of @p pNode from the upper layer. @p pIsFound says
/// whether the layer decided the answer; otherwise @p pIsVisible and
/// @p pViewPath describe the lower key to ask.
static LSTATUS VReg__HiveQueryUpperValue(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const VReg__WStringView name,
    _Out_ BOOL* const pIsFound,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg__Buffer* const pData,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath
)
{
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    const VReg__Value* pValue;

    *pIsFound = TRUE;
    *pIsVisible = FALSE;
    *pdwType = REG_NONE;
    *pcbData = 0;
    pValue = pNode->isDeleted ? NULL : VReg__HiveFindValue(pNode, name);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (pValue)
    {
        if (pValue->isTombstone)
        {
            status = ERROR_FILE_NOT_FOUND;
        }
        else
        {
            *pdwType = pValue->dwType;
            *pcbData = (DWORD)pValue->data.cb;
            if (pData && !VReg__BufferAssign(pHive->pAllocContext, pData, pValue->data.pb, pValue->data.cb))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else
    {
        *pIsFound = FALSE;
        status = VReg__HiveViewOf(pHive, pNode, pIsVisible, pViewPath);
    }

    VReg__HiveUnlockRead(pHive, isLocked);
    return status;
}

VREGDEF LSTATUS VReg__HiveQueryValue(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg__Buffer* const pData
)
{
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    HKEY hLower = NULL;
    LSTATUS status = VReg__HiveQueryUpperValue(
        pHive,
        pNode,
        VReg__ViewOf(pszName),
        &isFound,
        pdwType,
        pcbData,
        pData,
        &isVisible,
        &viewPath
    );
    if (status != ERROR_SUCCESS || isFound)
        goto done;
    if (!isVisible)
    {
        status = ERROR_FILE_NOT_FOUND;
        goto done;
    }

    status = VReg__HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
    if (status != ERROR_SUCCESS)
        goto done;

    if (!pData)
    {
        *pcbData = 0;
        status = VRegQueryValueExW(
            pHive->pLower,
            hLower,
            pszName,
            NULL,
            pdwType,
            NULL,
            pcbData
        );
        goto done;
    }

    status = VReg__QueryValueOwned(pHive->pLower, hLower, pszName, pHive->pAllocContext, pdwType, pData);
    if (status == ERROR_SUCCESS)
        *pcbData = (DWORD)pData->cb;

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringFree(pHive->pAllocContext, &viewPath);
    return status;
}

static LSTATUS VReg__HiveStoreValue(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pNode,
    const VReg__WStringView name,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData,
    const BOOL isMuiText
)
{
    void* const pAllocContext = pHive->pAllocContext;
    VReg__Buffer bytes = {NULL, 0, 0};
    VReg__Value* pValue;
    LSTATUS status = ERROR_SUCCESS;

    if (!VReg__BufferAssign(pAllocContext, &bytes, pData, cbData))
        return ERROR_OUTOFMEMORY;

    if (!VReg__HiveLockWrite(pHive))
    {
        VReg__BufferFree(pAllocContext, &bytes);
        return ERROR_ACCESS_DENIED;
    }

    pValue = pNode->isDeleted ? NULL : VReg__HiveFindValue(pNode, name);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (pValue)
    {
        VReg__BufferFree(pAllocContext, &pValue->data);
        pValue->dwType = dwType;
        pValue->data = bytes;
        pValue->isTombstone = FALSE;
        pValue->isMuiText = isMuiText;
        bytes.pb = NULL;
    }
    else
    {
        VReg__Value value;
        value.name.psz = NULL;
        value.name.cch = 0;
        value.name.cchCapacity = 0;
        value.dwType = dwType;
        value.data = bytes;
        value.isTombstone = FALSE;
        value.isMuiText = isMuiText;
        if (!VReg__WStringAssign(pAllocContext, &value.name, name.pch, name.cch))
        {
            status = ERROR_OUTOFMEMORY;
        }
        else if (!VReg__ArrayAppend(pAllocContext, &pNode->values, sizeof(value), &value))
        {
            VReg__WStringFree(pAllocContext, &value.name);
            status = ERROR_OUTOFMEMORY;
        }
        else
        {
            bytes.pb = NULL;
        }
    }

    if (status == ERROR_SUCCESS)
    {
        VReg__HiveMaterialize(pNode);
        VReg__HiveTouch(pNode);
        VReg__HiveNotify(pNode, REG_NOTIFY_CHANGE_LAST_SET);
    }

    VReg__HiveUnlockWrite(pHive);
    if (bytes.pb)
        VReg__BufferFree(pAllocContext, &bytes);
    return status;
}

VREGDEF LSTATUS VReg__HiveSetValue(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const pData,
    const DWORD cbData
)
{
    return VReg__HiveStoreValue(pHive, pNode, VReg__ViewOf(pszName), dwType, pData, cbData, FALSE);
}

VREGDEF LSTATUS VReg__HiveSetMuiTextValue(
    _In_ VRegHive* const pHive,
    _Inout_ VReg__Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _In_z_ const LPCWSTR pszText
)
{
    const VReg__WStringView text = VReg__ViewOf(pszText);
    return VReg__HiveStoreValue(
        pHive,
        pNode,
        VReg__ViewOf(pszName),
        REG_SZ,
        (const BYTE*)pszText,
        (DWORD)((text.cch + 1) * sizeof(wchar_t)),
        TRUE
    );
}

VREGDEF LSTATUS VReg__HiveQueryMuiText(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _Inout_ VReg__WString* const pText
)
{
    const VReg__WStringView name = VReg__ViewOf(pszName);
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    BOOL isFound = TRUE;
    const VReg__Value* pValue;
    DWORD dwType;
    DWORD cbData;

    pValue = pNode->isDeleted ? NULL : VReg__HiveFindValue(pNode, name);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (pValue)
    {
        if (pValue->isTombstone)
        {
            status = ERROR_FILE_NOT_FOUND;
        }
        else if (!pValue->isMuiText)
        {
            status = ERROR_INVALID_DATA;
        }
        else
        {
            const SIZE_T cchText = pValue->data.cb < sizeof(wchar_t) ? 0 : pValue->data.cb / sizeof(wchar_t) - 1;
            if (!VReg__WStringAssign(pHive->pAllocContext, pText, (const wchar_t*)pValue->data.pb, cchText))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else
    {
        isFound = FALSE;
    }

    VReg__HiveUnlockRead(pHive, isLocked);
    if (isFound)
        return status;

    status = VReg__HiveQueryValue(pHive, pNode, pszName, &dwType, &cbData, NULL);
    return status == ERROR_SUCCESS ? ERROR_INVALID_DATA : status;
}

VREGDEF LSTATUS VReg__HiveDeleteValue(_In_ VRegHive* const pHive, _Inout_ VReg__Node* const pNode, _In_z_ const LPCWSTR pszName)
{
    void* const pAllocContext = pHive->pAllocContext;
    const VReg__WStringView name = VReg__ViewOf(pszName);
    BOOL isLocked;
    BOOL isInUpper = FALSE;
    BOOL isInLower = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    HKEY hLower = NULL;
    VReg__Value* pValue;
    LSTATUS status = ERROR_SUCCESS;

    if (VReg__HiveIsFrozen(pHive))
        return ERROR_ACCESS_DENIED;

    isLocked = VReg__HiveLockRead(pHive);
    pValue = pNode->isDeleted ? NULL : VReg__HiveFindValue(pNode, name);
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;
    else if (pValue && pValue->isTombstone)
        status = ERROR_FILE_NOT_FOUND;
    else
        isInUpper = pValue != NULL;
    if (status == ERROR_SUCCESS)
        status = VReg__HiveViewOf(pHive, pNode, &isVisible, &viewPath);
    VReg__HiveUnlockRead(pHive, isLocked);
    if (status != ERROR_SUCCESS)
        goto done;

    if (isVisible)
    {
        status = VReg__HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
        if (status == ERROR_SUCCESS)
        {
            const LSTATUS queryStatus = VRegQueryValueExW(
                pHive->pLower,
                hLower,
                pszName,
                NULL,
                NULL,
                NULL,
                NULL
            );
            if (queryStatus != ERROR_FILE_NOT_FOUND && !VReg__StatusExists(queryStatus))
            {
                status = queryStatus;
                goto done;
            }

            isInLower = VReg__StatusExists(queryStatus);
        }
        else if (status != ERROR_FILE_NOT_FOUND)
        {
            goto done;
        }

        status = ERROR_SUCCESS;
    }

    if (!isInUpper && !isInLower)
    {
        status = ERROR_FILE_NOT_FOUND;
        goto done;
    }

    if (!VReg__HiveLockWrite(pHive))
    {
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else
    {
        pValue = VReg__HiveFindValue(pNode, name);
        if (isInLower)
        {
            if (pValue)
            {
                VReg__BufferFree(pAllocContext, &pValue->data);
                pValue->dwType = REG_NONE;
                pValue->isTombstone = TRUE;
                pValue->isMuiText = FALSE;
            }
            else
            {
                VReg__Value tombstone;
                ZeroMemory(&tombstone, sizeof(tombstone));
                tombstone.dwType = REG_NONE;
                tombstone.isTombstone = TRUE;
                if (!VReg__WStringAssign(pAllocContext, &tombstone.name, name.pch, name.cch))
                {
                    status = ERROR_OUTOFMEMORY;
                }
                else if (!VReg__ArrayAppend(pAllocContext, &pNode->values, sizeof(tombstone), &tombstone))
                {
                    VReg__WStringFree(pAllocContext, &tombstone.name);
                    status = ERROR_OUTOFMEMORY;
                }
            }
        }
        else if (pValue)
        {
            const SIZE_T index = (SIZE_T)(pValue - &VREG__VALUE_AT(&pNode->values, 0));
            VReg__ValueFree(pAllocContext, pValue);
            VReg__ArrayRemoveAt(&pNode->values, sizeof(*pValue), index);
        }

        if (status == ERROR_SUCCESS)
        {
            VReg__HiveMaterialize(pNode);
            VReg__HiveTouch(pNode);
            VReg__HiveNotify(pNode, REG_NOTIFY_CHANGE_LAST_SET);
        }
    }

    VReg__HiveUnlockWrite(pHive);

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringFree(pAllocContext, &viewPath);
    return status;
}

// --- Enumeration ---

/// The upper layer's part of @ref VReg__HiveEnumKey. @p pIsFound says the
/// answer is complete. Otherwise, when @p pIsClassVisible, the entry is an
/// upper key made from a lower one at @p pClassPath whose class is still to
/// be read; and when not, the lower key at @p pViewPath (if @p pIsVisible)
/// continues the enumeration past @p pcUpper upper entries, with the names in
/// @p pShadowed hidden.
static LSTATUS VReg__HiveEnumUpperKey(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg__WString* const pName,
    _Inout_ VReg__WString* const pClassName,
    _Out_ FILETIME* const pLastWrite,
    _Out_ BOOL* const pIsFound,
    _Out_ BOOL* const pIsClassVisible,
    _Inout_ VReg__WString* const pClassPath,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath,
    _Out_ DWORD* const pcUpper,
    _Inout_ VReg__Array* const pShadowed
)
{
    void* const pAllocContext = pHive->pAllocContext;
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    DWORD cUpper = 0;
    SIZE_T i;

    *pIsFound = FALSE;
    *pIsClassVisible = FALSE;
    *pIsVisible = FALSE;
    ZeroMemory(pLastWrite, sizeof(*pLastWrite));
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;

    for (i = 0; status == ERROR_SUCCESS && !*pIsFound && !*pIsClassVisible && i < pNode->children.count; ++i)
    {
        const VReg__Node* const pChild = VREG__NODE_AT(&pNode->children, i);

        // A transparent child stands for a lower key; it is listed with the
        // lower ones.
        if (pChild->isTransparent)
            continue;

        if (!pChild->isTombstone && cUpper++ == dwIndex)
        {
            if (
                !VReg__WStringAssign(pAllocContext, pName, VReg__WStringText(&pChild->name), pChild->name.cch)
                || !VReg__WStringAssign(
                    pAllocContext,
                    pClassName,
                    VReg__WStringText(&pChild->className),
                    pChild->className.cch
                )
            )
            {
                status = ERROR_OUTOFMEMORY;
                break;
            }

            *pLastWrite = pChild->lastWrite;
            if (!pChild->isLowerBacked)
            {
                *pIsFound = TRUE;
                break;
            }

            status = VReg__HiveViewOf(pHive, pChild, pIsClassVisible, pClassPath);
            if (status == ERROR_SUCCESS && !*pIsClassVisible)
                *pIsFound = TRUE;
            break;
        }

        if (!VReg__WStringArrayInsertSorted(pAllocContext, pShadowed, VReg__WStringViewOf(&pChild->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS && !*pIsFound && !*pIsClassVisible)
        status = VReg__HiveViewOf(pHive, pNode, pIsVisible, pViewPath);

    VReg__HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREGDEF LSTATUS VReg__HiveEnumKey(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg__WString* const pName,
    _Inout_ VReg__WString* const pClassName,
    _Out_ FILETIME* const pLastWrite
)
{
    void* const pAllocContext = pHive->pAllocContext;
    BOOL isFound = FALSE;
    BOOL isClassVisible = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString classPath = {NULL, 0, 0};
    VReg__WString viewPath = {NULL, 0, 0};
    VReg__WString nameBuffer = {NULL, 0, 0};
    VReg__WString classBuffer = {NULL, 0, 0};
    VReg__Array shadowed = {NULL, 0, 0};
    DWORD cUpper = 0;
    DWORD cchMaxClass = 0;
    DWORD dwSkip;
    HKEY hLower = NULL;
    DWORD i;
    LSTATUS status = VReg__HiveEnumUpperKey(
        pHive,
        pNode,
        dwIndex,
        pName,
        pClassName,
        pLastWrite,
        &isFound,
        &isClassVisible,
        &classPath,
        &isVisible,
        &viewPath,
        &cUpper,
        &shadowed
    );
    if (status != ERROR_SUCCESS || isFound)
        goto done;

    // An upper key that was made from a lower one has no class of its own.
    if (isClassVisible)
    {
        status = VReg__HiveQueryLowerClass(pHive, &classPath, pClassName);
        goto done;
    }

    if (!isVisible)
    {
        status = ERROR_NO_MORE_ITEMS;
        goto done;
    }

    status = VReg__HiveOpenLower(pHive, &viewPath, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hLower);
    if (status != ERROR_SUCCESS)
    {
        if (status == ERROR_FILE_NOT_FOUND)
            status = ERROR_NO_MORE_ITEMS;
        goto done;
    }

    status = VRegQueryInfoKeyW(
        pHive->pLower,
        hLower,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        &cchMaxClass,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );
    if (status != ERROR_SUCCESS)
        goto done;

    if (
        !VReg__WStringResize(pAllocContext, &nameBuffer, VREG__INITIAL_NAME_CHARS)
        || !VReg__WStringResize(pAllocContext, &classBuffer, cchMaxClass + 1)
    )
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    dwSkip = dwIndex - cUpper;
    for (i = 0;; ++i)
    {
        DWORD cchName = (DWORD)nameBuffer.cchCapacity;
        DWORD cchClass = (DWORD)classBuffer.cchCapacity;
        FILETIME ft;
        VReg__WStringView lowerName;
        ZeroMemory(&ft, sizeof(ft));
        status = VRegEnumKeyExW(
            pHive->pLower,
            hLower,
            i,
            nameBuffer.psz,
            &cchName,
            NULL,
            classBuffer.psz,
            &cchClass,
            &ft
        );
        if (status != ERROR_SUCCESS)
            goto done;

        lowerName.pch = nameBuffer.psz;
        lowerName.cch = cchName;
        if (VReg__WStringArrayContainsSorted(&shadowed, lowerName) || dwSkip-- != 0)
            continue;

        if (
            !VReg__WStringAssign(pAllocContext, pName, lowerName.pch, lowerName.cch)
            || !VReg__WStringAssign(pAllocContext, pClassName, classBuffer.psz, cchClass)
        )
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        *pLastWrite = ft;
        status = ERROR_SUCCESS;
        goto done;
    }

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringArrayFree(pAllocContext, &shadowed);
    VReg__WStringFree(pAllocContext, &classPath);
    VReg__WStringFree(pAllocContext, &viewPath);
    VReg__WStringFree(pAllocContext, &nameBuffer);
    VReg__WStringFree(pAllocContext, &classBuffer);
    return status;
}

/// The upper layer's part of @ref VReg__HiveEnumValue, like
/// @ref VReg__HiveEnumUpperKey without the class step.
static LSTATUS VReg__HiveEnumUpperValue(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg__WString* const pName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg__Buffer* const pData,
    _Out_ BOOL* const pIsFound,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath,
    _Out_ DWORD* const pcUpper,
    _Inout_ VReg__Array* const pShadowed
)
{
    void* const pAllocContext = pHive->pAllocContext;
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    DWORD cUpper = 0;
    SIZE_T i;

    *pIsFound = FALSE;
    *pIsVisible = FALSE;
    *pdwType = REG_NONE;
    *pcbData = 0;
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;

    for (i = 0; status == ERROR_SUCCESS && !*pIsFound && i < pNode->values.count; ++i)
    {
        const VReg__Value* const pValue = &VREG__VALUE_AT(&pNode->values, i);
        if (!pValue->isTombstone && cUpper++ == dwIndex)
        {
            *pIsFound = TRUE;
            if (
                !VReg__WStringAssign(pAllocContext, pName, VReg__WStringText(&pValue->name), pValue->name.cch)
                || (pData && !VReg__BufferAssign(pAllocContext, pData, pValue->data.pb, pValue->data.cb))
            )
            {
                status = ERROR_OUTOFMEMORY;
                break;
            }

            *pdwType = pValue->dwType;
            *pcbData = (DWORD)pValue->data.cb;
            break;
        }

        if (!VReg__WStringArrayInsertSorted(pAllocContext, pShadowed, VReg__WStringViewOf(&pValue->name)))
            status = ERROR_OUTOFMEMORY;
    }

    if (status == ERROR_SUCCESS && !*pIsFound)
        status = VReg__HiveViewOf(pHive, pNode, pIsVisible, pViewPath);

    VReg__HiveUnlockRead(pHive, isLocked);
    *pcUpper = cUpper;
    return status;
}

VREGDEF LSTATUS VReg__HiveEnumValue(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const DWORD dwIndex,
    _Inout_ VReg__WString* const pName,
    _Out_ LPDWORD const pdwType,
    _Out_ LPDWORD const pcbData,
    _Inout_opt_ VReg__Buffer* const pData
)
{
    void* const pAllocContext = pHive->pAllocContext;
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    VReg__WString sizeName = {NULL, 0, 0};
    VReg__Array shadowed = {NULL, 0, 0};
    DWORD cUpper = 0;
    DWORD dwSkip;
    HKEY hLower = NULL;
    DWORD i;
    LSTATUS status = VReg__HiveEnumUpperValue(
        pHive,
        pNode,
        dwIndex,
        pName,
        pdwType,
        pcbData,
        pData,
        &isFound,
        &isVisible,
        &viewPath,
        &cUpper,
        &shadowed
    );
    if (status != ERROR_SUCCESS || isFound)
        goto done;
    if (!isVisible)
    {
        status = ERROR_NO_MORE_ITEMS;
        goto done;
    }

    status = VReg__HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
    if (status != ERROR_SUCCESS)
    {
        if (status == ERROR_FILE_NOT_FOUND)
            status = ERROR_NO_MORE_ITEMS;
        goto done;
    }

    dwSkip = dwIndex - cUpper;
    for (i = 0;; ++i)
    {
        DWORD cchName;
        status = VReg__EnumValueOwned(pHive->pLower, hLower, i, pAllocContext, pName, pdwType, NULL);
        if (status != ERROR_SUCCESS)
            goto done;

        if (VReg__WStringArrayContainsSorted(&shadowed, VReg__WStringViewOf(pName)) || dwSkip-- != 0)
            continue;

        if (pData)
        {
            // The scan fetched no data; enumerate the matching entry again
            // with it, so name, type and data all come from the one call,
            // should the entry have changed since the scan.
            status = VReg__EnumValueOwned(pHive->pLower, hLower, i, pAllocContext, pName, pdwType, pData);
            if (status == ERROR_SUCCESS)
                *pcbData = (DWORD)pData->cb;
            goto done;
        }

        if (!VReg__WStringResize(pAllocContext, &sizeName, pName->cch))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        cchName = (DWORD)(pName->cch + 1);
        *pcbData = 0;
        status = VRegEnumValueW(
            pHive->pLower,
            hLower,
            i,
            sizeName.psz,
            &cchName,
            NULL,
            NULL,
            NULL,
            pcbData
        );
        goto done;
    }

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringArrayFree(pAllocContext, &shadowed);
    VReg__WStringFree(pAllocContext, &viewPath);
    VReg__WStringFree(pAllocContext, &sizeName);
    return status;
}

/// The upper layer's part of @ref VReg__HiveQueryInfo: the counts and
/// maxima of the node's own entries into @p pInfo, the names that shadow
/// lower entries, and the node's relation to its lower key.
static LSTATUS VReg__HiveQueryUpperInfo(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _Inout_ VReg__KeyInfo* const pInfo,
    _Inout_ VReg__Array* const pShadowedKeys,
    _Inout_ VReg__Array* const pShadowedValues,
    _Out_ BOOL* const pIsTransparent,
    _Out_ BOOL* const pIsLowerBacked,
    _Out_ BOOL* const pIsVisible,
    _Inout_ VReg__WString* const pViewPath
)
{
    void* const pAllocContext = pHive->pAllocContext;
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    LSTATUS status = ERROR_SUCCESS;
    SIZE_T i;

    *pIsTransparent = FALSE;
    *pIsLowerBacked = FALSE;
    *pIsVisible = FALSE;
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;

    for (i = 0; status == ERROR_SUCCESS && i < pNode->children.count; ++i)
    {
        const VReg__Node* const pChild = VREG__NODE_AT(&pNode->children, i);
        if (pChild->isTransparent)
            continue;

        if (!VReg__WStringArrayInsertSorted(pAllocContext, pShadowedKeys, VReg__WStringViewOf(&pChild->name)))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }
        if (pChild->isTombstone)
            continue;

        ++pInfo->cSubKeys;
        pInfo->cchMaxSubKey = VReg__MaxDword(pInfo->cchMaxSubKey, (DWORD)pChild->name.cch);
        pInfo->cchMaxClass = VReg__MaxDword(pInfo->cchMaxClass, (DWORD)pChild->className.cch);
    }

    for (i = 0; status == ERROR_SUCCESS && i < pNode->values.count; ++i)
    {
        const VReg__Value* const pValue = &VREG__VALUE_AT(&pNode->values, i);
        if (!VReg__WStringArrayInsertSorted(pAllocContext, pShadowedValues, VReg__WStringViewOf(&pValue->name)))
        {
            status = ERROR_OUTOFMEMORY;
            break;
        }
        if (pValue->isTombstone)
            continue;

        ++pInfo->cValues;
        pInfo->cchMaxValueName = VReg__MaxDword(pInfo->cchMaxValueName, (DWORD)pValue->name.cch);
        pInfo->cbMaxValue = VReg__MaxDword(pInfo->cbMaxValue, (DWORD)pValue->data.cb);
    }

    if (status == ERROR_SUCCESS)
    {
        *pIsTransparent = pNode->isTransparent;
        *pIsLowerBacked = pNode->isLowerBacked;
        pInfo->lastWrite = pNode->lastWrite;
        const VReg__WStringView className = VReg__WStringViewOf(&pNode->className);
        if (!VReg__WStringAssign(pAllocContext, &pInfo->className, className.pch, className.cch))
            status = ERROR_OUTOFMEMORY;
    }
    if (status == ERROR_SUCCESS)
        status = VReg__HiveViewOf(pHive, pNode, pIsVisible, pViewPath);

    VReg__HiveUnlockRead(pHive, isLocked);
    return status;
}

VREGDEF LSTATUS VReg__HiveQueryInfo(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _Inout_ VReg__KeyInfo* const pInfo
)
{
    void* const pAllocContext = pHive->pAllocContext;
    VReg__Array shadowedKeys = {NULL, 0, 0};
    VReg__Array shadowedValues = {NULL, 0, 0};
    BOOL isTransparent = FALSE;
    BOOL isLowerBacked = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    VReg__WString lowerClass = {NULL, 0, 0};
    HKEY hLower = NULL;
    DWORD cchClassBuffer = VREG__INITIAL_NAME_CHARS;
    DWORD cSubKeys = 0;
    DWORD cchMaxSubKey = 0;
    DWORD cchMaxClass = 0;
    DWORD cValues = 0;
    DWORD cchMaxValueName = 0;
    DWORD cbMaxValue = 0;
    FILETIME lastWrite;
    SIZE_T i;
    LSTATUS status;

    pInfo->cSubKeys = 0;
    pInfo->cchMaxSubKey = 0;
    pInfo->cchMaxClass = 0;
    pInfo->cValues = 0;
    pInfo->cchMaxValueName = 0;
    pInfo->cbMaxValue = 0;
    ZeroMemory(&lastWrite, sizeof(lastWrite));

    status = VReg__HiveQueryUpperInfo(
        pHive,
        pNode,
        pInfo,
        &shadowedKeys,
        &shadowedValues,
        &isTransparent,
        &isLowerBacked,
        &isVisible,
        &viewPath
    );
    if (status != ERROR_SUCCESS || !isVisible)
        goto done;

    status = VReg__HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &hLower);
    if (status == ERROR_FILE_NOT_FOUND)
    {
        status = ERROR_SUCCESS;
        goto done;
    }
    if (status != ERROR_SUCCESS)
        goto done;

    for (;;)
    {
        DWORD cchClass = cchClassBuffer;
        if (!VReg__WStringResize(pAllocContext, &lowerClass, cchClassBuffer))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        status = VRegQueryInfoKeyW(
            pHive->pLower,
            hLower,
            lowerClass.psz,
            &cchClass,
            NULL,
            &cSubKeys,
            &cchMaxSubKey,
            &cchMaxClass,
            &cValues,
            &cchMaxValueName,
            &cbMaxValue,
            NULL,
            &lastWrite
        );
        if (status == ERROR_MORE_DATA || status == ERROR_INSUFFICIENT_BUFFER)
        {
            cchClassBuffer *= 2;
            continue;
        }
        if (status != ERROR_SUCCESS)
            goto done;

        VReg__WStringTruncate(&lowerClass, cchClass);
        break;
    }

    // An upper entry that shadows a lower one is counted once.
    for (i = 0; i < shadowedKeys.count; ++i)
    {
        HKEY hChild = NULL;
        const LSTATUS childStatus = VRegOpenKeyExW(
            pHive->pLower,
            hLower,
            VReg__WStringText(&VREG__NAME_AT(&shadowedKeys, i)),
            0,
            KEY_QUERY_VALUE | pHive->samLowerView,
            &hChild
        );
        VReg__HiveCloseLower(pHive, childStatus == ERROR_SUCCESS ? hChild : NULL);
        if (childStatus != ERROR_FILE_NOT_FOUND && !VReg__StatusExists(childStatus))
        {
            status = childStatus;
            goto done;
        }
        if (VReg__StatusExists(childStatus))
            --cSubKeys;
    }

    for (i = 0; i < shadowedValues.count; ++i)
    {
        const LSTATUS valueStatus = VRegQueryValueExW(
            pHive->pLower,
            hLower,
            VReg__WStringText(&VREG__NAME_AT(&shadowedValues, i)),
            NULL,
            NULL,
            NULL,
            NULL
        );
        if (valueStatus != ERROR_FILE_NOT_FOUND && !VReg__StatusExists(valueStatus))
        {
            status = valueStatus;
            goto done;
        }
        if (VReg__StatusExists(valueStatus))
            --cValues;
    }

    pInfo->cSubKeys += cSubKeys;
    pInfo->cValues += cValues;
    pInfo->cchMaxSubKey = VReg__MaxDword(pInfo->cchMaxSubKey, cchMaxSubKey);
    pInfo->cchMaxClass = VReg__MaxDword(pInfo->cchMaxClass, cchMaxClass);
    pInfo->cchMaxValueName = VReg__MaxDword(pInfo->cchMaxValueName, cchMaxValueName);
    pInfo->cbMaxValue = VReg__MaxDword(pInfo->cbMaxValue, cbMaxValue);
    if (isLowerBacked && !VReg__WStringAssign(pAllocContext, &pInfo->className, lowerClass.psz, lowerClass.cch))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }
    if (isTransparent)
        pInfo->lastWrite = lastWrite;

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringArrayFree(pAllocContext, &shadowedKeys);
    VReg__WStringArrayFree(pAllocContext, &shadowedValues);
    VReg__WStringFree(pAllocContext, &viewPath);
    VReg__WStringFree(pAllocContext, &lowerClass);
    return status;
}

// --- The rest ---

/// Visits @p pNode and everything below it, leaving out a transparent node
/// without values (the way to a lower key, which only the root can be while
/// it has upper keys below it). @p pPath is the node's path, extended and
/// restored for each child.
static LSTATUS VReg__HiveListUpper(
    _In_ VRegHive* const pHive,
    _In_ const VReg__Node* const pNode,
    _Inout_ VReg__WString* const pPath,
    _In_ const VReg__UpperVisitor pfnVisit,
    _In_opt_ void* const pContext
)
{
    void* const pAllocContext = pHive->pAllocContext;
    const BOOL isBare = pNode->isTransparent && pNode->values.count == 0;
    const SIZE_T cchPath = pPath->cch;
    VReg__UpperRecord record;
    LSTATUS status;
    SIZE_T i;

    ZeroMemory(&record, sizeof(record));
    record.path = VReg__WStringViewOf(pPath);
    if (!isBare)
    {
        record.kind = VREG__UPPER_KEY;
        record.isOpaque = pNode->isOpaque;
        status = pfnVisit(pContext, &record);
        if (status != ERROR_SUCCESS)
            return status;
    }

    for (i = 0; i < pNode->values.count; ++i)
    {
        const VReg__Value* const pValue = &VREG__VALUE_AT(&pNode->values, i);
        ZeroMemory(&record, sizeof(record));
        record.kind = VREG__UPPER_VALUE;
        record.path = VReg__WStringViewOf(pPath);
        record.name = VReg__WStringViewOf(&pValue->name);
        record.dwType = pValue->dwType;
        record.pData = pValue->data.pb;
        record.cbData = (DWORD)pValue->data.cb;
        record.isTombstone = pValue->isTombstone;
        status = pfnVisit(pContext, &record);
        if (status != ERROR_SUCCESS)
            return status;
    }

    for (i = 0; i < pNode->children.count; ++i)
    {
        const VReg__Node* const pChild = VREG__NODE_AT(&pNode->children, i);
        if (
            (pPath->cch != 0 && !VReg__WStringAppendChar(pAllocContext, pPath, L'\\'))
            || !VReg__WStringAppend(pAllocContext, pPath, VReg__WStringText(&pChild->name), pChild->name.cch)
        )
            return ERROR_OUTOFMEMORY;

        if (pChild->isTombstone)
        {
            ZeroMemory(&record, sizeof(record));
            record.kind = VREG__UPPER_KEY;
            record.path = VReg__WStringViewOf(pPath);
            record.isTombstone = TRUE;
            status = pfnVisit(pContext, &record);
        }
        else
        {
            status = VReg__HiveListUpper(pHive, pChild, pPath, pfnVisit, pContext);
        }

        if (status != ERROR_SUCCESS)
            return status;

        VReg__WStringTruncate(pPath, cchPath);
    }

    return ERROR_SUCCESS;
}

VREGDEF LSTATUS VReg__HiveSnapshotUpper(
    _In_ VRegHive* const pHive,
    _In_ const VReg__UpperVisitor pfnVisit,
    _In_opt_ void* const pContext
)
{
    VReg__WString path = {NULL, 0, 0};
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    const LSTATUS status = VReg__HiveListUpper(pHive, pHive->pRoot, &path, pfnVisit, pContext);
    VReg__HiveUnlockRead(pHive, isLocked);
    VReg__WStringFree(pHive->pAllocContext, &path);
    return status;
}

VREGDEF LSTATUS VReg__HiveCheckKey(_In_ VRegHive* const pHive, _In_ VReg__Node* const pNode)
{
    const BOOL isLocked = VReg__HiveLockRead(pHive);
    const LSTATUS status = pNode->isDeleted ? ERROR_KEY_DELETED : ERROR_SUCCESS;
    VReg__HiveUnlockRead(pHive, isLocked);
    return status;
}

VREGDEF LSTATUS VReg__HiveWatch(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    const BOOL bWatchSubtree,
    const DWORD dwFilter,
    _In_ const HANDLE hEvent
)
{
    HANDLE hOwn = NULL;
    BOOL isLocked;
    LSTATUS status = ERROR_SUCCESS;

    if (!(dwFilter & (REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET)))
        return ERROR_SUCCESS;

    if (!DuplicateHandle(GetCurrentProcess(), hEvent, GetCurrentProcess(), &hOwn, 0, FALSE, DUPLICATE_SAME_ACCESS))
        return GetLastError();

    isLocked = VReg__HiveLockWrite(pHive);
    if (pNode->isDeleted)
    {
        status = ERROR_KEY_DELETED;
    }
    else if (isLocked)
    {
        VReg__ChangeWatch watch;
        watch.hEvent = hOwn;
        watch.isSubtree = bWatchSubtree != FALSE;
        watch.dwFilter = dwFilter;
        if (VReg__ArrayAppend(pHive->pAllocContext, &pNode->watches, sizeof(watch), &watch))
            hOwn = NULL;
        else
            status = ERROR_OUTOFMEMORY;
    }

    if (isLocked)
        VReg__HiveUnlockWrite(pHive);
    if (hOwn)
        CloseHandle(hOwn);
    return status;
}

VREGDEF LSTATUS VReg__HiveOpenLowerValueKey(
    _In_ VRegHive* const pHive,
    _In_ VReg__Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _Out_ PHKEY const phLower
)
{
    const VReg__WStringView name = VReg__ViewOf(pszName);
    BOOL isLocked;
    BOOL isFound = FALSE;
    BOOL isVisible = FALSE;
    VReg__WString viewPath = {NULL, 0, 0};
    HKEY hLower = NULL;
    LSTATUS status = ERROR_SUCCESS;

    *phLower = NULL;
    isLocked = VReg__HiveLockRead(pHive);
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;
    else if (VReg__HiveFindValue(pNode, name))
        isFound = TRUE;
    else
        status = VReg__HiveViewOf(pHive, pNode, &isVisible, &viewPath);
    VReg__HiveUnlockRead(pHive, isLocked);
    if (status != ERROR_SUCCESS || isFound || !isVisible)
        goto done;

    status = VReg__HiveOpenLower(pHive, &viewPath, KEY_QUERY_VALUE, &hLower);
    if (status != ERROR_SUCCESS)
    {
        if (status == ERROR_FILE_NOT_FOUND)
            status = ERROR_SUCCESS;
        goto done;
    }

    status = VRegQueryValueExW(
        pHive->pLower,
        hLower,
        pszName,
        NULL,
        NULL,
        NULL,
        NULL
    );
    if (status == ERROR_FILE_NOT_FOUND)
    {
        status = ERROR_SUCCESS;
        goto done;
    }
    if (!VReg__StatusExists(status))
        goto done;

    *phLower = hLower;
    hLower = NULL;
    status = ERROR_SUCCESS;

done:
    VReg__HiveCloseLower(pHive, hLower);
    VReg__WStringFree(pHive->pAllocContext, &viewPath);
    return status;
}

VREGDEF LSTATUS WINAPI VRegSetHiveMuiTextValue(
    _In_ VRegHive* const pHive,
    _In_opt_z_ const LPCWSTR pszPath,
    _In_opt_z_ const LPCWSTR pszName,
    _In_z_ const LPCWSTR pszText
)
{
    VReg__Array components = {NULL, 0, 0};
    VReg__OpenResult opened;
    DWORD dwDisposition = 0;
    LSTATUS status = VReg__SplitPath(pHive->pAllocContext, VReg__ViewOf(pszPath), &components);
    if (status != ERROR_SUCCESS)
    {
        VReg__ArrayFree(pHive->pAllocContext, &components);
        return status;
    }

    status = VReg__HiveCreateKey(
        pHive,
        pHive->pRoot,
        (const VReg__WStringView*)components.p,
        components.count,
        L"",
        0,
        KEY_SET_VALUE,
        &opened,
        &dwDisposition
    );
    VReg__ArrayFree(pHive->pAllocContext, &components);
    if (status != ERROR_SUCCESS)
        return status;

    // Frozen: the open answered with a lower key, which cannot take the value.
    if (!opened.pNode)
    {
        VReg__HiveCloseLower(pHive, opened.hLower);
        return ERROR_ACCESS_DENIED;
    }

    return VReg__HiveSetMuiTextValue(pHive, opened.pNode, pszName ? pszName : L"", pszText);
}
