#include <VReg/virtual_registry.h>

#include <VReg/handle.h>

#include "core/alloc.h"
#include "core/buffer.h"
#include "core/export_table.h"
#include "core/handle_space.h"
#include "core/hive.h"
#include "core/node.h"
#include "core/path.h"
#include "core/rrf.h"
#include "core/status.h"
#include "core/value_type.h"

#include <sddl.h>

typedef struct VReg__MountPoint
{
    HKEY hRoot;

    /// `VReg__WString` components.
    VReg__Array path;

    VRegHive* pHive;

    /// @ref KEY_WOW64_64KEY, @ref KEY_WOW64_32KEY or 0 for both views.
    REGSAM samView;

    /// The native key path as `VReg__WString` components without
    /// `WOW6432Node`; empty when the mount has none.
    VReg__Array nativePath;
} VReg__MountPoint;

/// A mount table is replaced, never changed: a new one copies the mount points
/// of the old (sharing their strings, which only the newest table frees) and
/// adds one. Old tables stay reachable through @ref pPrevious until the
/// registry is destroyed, since a call in flight may still read them.
typedef struct VReg__MountTable VReg__MountTable;

struct VReg__MountTable
{
    /// `VReg__MountPoint`.
    VReg__Array mounts;

    VReg__MountTable* pPrevious;
};

typedef struct VReg__VirtualRegistry
{
    VRegRegistry object;
    void* pAllocContext;

    /// Null for a registry held entirely in memory.
    VRegRegistry* pBase;

    VRegNtQueryKeyFunction pfnNtQueryKey;
    VReg__MountTable* volatile pMounts;
    SRWLOCK mountLock;
} VReg__VirtualRegistry;

/// Where a key handle plus a subkey path lands.
typedef struct VReg__Target
{
    /// The node, when the key is inside a mount.
    VReg__Node* pNode;

    /// The hive of @ref pNode.
    VRegHive* pHive;

    /// The components below @ref pNode still to walk.
    const VReg__WStringView* pRest;
    SIZE_T cRest;

    /// Whether the call goes to the base registry, because the key is
    /// outside every mount.
    BOOL isBase;

    LSTATUS status;

    /// `VReg__WStringView` into the caller's subkey.
    VReg__Array components;

    /// `VReg__WStringView` into @ref nativeText and @ref components: the
    /// native path of a real key plus the subkey.
    VReg__Array full;
    VReg__WString nativeText;
} VReg__Target;

#define VREG__MOUNT_AT(pArray, index) VREG__AT(pArray, VReg__MountPoint, index)
#define VREG__STRING_AT(pArray, index) VREG__AT(pArray, VReg__WString, index)
#define VREG__VIEW_AT(pArray, index) VREG__AT(pArray, VReg__WStringView, index)

/// The view of the process: what a call without a view flag asks for.
#define VREG__PROCESS_VIEW (sizeof(void*) == 8 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY)

#define VREG__KEY_NAME_INFORMATION_CLASS ((ULONG)3)
#define VREG__NT_STATUS_BUFFER_OVERFLOW ((LONG)0x80000005)
#define VREG__NT_STATUS_BUFFER_TOO_SMALL ((LONG)0xC0000023)

#define VREG__KNOWN_NOTIFY_FILTERS \
    (REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES | REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_SECURITY \
        | REG_NOTIFY_THREAD_AGNOSTIC)

static VReg__VirtualRegistry* VReg__VirtualOf(_In_ VRegRegistry* const pThis)
{
    return (VReg__VirtualRegistry*)pThis;
}

/// @p psz, or an empty string for null: the hive layer takes no null names.
static LPCWSTR VReg__OrEmpty(_In_opt_z_ const LPCWSTR psz)
{
    return psz ? psz : L"";
}

/// Copies a value into a caller buffer of @p cbBuffer bytes that is known to
/// hold it. Native adds a terminator after a string that lacks one, when it
/// fits, without counting it in the size.
static void VReg__CopyValue(
    _Out_writes_bytes_(cbBuffer) const LPBYTE pbBuffer,
    const DWORD cbBuffer,
    const DWORD dwType,
    _In_reads_bytes_opt_(cbValue) const BYTE* const pValue,
    const DWORD cbValue
)
{
    DWORD cbWhole;
    BOOL isTerminated;
    if (cbValue == 0)
        return;

    // A null value is legal only when it is empty.
    _Analysis_assume_(pValue != NULL);
    CopyMemory(pbBuffer, pValue, cbValue);
    if (!VReg__IsStringType(dwType))
        return;

    cbWhole = cbValue / sizeof(wchar_t) * sizeof(wchar_t);
    if (cbWhole == 0)
        return;

    isTerminated = pValue[cbWhole - 2] == 0 && pValue[cbWhole - 1] == 0;
    if (!isTerminated && cbWhole + sizeof(wchar_t) <= cbBuffer)
        ZeroMemory(pbBuffer + cbWhole, sizeof(wchar_t));
}

static BOOL VReg__IsWow6432Node(const VReg__WStringView name)
{
    return VReg__CompareNames(name, VReg__ViewOf(L"WOW6432Node")) == 0;
}

/// The view a call with @p samDesired is of: the one it names, else the
/// process's.
static REGSAM VReg__CallView(const REGSAM samDesired)
{
    if (samDesired & KEY_WOW64_64KEY)
        return KEY_WOW64_64KEY;
    if (samDesired & KEY_WOW64_32KEY)
        return KEY_WOW64_32KEY;

    return VREG__PROCESS_VIEW;
}

/// Whether a mount of view @p samMount answers a call of view @p samCall.
static BOOL VReg__Admits(const REGSAM samMount, const REGSAM samCall)
{
    return samMount == 0 || samMount == samCall;
}

/// The string form of the current user's SID; empty when it cannot be read.
static LSTATUS VReg__CurrentUserSid(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* pSid)
{
    HANDLE hToken = NULL;
    DWORD cbUser = 0;
    VReg__Buffer user = {NULL, 0, 0};
    LSTATUS status = ERROR_SUCCESS;

    if (!VReg__WStringResize(pAllocContext, pSid, 0))
        return ERROR_OUTOFMEMORY;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return ERROR_SUCCESS;

    GetTokenInformation(hToken, TokenUser, NULL, 0, &cbUser);
    if (cbUser != 0)
    {
        if (!VReg__BufferResize(pAllocContext, &user, cbUser))
        {
            status = ERROR_OUTOFMEMORY;
        }
        else if (GetTokenInformation(hToken, TokenUser, user.pb, cbUser, &cbUser))
        {
            LPWSTR pszSid = NULL;
            if (ConvertSidToStringSidW(((const TOKEN_USER*)user.pb)->User.Sid, &pszSid))
            {
                if (!VReg__WStringAssign(pAllocContext, pSid, pszSid, VReg__ViewOf(pszSid).cch))
                    status = ERROR_OUTOFMEMORY;
                LocalFree(pszSid);
            }
        }
    }

    VReg__BufferFree(pAllocContext, &user);
    CloseHandle(hToken);
    return status;
}

/// Appends the native path of a predefined root to @p pPath as components;
/// nothing for a root with none.
static LSTATUS VReg__NativeRootPath(
    _In_opt_ void* const pAllocContext,
    const HKEY hRoot,
    _Inout_ VReg__Array* const pPath
)
{
    const wchar_t* pszSecond = NULL;
    VReg__WString sid = {NULL, 0, 0};
    LSTATUS status = ERROR_SUCCESS;

    if (hRoot == HKEY_LOCAL_MACHINE)
    {
        pszSecond = L"MACHINE";
    }
    else if (hRoot == HKEY_USERS)
    {
        pszSecond = L"USER";
    }
    else if (hRoot == HKEY_CURRENT_USER)
    {
        status = VReg__CurrentUserSid(pAllocContext, &sid);
        if (status == ERROR_SUCCESS && sid.cch != 0)
            pszSecond = L"USER";
    }

    if (status == ERROR_SUCCESS && pszSecond)
    {
        if (
            !VReg__WStringArrayAppend(pAllocContext, pPath, VReg__ViewOf(L"REGISTRY"))
            || !VReg__WStringArrayAppend(pAllocContext, pPath, VReg__ViewOf(pszSecond))
            || (sid.cch != 0 && !VReg__WStringArrayAppend(pAllocContext, pPath, VReg__WStringViewOf(&sid)))
        )
            status = ERROR_OUTOFMEMORY;
    }

    VReg__WStringFree(pAllocContext, &sid);
    return status;
}

/// Whether @p pFull, with `WOW6432Node` components skipped, starts with
/// @p pMountPath; @p piRest then indexes the first component past the match
/// and @p pIsWow says a `WOW6432Node` was skipped.
static BOOL VReg__MatchesNative(
    _In_ const VReg__Array* const pMountPath,
    _In_ const VReg__Array* const pFull,
    _Out_ BOOL* const pIsWow,
    _Out_ SIZE_T* const piRest
)
{
    SIZE_T iMount = 0;
    SIZE_T i = 0;
    *pIsWow = FALSE;
    *piRest = 0;
    for (; i < pFull->count && iMount < pMountPath->count; ++i)
    {
        const VReg__WStringView component = VREG__VIEW_AT(pFull, i);
        if (VReg__IsWow6432Node(component))
        {
            *pIsWow = TRUE;
            continue;
        }
        if (VReg__CompareNames(component, VReg__WStringViewOf(&VREG__STRING_AT(pMountPath, iMount))) != 0)
            return FALSE;

        ++iMount;
    }

    *piRest = i;
    return iMount == pMountPath->count;
}

// --- The mount table ---

/// The current table, or null while nothing is mounted. Lock-free.
static _Ret_maybenull_ const VReg__MountTable* VReg__MountsOf(_In_ const VReg__VirtualRegistry* const pThis)
{
    return (const VReg__MountTable*)ReadPointerAcquire((PVOID volatile*)&pThis->pMounts);
}

/// A mount of @p pHive here; null when it is not mounted.
static _Ret_maybenull_ const VReg__MountPoint* VReg__MountOf(
    _In_ const VReg__VirtualRegistry* const pThis,
    _In_ const VRegHive* const pHive
)
{
    const VReg__MountTable* const pTable = VReg__MountsOf(pThis);
    SIZE_T i;
    if (!pTable)
        return NULL;

    for (i = 0; i < pTable->mounts.count; ++i)
    {
        const VReg__MountPoint* const pMount = &VREG__MOUNT_AT(&pTable->mounts, i);
        if (pMount->pHive == pHive)
            return pMount;
    }

    return NULL;
}

static void VReg__MountPointFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__MountPoint* const pMount)
{
    VReg__WStringArrayFree(pAllocContext, &pMount->path);
    VReg__WStringArrayFree(pAllocContext, &pMount->nativePath);
}

/// Whether the mount @p pMount is of @p hRoot at the path @p pPath, by the
/// registry's own name comparison.
static BOOL VReg__MountIsAt(
    _In_ const VReg__MountPoint* const pMount,
    const HKEY hRoot,
    _In_ const VReg__Array* const pPath
)
{
    SIZE_T i;
    if (pMount->hRoot != hRoot || pMount->path.count != pPath->count)
        return FALSE;

    for (i = 0; i < pPath->count; ++i)
    {
        const VReg__WStringView left = VReg__WStringViewOf(&VREG__STRING_AT(&pMount->path, i));
        const VReg__WStringView right = VReg__WStringViewOf(&VREG__STRING_AT(pPath, i));
        if (VReg__CompareNames(left, right) != 0)
            return FALSE;
    }

    return TRUE;
}

/// Publishes a table holding the current mounts plus @p pMount, whose strings
/// it then owns. Under the mount lock.
static LSTATUS VReg__PublishMount(_In_ VReg__VirtualRegistry* const pThis, _In_ const VReg__MountPoint* const pMount)
{
    void* const pAllocContext = pThis->pAllocContext;
    VReg__MountTable* const pOld = pThis->pMounts;
    VReg__MountTable* const pNew = (VReg__MountTable*)VReg__AllocZeroed(pAllocContext, sizeof(VReg__MountTable));
    SIZE_T i;
    if (!pNew)
        return ERROR_OUTOFMEMORY;

    for (i = 0; pOld && i < pOld->mounts.count; ++i)
    {
        const VReg__MountPoint* const pExisting = &VREG__MOUNT_AT(&pOld->mounts, i);
        if (VReg__MountIsAt(pExisting, pMount->hRoot, &pMount->path))
        {
            VReg__ArrayFree(pAllocContext, &pNew->mounts);
            VReg__Free(pAllocContext, pNew);
            return ERROR_ALREADY_EXISTS;
        }
        if (!VReg__ArrayAppend(pAllocContext, &pNew->mounts, sizeof(*pExisting), pExisting))
        {
            VReg__ArrayFree(pAllocContext, &pNew->mounts);
            VReg__Free(pAllocContext, pNew);
            return ERROR_OUTOFMEMORY;
        }
    }

    if (!VReg__ArrayAppend(pAllocContext, &pNew->mounts, sizeof(*pMount), pMount))
    {
        VReg__ArrayFree(pAllocContext, &pNew->mounts);
        VReg__Free(pAllocContext, pNew);
        return ERROR_OUTOFMEMORY;
    }

    pNew->pPrevious = pOld;
    WritePointerRelease((PVOID volatile*)&pThis->pMounts, pNew);
    return ERROR_SUCCESS;
}

// --- Resolution ---

static void VReg__TargetFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__Target* const pTarget)
{
    VReg__ArrayFree(pAllocContext, &pTarget->components);
    VReg__ArrayFree(pAllocContext, &pTarget->full);
    VReg__WStringFree(pAllocContext, &pTarget->nativeText);
}

/// Looks for a mount a real key plus the subkey components lands in, by
/// native path; fills @p pTarget and sets @p pIsResolved when it does.
static LSTATUS VReg__ResolveNative(
    _In_ const VReg__VirtualRegistry* const pThis,
    const HKEY hKey,
    const REGSAM samDesired,
    _Inout_ VReg__Target* const pTarget,
    _Out_ BOOL* const pIsResolved
)
{
    void* const pAllocContext = pThis->pAllocContext;
    const VReg__MountTable* const pTable = VReg__MountsOf(pThis);
    BOOL isGated = FALSE;
    VReg__Buffer buffer = {NULL, 0, 0};
    ULONG cbName = 0;
    const VReg__MountPoint* pBest = NULL;
    SIZE_T iBestRest = 0;
    REGSAM samCall;
    SIZE_T i;
    SIZE_T start;
    LSTATUS status = ERROR_SUCCESS;

    *pIsResolved = FALSE;
    for (i = 0; pTable && !isGated && i < pTable->mounts.count; ++i)
    {
        const VReg__MountPoint* const pMount = &VREG__MOUNT_AT(&pTable->mounts, i);
        SIZE_T j;
        if (pMount->path.count == 0)
            continue;

        for (j = 0; !isGated && j < pTarget->components.count; ++j)
        {
            const VReg__WStringView last = VReg__WStringViewOf(&VREG__STRING_AT(&pMount->path, pMount->path.count - 1));
            isGated = VReg__CompareNames(VREG__VIEW_AT(&pTarget->components, j), last) == 0;
        }
    }
    if (!isGated || !pThis->pfnNtQueryKey || VRegIsVirtualHKey(hKey))
        return ERROR_SUCCESS;

    // The name is a KEY_NAME_INFORMATION: a byte count, then the characters.
    if (!VReg__BufferResize(pAllocContext, &buffer, 1024))
        return ERROR_OUTOFMEMORY;

    for (;;)
    {
        ULONG cbResult = 0;
        const LONG ntStatus = pThis->pfnNtQueryKey(
            hKey,
            VREG__KEY_NAME_INFORMATION_CLASS,
            buffer.pb,
            (ULONG)buffer.cb,
            &cbResult
        );
        if (ntStatus >= 0)
            break;
        if (
            (ntStatus != VREG__NT_STATUS_BUFFER_OVERFLOW && ntStatus != VREG__NT_STATUS_BUFFER_TOO_SMALL)
            || cbResult <= buffer.cb
        )
            goto done;
        if (!VReg__BufferResize(pAllocContext, &buffer, cbResult))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    // A resize to a nonzero size that succeeded allocated the storage.
    _Analysis_assume_(buffer.pb != NULL);
    CopyMemory(&cbName, buffer.pb, sizeof(cbName));
    if (cbName > buffer.cb - sizeof(ULONG))
        goto done;
    if (
        !VReg__WStringAssign(
            pAllocContext,
            &pTarget->nativeText,
            (const wchar_t*)(buffer.pb + sizeof(ULONG)),
            cbName / sizeof(wchar_t)
        )
    )
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    for (start = 0; start < pTarget->nativeText.cch;)
    {
        SIZE_T end = start;
        while (end < pTarget->nativeText.cch && pTarget->nativeText.psz[end] != L'\\')
            ++end;
        if (end > start)
        {
            VReg__WStringView component;
            component.pch = pTarget->nativeText.psz + start;
            component.cch = end - start;
            if (!VReg__ArrayAppend(pAllocContext, &pTarget->full, sizeof(component), &component))
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }
        }

        start = end + 1;
    }

    for (i = 0; i < pTarget->components.count; ++i)
    {
        const VReg__WStringView* const pComponent = &VREG__VIEW_AT(&pTarget->components, i);
        if (!VReg__ArrayAppend(pAllocContext, &pTarget->full, sizeof(*pComponent), pComponent))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    samCall = VReg__CallView(samDesired);
    for (i = 0; i < pTable->mounts.count; ++i)
    {
        const VReg__MountPoint* const pMount = &VREG__MOUNT_AT(&pTable->mounts, i);
        BOOL isWow = FALSE;
        SIZE_T iRest = 0;
        if (
            pMount->nativePath.count == 0
            || !VReg__MatchesNative(&pMount->nativePath, &pTarget->full, &isWow, &iRest)
            || !VReg__Admits(pMount->samView, isWow ? KEY_WOW64_32KEY : samCall)
        )
            continue;

        if (!pBest || pMount->nativePath.count > pBest->nativePath.count)
        {
            pBest = pMount;
            iBestRest = iRest;
        }
    }

    if (pBest)
    {
        pTarget->pNode = VReg__HiveRoot(pBest->pHive);
        pTarget->pHive = pBest->pHive;
        pTarget->pRest = (const VReg__WStringView*)pTarget->full.p + iBestRest;
        pTarget->cRest = pTarget->full.count - iBestRest;
        *pIsResolved = TRUE;
    }

done:
    VReg__BufferFree(pAllocContext, &buffer);
    return status;
}

/// Resolves @p hKey and @p subKey (empty for the key itself). @p samDesired
/// supplies the registry view of the call. Answers a status only for a
/// failure of its own (memory); what the call should answer is in the target.
static LSTATUS VReg__Resolve(
    _In_ const VReg__VirtualRegistry* const pThis,
    const HKEY hKey,
    const VReg__WStringView subKey,
    const REGSAM samDesired,
    _Out_ VReg__Target* const pTarget
)
{
    void* const pAllocContext = pThis->pAllocContext;
    LSTATUS splitStatus;
    BOOL isSplit;
    BOOL isResolved = FALSE;
    VReg__Node* pNode;

    ZeroMemory(pTarget, sizeof(*pTarget));
    splitStatus = VReg__SplitPath(pAllocContext, subKey, &pTarget->components);
    if (splitStatus == ERROR_OUTOFMEMORY)
        return splitStatus;

    isSplit = splitStatus == ERROR_SUCCESS;
    pNode = VReg__NodeOf(hKey);
    if (pNode)
    {
        const VReg__MountPoint* const pMount = VReg__MountOf(pThis, VReg__NodeHive(pNode));
        if (pMount)
        {
            if (isSplit)
            {
                pTarget->pNode = pNode;
                pTarget->pHive = pMount->pHive;
                pTarget->pRest = (const VReg__WStringView*)pTarget->components.p;
                pTarget->cRest = pTarget->components.count;
            }
            else
            {
                pTarget->status = splitStatus;
            }
            return ERROR_SUCCESS;
        }
    }
    else if (VReg__HandleSpaceOwns(VReg__ProcessHandleSpace(), hKey))
    {
        pTarget->status = ERROR_INVALID_HANDLE;
        return ERROR_SUCCESS;
    }

    if (VRegIsPredefinedHKey(hKey))
    {
        const REGSAM samCall = VReg__CallView(samDesired);
        const VReg__MountTable* const pTable = VReg__MountsOf(pThis);
        const VReg__MountPoint* pBest = NULL;
        SIZE_T i;
        for (i = 0; pTable && isSplit && i < pTable->mounts.count; ++i)
        {
            const VReg__MountPoint* const pMount = &VREG__MOUNT_AT(&pTable->mounts, i);
            SIZE_T j;
            BOOL isPrefix = TRUE;
            if (
                pMount->hRoot != hKey
                || pMount->path.count > pTarget->components.count
                || !VReg__Admits(pMount->samView, samCall)
            )
                continue;

            for (j = 0; isPrefix && j < pMount->path.count; ++j)
            {
                const VReg__WStringView mountName = VReg__WStringViewOf(&VREG__STRING_AT(&pMount->path, j));
                isPrefix = VReg__CompareNames(mountName, VREG__VIEW_AT(&pTarget->components, j)) == 0;
            }
            if (!isPrefix)
                continue;

            if (!pBest || pMount->path.count > pBest->path.count)
                pBest = pMount;
        }

        if (pBest)
        {
            pTarget->pNode = VReg__HiveRoot(pBest->pHive);
            pTarget->pHive = pBest->pHive;
            pTarget->pRest = (const VReg__WStringView*)pTarget->components.p + pBest->path.count;
            pTarget->cRest = pTarget->components.count - pBest->path.count;
            return ERROR_SUCCESS;
        }

        if (pThis->pBase)
            pTarget->isBase = TRUE;
        else
            pTarget->status = isSplit ? ERROR_FILE_NOT_FOUND : splitStatus;

        return ERROR_SUCCESS;
    }

    if (!pThis->pBase)
    {
        pTarget->status = ERROR_INVALID_HANDLE;
        return ERROR_SUCCESS;
    }

    if (isSplit)
    {
        const LSTATUS status = VReg__ResolveNative(pThis, hKey, samDesired, pTarget, &isResolved);
        if (status != ERROR_SUCCESS)
            return status;
    }

    if (!isResolved)
        pTarget->isBase = TRUE;

    return ERROR_SUCCESS;
}

// --- The slots ---

static LSTATUS WINAPI VReg__VirtualRegOpenKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_ const DWORD ulOptions,
    _In_ const REGSAM samDesired,
    _Out_ const PHKEY phkResult
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__OpenResult result;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegOpenKeyExW(
            pRegistry->pBase,
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );
        goto done;
    }

    if (!phkResult)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    *phkResult = NULL;
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }
    if (!samDesired)
    {
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    status = VReg__HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, samDesired, &result);
    if (status == ERROR_SUCCESS)
        *phkResult = result.hLower ? result.hLower : VReg__NodeHandle(result.pNode);

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegCreateKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _Reserved_ const DWORD Reserved,
    _In_opt_ const LPWSTR lpClass,
    _In_ const DWORD dwOptions,
    _In_ const REGSAM samDesired,
    _In_opt_ const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ const PHKEY phkResult,
    _Out_opt_ const LPDWORD lpdwDisposition
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__OpenResult result;
    DWORD dwDisposition = REG_OPENED_EXISTING_KEY;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegCreateKeyExW(
            pRegistry->pBase,
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
        goto done;
    }

    if (!phkResult)
    {
        status = ERROR_BADKEY;
        goto done;
    }

    // Native leaves the result alone for these two and nulls it for every other
    // failure.
    if (Reserved)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (target.status == ERROR_INVALID_HANDLE)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    *phkResult = NULL;
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }
    if (dwOptions & ~(DWORD)0x1F)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    status = VReg__HiveCreateKey(
        target.pHive,
        target.pNode,
        target.pRest,
        target.cRest,
        VReg__OrEmpty(lpClass),
        dwOptions,
        samDesired,
        &result,
        &dwDisposition
    );
    if (status != ERROR_SUCCESS)
    {
        status = VReg__FailureStatus(status);
        goto done;
    }

    *phkResult = result.hLower ? result.hLower : VReg__NodeHandle(result.pNode);
    if (lpdwDisposition)
        *lpdwDisposition = dwDisposition;

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegCloseKey(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    if (VReg__NodeOf(hKey) || VRegIsPredefinedHKey(hKey))
        return ERROR_SUCCESS;
    if (VReg__HandleSpaceOwns(VReg__ProcessHandleSpace(), hKey) || !pRegistry->pBase)
        return ERROR_INVALID_HANDLE;

    return VRegCloseKey(
        pRegistry->pBase,
        hKey
    );
}

static LSTATUS WINAPI VReg__VirtualRegQueryValueExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) const LPDWORD lpcbData
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__Buffer value = {NULL, 0, 0};
    DWORD dwType = 0;
    DWORD cbValue = 0;
    DWORD cbBuffer;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegQueryValueExW(
            pRegistry->pBase,
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
        goto done;
    }

    if ((lpData && !lpcbData) || lpReserved)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    status = VReg__HiveQueryValue(
        target.pHive,
        target.pNode,
        VReg__OrEmpty(lpValueName),
        &dwType,
        &cbValue,
        lpData ? &value : NULL
    );
    if (status != ERROR_SUCCESS)
        goto done;

    if (lpType)
        *lpType = dwType;
    if (!lpData)
    {
        if (lpcbData)
            *lpcbData = cbValue;
        goto done;
    }

    cbBuffer = *lpcbData;
    *lpcbData = cbValue;
    if (cbValue > cbBuffer)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    VReg__CopyValue(lpData, cbBuffer, dwType, value.pb, (DWORD)value.cb);

done:
    if (target.pHive)
        VReg__BufferFree(VReg__HiveAllocContext(target.pHive), &value);
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegSetValueExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName,
    _Reserved_ const DWORD Reserved,
    _In_ const DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* const lpData,
    _In_ const DWORD cbData
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegSetValueExW(
            pRegistry->pBase,
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );
        goto done;
    }

    if (Reserved)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }
    if (!lpData && cbData)
    {
        status = ERROR_NOACCESS;
        goto done;
    }

    status = VReg__HiveSetValue(
        target.pHive,
        target.pNode,
        VReg__OrEmpty(lpValueName),
        dwType,
        lpData,
        lpData ? cbData : 0
    );

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegDeleteValueW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpValueName
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegDeleteValueW(
            pRegistry->pBase,
            hKey,
            lpValueName
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    status = VReg__HiveDeleteValue(target.pHive, target.pNode, VReg__OrEmpty(lpValueName));

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegDeleteKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const LPCWSTR lpSubKey,
    _In_ const REGSAM samDesired,
    _Reserved_ const DWORD Reserved
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(lpSubKey), samDesired, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegDeleteKeyExW(
            pRegistry->pBase,
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );
        goto done;
    }

    if (Reserved)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    status = VReg__HiveDeleteKey(target.pHive, target.pNode, target.pRest, target.cRest);

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegDeleteTreeW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR lpSubKey
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__OpenResult result;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(lpSubKey), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegDeleteTreeW(
            pRegistry->pBase,
            hKey,
            lpSubKey
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    status = VReg__HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, KEY_ALL_ACCESS, &result);
    if (status != ERROR_SUCCESS)
        goto done;

    // Only a frozen hive hands out lower keys, and it refuses every write.
    if (result.hLower)
    {
        VRegRegistry* const pLower = VReg__HiveLower(target.pHive);
        // A lower handle exists only where there is a lower registry.
        _Analysis_assume_(pLower);
        VRegCloseKey(
            pLower,
            result.hLower
        );
        status = ERROR_ACCESS_DENIED;
        goto done;
    }

    status = VReg__HiveDeleteTree(target.pHive, result.pNode);
    if (status != ERROR_SUCCESS || target.cRest == 0)
        goto done;

    status = VReg__HiveDeleteKey(target.pHive, target.pNode, target.pRest, target.cRest);

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegEnumKeyExW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) const LPWSTR lpName,
    _Inout_ const LPDWORD lpcchName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPWSTR lpClass,
    _Inout_opt_ const LPDWORD lpcchClass,
    _Out_opt_ const PFILETIME lpftLastWriteTime
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__WString name = {NULL, 0, 0};
    VReg__WString className = {NULL, 0, 0};
    FILETIME lastWrite;
    DWORD cchClassBuffer;
    BOOL isNameFit;
    BOOL isClassFit;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    ZeroMemory(&lastWrite, sizeof(lastWrite));
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegEnumKeyExW(
            pRegistry->pBase,
            hKey,
            dwIndex,
            lpName,
            lpcchName,
            lpReserved,
            lpClass,
            lpcchClass,
            lpftLastWriteTime
        );
        goto done;
    }

    if (lpReserved || !lpcchName || (lpClass && !lpcchClass))
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    cchClassBuffer = lpClass ? *lpcchClass : 0;
    if (lpcchClass)
        *lpcchClass = 0;

    status = VReg__HiveEnumKey(target.pHive, target.pNode, dwIndex, &name, &className, &lastWrite);
    if (status != ERROR_SUCCESS)
        goto done;

    if (lpftLastWriteTime)
        *lpftLastWriteTime = lastWrite;

    isNameFit = lpName && name.cch < *lpcchName;
    isClassFit = !lpClass || className.cch < cchClassBuffer;
    if (!isNameFit || !isClassFit)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    CopyMemory(lpName, VReg__WStringText(&name), (name.cch + 1) * sizeof(wchar_t));
    *lpcchName = (DWORD)name.cch;
    if (lpClass)
    {
        CopyMemory(lpClass, VReg__WStringText(&className), (className.cch + 1) * sizeof(wchar_t));
        *lpcchClass = (DWORD)className.cch;
    }

done:
    if (target.pHive)
    {
        VReg__WStringFree(VReg__HiveAllocContext(target.pHive), &name);
        VReg__WStringFree(VReg__HiveAllocContext(target.pHive), &className);
    }
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegEnumValueW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const DWORD dwIndex,
    _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) const LPWSTR lpValueName,
    _Inout_ const LPDWORD lpcchValueName,
    _Reserved_ const LPDWORD lpReserved,
    _Out_opt_ const LPDWORD lpType,
    _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) const LPBYTE lpData,
    _Inout_opt_ const LPDWORD lpcbData
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__WString name = {NULL, 0, 0};
    VReg__Buffer value = {NULL, 0, 0};
    DWORD dwType = 0;
    DWORD cbValue = 0;
    BOOL isDataFit = TRUE;
    DWORD cbDataBuffer = 0;
    BOOL isNameFit;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegEnumValueW(
            pRegistry->pBase,
            hKey,
            dwIndex,
            lpValueName,
            lpcchValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );
        goto done;
    }

    if (lpReserved || !lpValueName || !lpcchValueName || (lpData && !lpcbData))
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    status = VReg__HiveEnumValue(
        target.pHive,
        target.pNode,
        dwIndex,
        &name,
        &dwType,
        &cbValue,
        lpData ? &value : NULL
    );
    if (status != ERROR_SUCCESS)
        goto done;

    if (lpType)
        *lpType = dwType;

    if (lpcbData)
    {
        cbDataBuffer = *lpcbData;
        isDataFit = !lpData || cbValue <= cbDataBuffer;
        *lpcbData = cbValue;
    }

    isNameFit = name.cch < *lpcchValueName;
    if (isNameFit)
    {
        CopyMemory(lpValueName, VReg__WStringText(&name), (name.cch + 1) * sizeof(wchar_t));
        *lpcchValueName = (DWORD)name.cch;
    }

    if (!isNameFit || !isDataFit)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    if (lpData)
        VReg__CopyValue(lpData, cbDataBuffer, dwType, value.pb, (DWORD)value.cb);

done:
    if (target.pHive)
    {
        VReg__WStringFree(VReg__HiveAllocContext(target.pHive), &name);
        VReg__BufferFree(VReg__HiveAllocContext(target.pHive), &value);
    }
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

// Native answers success without writing lpClass when *lpcchClass is zero,
// which the annotation's terminator guarantee does not allow for.
#pragma warning(suppress: 6054)
static LSTATUS WINAPI VReg__VirtualRegQueryInfoKeyW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) const LPWSTR lpClass,
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
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    VReg__KeyInfo info;
    DWORD cchClass;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    ZeroMemory(&info, sizeof(info));
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegQueryInfoKeyW(
            pRegistry->pBase,
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
        goto done;
    }

    if (lpReserved || (lpClass && !lpcchClass))
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    status = VReg__HiveQueryInfo(target.pHive, target.pNode, &info);
    if (status != ERROR_SUCCESS)
        goto done;

    if (lpcSubKeys)
        *lpcSubKeys = info.cSubKeys;
    if (lpcbMaxSubKeyLen)
        *lpcbMaxSubKeyLen = info.cchMaxSubKey;
    if (lpcbMaxClassLen)
        *lpcbMaxClassLen = info.cchMaxClass;
    if (lpcValues)
        *lpcValues = info.cValues;
    if (lpcbMaxValueNameLen)
        *lpcbMaxValueNameLen = info.cchMaxValueName;
    if (lpcbMaxValueLen)
        *lpcbMaxValueLen = info.cbMaxValue;
    if (lpcbSecurityDescriptor)
        *lpcbSecurityDescriptor = 0;
    if (lpftLastWriteTime)
        *lpftLastWriteTime = info.lastWrite;

    cchClass = (DWORD)info.className.cch;
    if (lpClass && *lpcchClass != 0)
    {
        if (*lpcchClass < cchClass)
        {
            *lpcchClass = cchClass;
            status = ERROR_MORE_DATA;
            goto done;
        }
        if (*lpcchClass == cchClass)
        {
            *lpcchClass = cchClass - 1;
            status = ERROR_INSUFFICIENT_BUFFER;
            goto done;
        }

        CopyMemory(lpClass, VReg__WStringText(&info.className), (cchClass + 1) * sizeof(wchar_t));
    }

    if (lpClass && lpcchClass && (*lpcchClass != 0 || lpcbMaxClassLen))
        *lpcchClass = cchClass;
    else if (!lpClass && lpcchClass)
        *lpcchClass = 0;

done:
    if (target.pHive)
        VReg__WStringFree(VReg__HiveAllocContext(target.pHive), &info.className);
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegLoadMUIStringW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_opt_ const LPCWSTR pszValue,
    _Out_writes_bytes_opt_(cbOutBuf) const LPWSTR pszOutBuf,
    _In_ const DWORD cbOutBuf,
    _Out_opt_ const LPDWORD pcbData,
    _In_ const DWORD Flags,
    _In_opt_ const LPCWSTR pszDirectory
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    HKEY hLower = NULL;
    VReg__WString text = {NULL, 0, 0};
    DWORD cbText;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegLoadMUIStringW(
            pRegistry->pBase,
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );
        goto done;
    }

    if (!hKey)
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }
    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }
    if (
        (Flags & ~(DWORD)REG_MUI_STRING_TRUNCATE)
        || ((Flags & REG_MUI_STRING_TRUNCATE) != 0) != (pcbData == NULL)
        || cbOutBuf % sizeof(wchar_t) != 0
        || (!pszOutBuf && cbOutBuf != 0)
    )
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    status = VReg__HiveOpenLowerValueKey(target.pHive, target.pNode, VReg__OrEmpty(pszValue), &hLower);
    if (status != ERROR_SUCCESS)
        goto done;

    // A value from below is resolved there, as the system would. One the hive
    // holds would need a resource module to resolve, unless it is marked as
    // the text itself.
    if (hLower)
    {
        VRegRegistry* const pLower = VReg__HiveLower(target.pHive);
        // A lower handle exists only where there is a lower registry.
        _Analysis_assume_(pLower);
        status = VRegLoadMUIStringW(
            pLower,
            hLower,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );
        VRegCloseKey(
            pLower,
            hLower
        );
        goto done;
    }

    status = VReg__HiveQueryMuiText(target.pHive, target.pNode, VReg__OrEmpty(pszValue), &text);
    if (status != ERROR_SUCCESS)
        goto done;

    // Truncation fills whatever buffer there is, terminated; otherwise the
    // size is reported and the text copied only when all of it fits.
    cbText = (DWORD)((text.cch + 1) * sizeof(wchar_t));
    if (!pcbData)
    {
        SIZE_T cch;
        if (!pszOutBuf)
        {
            status = ERROR_MORE_DATA;
            goto done;
        }
        if (cbOutBuf == 0)
            goto done;

        cch = cbOutBuf / sizeof(wchar_t) - 1;
        if (cch > text.cch)
            cch = text.cch;
        CopyMemory(pszOutBuf, VReg__WStringText(&text), cch * sizeof(wchar_t));
        pszOutBuf[cch] = L'\0';
        goto done;
    }

    *pcbData = cbText;
    if (!pszOutBuf || cbOutBuf < cbText)
    {
        status = ERROR_MORE_DATA;
        goto done;
    }

    CopyMemory(pszOutBuf, VReg__WStringText(&text), cbText);

done:
    if (target.pHive)
        VReg__WStringFree(VReg__HiveAllocContext(target.pHive), &text);
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

/// Answers the failure @p status after clearing the caller's buffer when the
/// call asked for that on failure.
_Post_satisfies_(return != ERROR_SUCCESS)
static LSTATUS VReg__FailGetValue(
    _Out_writes_bytes_opt_(cbBuffer) const PVOID pvData,
    const DWORD cbBuffer,
    const DWORD dwFlags,
    const LSTATUS status
)
{
    _Analysis_assume_(status != ERROR_SUCCESS);
    if (pvData && (dwFlags & RRF_ZEROONFAILURE))
        ZeroMemory(pvData, cbBuffer);
    return status;
}

/// Whether the character at @p iwch of @p pBuffer is a terminator.
static BOOL VReg__IsNulAt(_In_ const VReg__Buffer* const pBuffer, const SIZE_T iwch)
{
    wchar_t ch;
    CopyMemory(&ch, pBuffer->pb + iwch * sizeof(wchar_t), sizeof(ch));
    return ch == L'\0';
}

// The SDK's _Post_z_ holds on every return, truncated reads and failures
// included; the analyzer cannot see the terminators written at run time.
#pragma warning(suppress: 6054)
static LSTATUS WINAPI VReg__VirtualRegGetValueW(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hkey,
    _In_opt_ const LPCWSTR lpSubKey,
    _In_opt_ const LPCWSTR lpValue,
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
    static const DWORD TYPE_FLAGS[] = {
        RRF_RT_REG_NONE,
        RRF_RT_REG_SZ,
        RRF_RT_REG_EXPAND_SZ,
        RRF_RT_REG_BINARY,
        RRF_RT_REG_DWORD,
        0,
        0,
        RRF_RT_REG_MULTI_SZ,
        0,
        0,
        0,
        RRF_RT_REG_QWORD,
    };

    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    void* pHiveContext = NULL;
    VReg__Target target;
    VReg__OpenResult result;
    VReg__Node* pNode;
    DWORD cbBuffer;
    DWORD dwType = 0;
    DWORD cbQueried = 0;
    BOOL isString;

    // Native copies the stored bytes first, then the string with the
    // terminators it lacks, then the expansion, each over the previous one and
    // only if it fits.
    VReg__Buffer stages[3] = {{NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}};
    DWORD cbReported;
    DWORD dwTypeFlag;
    BOOL isAllowed;
    const VReg__Buffer* pFinal;
    BOOL isFit;
    BOOL isSizeMismatch;
    SIZE_T i;
    LSTATUS status = VReg__Resolve(
        pRegistry,
        hkey,
        VReg__ViewOf(lpSubKey),
        (dwFlags & VREG__RRF_WOW6464_SUBKEY ? KEY_WOW64_64KEY : 0)
        | (dwFlags & VREG__RRF_WOW6432_SUBKEY ? KEY_WOW64_32KEY : 0),
        &target
    );
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegGetValueW(
            pRegistry->pBase,
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );
        goto done;
    }

    if (
        !hkey
        || (pvData && !pcbData)
        || !(dwFlags & RRF_RT_ANY)
        || (dwFlags & VREG__RRF_WOW_SUBKEYS) == VREG__RRF_WOW_SUBKEYS
    )
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    cbBuffer = pcbData ? *pcbData : 0;
    if (!target.pNode)
    {
        status = VReg__FailGetValue(pvData, cbBuffer, dwFlags, target.status);
        goto done;
    }

    pHiveContext = VReg__HiveAllocContext(target.pHive);
    pNode = target.pNode;
    if (target.cRest != 0)
    {
        status = VReg__HiveOpenKey(target.pHive, target.pNode, target.pRest, target.cRest, KEY_QUERY_VALUE, &result);
        if (status != ERROR_SUCCESS)
        {
            status = VReg__FailGetValue(pvData, cbBuffer, dwFlags, status);
            goto done;
        }

        if (result.hLower)
        {
            VRegRegistry* const pLower = VReg__HiveLower(target.pHive);
            // A lower handle exists only where there is a lower registry.
            _Analysis_assume_(pLower);
            status = VRegGetValueW(
                pLower,
                result.hLower,
                NULL,
                lpValue,
                dwFlags,
                pdwType,
                pvData,
                pcbData
            );
            VRegCloseKey(
                pLower,
                result.hLower
            );
            goto done;
        }

        pNode = result.pNode;
    }

    if (pdwType)
        *pdwType = 0;
    if (pcbData && !pvData)
        *pcbData = 0;

    status = VReg__HiveQueryValue(target.pHive, pNode, VReg__OrEmpty(lpValue), &dwType, &cbQueried, &stages[0]);
    if (status != ERROR_SUCCESS)
    {
        status = VReg__FailGetValue(pvData, cbBuffer, dwFlags, status);
        goto done;
    }

    isString = VReg__IsStringType(dwType);
    cbReported = (DWORD)stages[0].cb;
    if (isString)
    {
        const SIZE_T cwch = stages[0].cb / sizeof(wchar_t);
        SIZE_T cwchAdded = 0;
        static const wchar_t ZEROS[2] = {L'\0', L'\0'};
        if (cwch == 0 || !VReg__IsNulAt(&stages[0], cwch - 1))
            cwchAdded = dwType == REG_MULTI_SZ ? 2 : 1;
        else if (dwType == REG_MULTI_SZ && (cwch == 1 || !VReg__IsNulAt(&stages[0], cwch - 2)))
            cwchAdded = 1;

        if (
            !VReg__BufferAssign(pHiveContext, &stages[1], stages[0].pb, cwch * sizeof(wchar_t))
            || !VReg__BufferAppend(pHiveContext, &stages[1], ZEROS, cwchAdded * sizeof(wchar_t))
        )
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        cbReported = (DWORD)stages[1].cb;
        if (dwType == REG_EXPAND_SZ && !(dwFlags & RRF_NOEXPAND))
        {
            const wchar_t* const pszRaw = (const wchar_t*)stages[1].pb;
            DWORD cwchExpanded;

            // The text was terminated above, so the stage holds at least that.
            _Analysis_assume_(pszRaw != NULL);
            cwchExpanded = ExpandEnvironmentStringsW(pszRaw, NULL, 0);
            if (!VReg__BufferResize(pHiveContext, &stages[2], (SIZE_T)cwchExpanded * sizeof(wchar_t)))
            {
                status = ERROR_OUTOFMEMORY;
                goto done;
            }
            if (cwchExpanded != 0)
                ExpandEnvironmentStringsW(pszRaw, (wchar_t*)stages[2].pb, cwchExpanded);

            cbReported = VReg__MaxDword(cbReported, (DWORD)stages[2].cb);
            dwType = REG_SZ;
        }
    }

    dwTypeFlag = dwType < sizeof(TYPE_FLAGS) / sizeof(TYPE_FLAGS[0]) ? TYPE_FLAGS[dwType] : 0;
    isAllowed = dwTypeFlag ? (dwFlags & dwTypeFlag) != 0 : (dwFlags & RRF_RT_ANY) == RRF_RT_ANY;

    pFinal = stages[2].cb != 0 ? &stages[2] : (stages[1].cb != 0 ? &stages[1] : &stages[0]);
    isFit = pvData && pFinal->cb <= cbBuffer;
    if (pvData)
    {
        for (i = 0; i < 3; ++i)
        {
            if (stages[i].cb != 0 && stages[i].cb <= cbBuffer)
                CopyMemory(pvData, stages[i].pb, stages[i].cb);
        }

        // An expansion that does not fit leaves the unexpanded text with its
        // first character cleared.
        if (stages[2].cb != 0 && !isFit && stages[1].cb <= cbBuffer)
            ZeroMemory(pvData, sizeof(wchar_t));
    }

    if (pdwType)
        *pdwType = dwType;
    if (pcbData)
    {
        // A string that cannot be returned reports room for the terminators it
        // may lack.
        const DWORD cbTerminators = dwType == REG_MULTI_SZ ? 4 : (isString ? 2 : 0);
        const DWORD cbReserved = (DWORD)stages[0].cb + cbTerminators;
        if (stages[2].cb != 0)
            *pcbData = cbReported;
        else
            *pcbData = isFit ? cbReported : VReg__MaxDword(cbReported, cbReserved);
    }

    // Binary data passes as a DWORD only when it is empty or has that size.
    // Agent's choice: native was measured with sizes 0 and 5 only; rejecting
    // every other size is a guess for 1 to 3 and 6 or more.
    isSizeMismatch =
        dwType == REG_BINARY
        && (dwFlags & RRF_RT_ANY) == RRF_RT_DWORD
        && stages[0].cb != 0
        && stages[0].cb != sizeof(DWORD);

    if (!isAllowed)
        status = ERROR_UNSUPPORTED_TYPE;
    else if (isSizeMismatch)
        status = ERROR_DATATYPE_MISMATCH;
    else if (pvData && !isFit)
        status = ERROR_MORE_DATA;
    if (status != ERROR_SUCCESS)
        status = VReg__FailGetValue(pvData, cbBuffer, dwFlags, status);

done:
    for (i = 0; i < 3; ++i)
        VReg__BufferFree(pHiveContext, &stages[i]);
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegNotifyChangeKeyValue(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey,
    _In_ const BOOL bWatchSubtree,
    _In_ const DWORD dwNotifyFilter,
    _In_opt_ const HANDLE hEvent,
    _In_ const BOOL fAsynchronous
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    HANDLE hOwn;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegNotifyChangeKeyValue(
            pRegistry->pBase,
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }
    if (
        (dwNotifyFilter & ~(DWORD)VREG__KNOWN_NOTIFY_FILTERS)
        || (fAsynchronous && !hEvent)
    )
    {
        status = ERROR_INVALID_PARAMETER;
        goto done;
    }

    if (fAsynchronous)
    {
        status = VReg__HiveWatch(target.pHive, target.pNode, bWatchSubtree != FALSE, dwNotifyFilter, hEvent);
        goto done;
    }

    // A synchronous call waits for the change like the native one, on a private
    // event, so a watcher thread written for the native registry behaves the
    // same (and, as there, never returns on a key nothing changes).
    hOwn = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hOwn)
    {
        status = GetLastError();
        goto done;
    }

    status = VReg__HiveWatch(target.pHive, target.pNode, bWatchSubtree != FALSE, dwNotifyFilter, hOwn);
    if (status == ERROR_SUCCESS)
        WaitForSingleObject(hOwn, INFINITE);

    CloseHandle(hOwn);

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

static LSTATUS WINAPI VReg__VirtualRegFlushKey(
    _In_ VRegRegistry* const pThis,
    _In_ const HKEY hKey
)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    VReg__Target target;
    LSTATUS status = VReg__Resolve(pRegistry, hKey, VReg__ViewOf(NULL), 0, &target);
    if (status != ERROR_SUCCESS)
        goto done;
    if (target.isBase)
    {
        status = VRegFlushKey(
            pRegistry->pBase,
            hKey
        );
        goto done;
    }

    if (!target.pNode)
    {
        status = VReg__FailureStatus(target.status);
        goto done;
    }

    status = VReg__HiveCheckKey(target.pHive, target.pNode);

done:
    VReg__TargetFree(pRegistry->pAllocContext, &target);
    return status;
}

// --- Creation and destruction ---

static void WINAPI VReg__VirtualDestroy(_In_ VRegRegistry* const pThis)
{
    VReg__VirtualRegistry* const pRegistry = VReg__VirtualOf(pThis);
    void* const pAllocContext = pRegistry->pAllocContext;
    VReg__MountTable* pTable = pRegistry->pMounts;
    BOOL isNewest = TRUE;
    while (pTable)
    {
        VReg__MountTable* const pPrevious = pTable->pPrevious;
        if (isNewest)
        {
            SIZE_T i;
            for (i = 0; i < pTable->mounts.count; ++i)
                VReg__MountPointFree(pAllocContext, &VREG__MOUNT_AT(&pTable->mounts, i));
        }

        VReg__ArrayFree(pAllocContext, &pTable->mounts);
        VReg__Free(pAllocContext, pTable);
        pTable = pPrevious;
        isNewest = FALSE;
    }

    VReg__Free(pAllocContext, pRegistry);
}

static const VRegRegistryVtbl VReg__VirtualRegistryVtbl = {
    .Destroy = &VReg__VirtualDestroy,
    .RegOpenKeyExW = &VReg__VirtualRegOpenKeyExW,
    .RegCreateKeyExW = &VReg__VirtualRegCreateKeyExW,
    .RegCloseKey = &VReg__VirtualRegCloseKey,
    .RegQueryValueExW = &VReg__VirtualRegQueryValueExW,
    .RegSetValueExW = &VReg__VirtualRegSetValueExW,
    .RegDeleteValueW = &VReg__VirtualRegDeleteValueW,
    .RegDeleteKeyExW = &VReg__VirtualRegDeleteKeyExW,
    .RegDeleteTreeW = &VReg__VirtualRegDeleteTreeW,
    .RegEnumKeyExW = &VReg__VirtualRegEnumKeyExW,
    .RegEnumValueW = &VReg__VirtualRegEnumValueW,
    .RegQueryInfoKeyW = &VReg__VirtualRegQueryInfoKeyW,
    .RegLoadMUIStringW = &VReg__VirtualRegLoadMUIStringW,
    .RegGetValueW = &VReg__VirtualRegGetValueW,
    .RegNotifyChangeKeyValue = &VReg__VirtualRegNotifyChangeKeyValue,
    .RegFlushKey = &VReg__VirtualRegFlushKey,
};

VREGDEF LSTATUS WINAPI VRegCreateVirtualRegistry(
    _In_opt_ VRegRegistry* const pBase,
    _In_opt_ const VRegNtQueryKeyFunction pfnNtQueryKey,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegRegistry** const ppRegistry
)
{
    VReg__VirtualRegistry* const pRegistry =
        (VReg__VirtualRegistry*)VReg__AllocZeroed(pAllocContext, sizeof(VReg__VirtualRegistry));
    *ppRegistry = NULL;
    if (!pRegistry)
        return ERROR_OUTOFMEMORY;

    pRegistry->object.lpVtbl = &VReg__VirtualRegistryVtbl;
    pRegistry->object.pAllocContext = pAllocContext;
    pRegistry->pAllocContext = pAllocContext;
    pRegistry->pBase = pBase;
    pRegistry->pfnNtQueryKey = pfnNtQueryKey;
    InitializeSRWLock(&pRegistry->mountLock);
    if (!pRegistry->pfnNtQueryKey)
        *(void**)&pRegistry->pfnNtQueryKey = VReg__FindExport(GetModuleHandleW(L"ntdll.dll"), "NtQueryKey");

    *ppRegistry = &pRegistry->object;
    return ERROR_SUCCESS;
}

VREGDEF LSTATUS WINAPI VRegMount(
    _In_ VRegRegistry* const pRegistry,
    _In_ const HKEY hRoot,
    _In_opt_z_ const LPCWSTR pszPath,
    _In_ VRegHive* const pHive,
    _In_ const REGSAM samView
)
{
    VReg__VirtualRegistry* pThis;
    void* pAllocContext;
    VReg__Array components = {NULL, 0, 0};
    VReg__MountPoint mount;
    SIZE_T i;
    LSTATUS status;

    if (pRegistry->lpVtbl != &VReg__VirtualRegistryVtbl || !VRegIsPredefinedHKey(hRoot))
        return ERROR_INVALID_HANDLE;
    if (samView != 0 && samView != KEY_WOW64_64KEY && samView != KEY_WOW64_32KEY)
        return ERROR_INVALID_PARAMETER;

    pThis = VReg__VirtualOf(pRegistry);
    pAllocContext = pThis->pAllocContext;
    ZeroMemory(&mount, sizeof(mount));
    mount.hRoot = hRoot;
    mount.pHive = pHive;
    mount.samView = samView;

    status = VReg__SplitPath(pAllocContext, VReg__ViewOf(pszPath), &components);
    if (status != ERROR_SUCCESS)
    {
        VReg__ArrayFree(pAllocContext, &components);
        return status == ERROR_OUTOFMEMORY ? status : ERROR_BAD_PATHNAME;
    }

    for (i = 0; status == ERROR_SUCCESS && i < components.count; ++i)
    {
        if (!VReg__WStringArrayAppend(pAllocContext, &mount.path, VREG__VIEW_AT(&components, i)))
            status = ERROR_OUTOFMEMORY;
    }
    if (status == ERROR_SUCCESS)
        status = VReg__NativeRootPath(pAllocContext, hRoot, &mount.nativePath);
    for (i = 0; status == ERROR_SUCCESS && mount.nativePath.count != 0 && i < components.count; ++i)
    {
        const VReg__WStringView component = VREG__VIEW_AT(&components, i);
        if (!VReg__IsWow6432Node(component) && !VReg__WStringArrayAppend(pAllocContext, &mount.nativePath, component))
            status = ERROR_OUTOFMEMORY;
    }
    VReg__ArrayFree(pAllocContext, &components);

    if (status == ERROR_SUCCESS)
    {
        AcquireSRWLockExclusive(&pThis->mountLock);
        status = VReg__PublishMount(pThis, &mount);
        ReleaseSRWLockExclusive(&pThis->mountLock);
    }

    if (status != ERROR_SUCCESS)
        VReg__MountPointFree(pAllocContext, &mount);
    return status;
}
