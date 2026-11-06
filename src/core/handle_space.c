#include "core/handle_space.h"

typedef struct VReg__HandleBlock
{
    void* volatile targets[VREG__HANDLE_BLOCK_SIZE];
} VReg__HandleBlock;

struct VReg__HandleDirectory
{
    VReg__HandleBlock* volatile blocks[VREG__HANDLE_DIRECTORY_SIZE];
};

static VReg__HandleSpace g_processSpace;
static INIT_ONCE g_processSpaceOnce = INIT_ONCE_STATIC_INIT;

// Agent's choice: the block tables come from the process heap directly, not
// from the allocation macros. They live for the process and belong to no hive
// or registry, so no caller's allocation context could own them.
static void* VReg__AllocateZeroed(const SIZE_T cb)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
}

void VReg__HandleSpaceInit(_Out_ VReg__HandleSpace* const pSpace, _In_z_ const LPCWSTR pszName)
{
    HANDLE hSection;
    VReg__HandleSpaceShared* pShared;
    LONG previous;

    ZeroMemory(pSpace, sizeof(*pSpace));
    InitializeSRWLock(&pSpace->lock);

    // Pagefile-backed: the pages start zero-filled, which is the unclaimed
    // state. Open-or-create is atomic, so racing copies get the same section.
    hSection = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(VReg__HandleSpaceShared),
        pszName
    );
    if (!hSection)
        return;

    pShared = (VReg__HandleSpaceShared*)MapViewOfFile(
        hSection,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(VReg__HandleSpaceShared)
    );
    if (!pShared)
    {
        CloseHandle(hSection);
        return;
    }

    // The handle stays open on purpose, and so does the view: other copies'
    // handles were reserved through it, and a section that vanished and came
    // back zeroed would hand them out again.

    // Whoever moves magic from zero has claimed it; the layout version is part
    // of the magic value, so there is nothing else to publish.
    previous = InterlockedCompareExchange(&pShared->magic, VREG__HANDLE_SPACE_MAGIC, 0);
    if (previous != 0 && previous != VREG__HANDLE_SPACE_MAGIC)
        return;

    pSpace->pShared = pShared;
}

/// Writes @p value in decimal at @p pszOut, which has room for @p cchOut
/// characters including the terminator, and answers the characters written.
static SIZE_T VReg__WriteDecimal(_Out_writes_z_(cchOut) wchar_t* const pszOut, const SIZE_T cchOut, DWORD value)
{
    wchar_t wszDigits[10];
    SIZE_T cchDigits = 0;
    SIZE_T i;

    do
    {
        wszDigits[cchDigits++] = (wchar_t)(L'0' + value % 10);
        value /= 10;
    }
    while (value != 0 && cchDigits < 10);

    if (cchDigits >= cchOut)
        cchDigits = cchOut - 1;

    for (i = 0; i < cchDigits; ++i)
        pszOut[i] = wszDigits[cchDigits - 1 - i];

    pszOut[cchDigits] = L'\0';
    return cchDigits;
}

static BOOL CALLBACK VReg__InitProcessHandleSpace(
    _Inout_ PINIT_ONCE const pInitOnce,
    _Inout_opt_ const PVOID pParameter,
    _Outptr_opt_result_maybenull_ PVOID* const ppContext
)
{
    // Every copy of VReg in the process, however built, finds the others by
    // this name, so it never changes: a copy using another name would hand
    // out handles that overlap theirs.
    static const wchar_t wszPrefix[] = L"Local\\VReg-handles-";
    wchar_t wszName[64];
    SIZE_T cchPrefix = sizeof(wszPrefix) / sizeof(wszPrefix[0]) - 1;

    (void)pInitOnce;
    (void)pParameter;
    if (ppContext)
        *ppContext = NULL;

    CopyMemory(wszName, wszPrefix, cchPrefix * sizeof(wchar_t));
    VReg__WriteDecimal(wszName + cchPrefix, sizeof(wszName) / sizeof(wszName[0]) - cchPrefix, GetCurrentProcessId());
    VReg__HandleSpaceInit(&g_processSpace, wszName);
    return TRUE;
}

_Ret_notnull_ VReg__HandleSpace* VReg__ProcessHandleSpace(void)
{
    // Handles issued from it outlive every teardown, so the instance is never
    // destroyed.
    InitOnceExecuteOnce(&g_processSpaceOnce, VReg__InitProcessHandleSpace, NULL, NULL);
    return &g_processSpace;
}

BOOL VReg__HandleSpaceIsUsable(_In_ const VReg__HandleSpace* const pSpace)
{
    return pSpace->pShared != NULL;
}

// A slot holds null while unbound, which WritePointerRelease's annotations
// reject; the exchange accepts it, and its full barrier covers the release.
static void VReg__StoreTarget(_Inout_ void* volatile* const pSlot, _In_opt_ void* const pTarget)
{
    InterlockedExchangePointer(pSlot, pTarget);
}

static VReg__HandleBlock* VReg__FindBlock(_In_ const VReg__HandleSpace* const pSpace, const UINT_PTR block)
{
    const VReg__HandleDirectory* const pDirectory = (const VReg__HandleDirectory*)ReadPointerAcquire(
        (PVOID volatile*)&pSpace->directories[block / VREG__HANDLE_DIRECTORY_SIZE]
    );
    if (!pDirectory)
        return NULL;

    return (VReg__HandleBlock*)ReadPointerAcquire(
        (PVOID volatile*)&pDirectory->blocks[block % VREG__HANDLE_DIRECTORY_SIZE]
    );
}

static BOOL VReg__AddBlock(_Inout_ VReg__HandleSpace* const pSpace, const UINT_PTR block)
{
    VReg__HandleDirectory* volatile* const pDirectorySlot = &pSpace->directories[block / VREG__HANDLE_DIRECTORY_SIZE];
    VReg__HandleDirectory* pDirectory = *pDirectorySlot;
    VReg__HandleBlock* pBlock;

    if (!pDirectory)
    {
        pDirectory = (VReg__HandleDirectory*)VReg__AllocateZeroed(sizeof(VReg__HandleDirectory));
        if (!pDirectory)
            return FALSE;

        WritePointerRelease((PVOID volatile*)pDirectorySlot, pDirectory);
    }

    pBlock = (VReg__HandleBlock*)VReg__AllocateZeroed(sizeof(VReg__HandleBlock));
    if (!pBlock)
        return FALSE;

    WritePointerRelease((PVOID volatile*)&pDirectory->blocks[block % VREG__HANDLE_DIRECTORY_SIZE], pBlock);
    return TRUE;
}

_Ret_maybenull_ HKEY VReg__HandleSpaceAllocate(_Inout_ VReg__HandleSpace* const pSpace, _In_opt_ void* const pTarget)
{
    HKEY hKey = NULL;
    UINT_PTR index;
    VReg__HandleBlock* pBlock;

    if (!pSpace->pShared)
        return NULL;

    AcquireSRWLockExclusive(&pSpace->lock);

    if (pSpace->nextIndex == pSpace->endIndex)
    {
        // InterlockedIncrement returns the new value; the block reserved is the
        // one before it.
        const UINT_PTR block = (UINT_PTR)InterlockedIncrement(&pSpace->pShared->nextBlock) - 1;
        if (block >= VREG__HANDLE_BLOCK_COUNT || !VReg__AddBlock(pSpace, block))
        {
            ReleaseSRWLockExclusive(&pSpace->lock);
            return NULL;
        }

        pSpace->nextIndex = block * VREG__HANDLE_BLOCK_SIZE;
        pSpace->endIndex = pSpace->nextIndex + VREG__HANDLE_BLOCK_SIZE;
    }

    index = pSpace->nextIndex++;
    pBlock = VReg__FindBlock(pSpace, index / VREG__HANDLE_BLOCK_SIZE);
    VReg__StoreTarget(&pBlock->targets[index % VREG__HANDLE_BLOCK_SIZE], pTarget);
    hKey = VReg__VirtualHKeyFromIndex(index);

    ReleaseSRWLockExclusive(&pSpace->lock);
    return hKey;
}

BOOL VReg__HandleSpaceOwns(_In_ const VReg__HandleSpace* const pSpace, _In_opt_ const HKEY hKey)
{
    return
        VRegIsVirtualHKey(hKey)
        && VReg__FindBlock(pSpace, VReg__IndexFromVirtualHKey(hKey) / VREG__HANDLE_BLOCK_SIZE) != NULL;
}

void VReg__HandleSpaceRelease(_Inout_ VReg__HandleSpace* const pSpace, _In_opt_ const HKEY hKey)
{
    UINT_PTR index;
    VReg__HandleBlock* pBlock;

    if (!VRegIsVirtualHKey(hKey))
        return;

    index = VReg__IndexFromVirtualHKey(hKey);
    pBlock = VReg__FindBlock(pSpace, index / VREG__HANDLE_BLOCK_SIZE);
    if (pBlock)
        VReg__StoreTarget(&pBlock->targets[index % VREG__HANDLE_BLOCK_SIZE], NULL);
}

_Ret_maybenull_ void* VReg__HandleSpaceFind(_In_ const VReg__HandleSpace* const pSpace, _In_opt_ const HKEY hKey)
{
    UINT_PTR index;
    const VReg__HandleBlock* pBlock;

    if (!VRegIsVirtualHKey(hKey))
        return NULL;

    index = VReg__IndexFromVirtualHKey(hKey);
    pBlock = VReg__FindBlock(pSpace, index / VREG__HANDLE_BLOCK_SIZE);
    if (!pBlock)
        return NULL;

    return ReadPointerAcquire((PVOID volatile*)&pBlock->targets[index % VREG__HANDLE_BLOCK_SIZE]);
}

_Ret_maybenull_ HKEY VReg__VirtualHKeyFromIndex(const UINT_PTR index)
{
    if (index >= VREG__VIRTUAL_HKEY_COUNT)
        return NULL;

    return (HKEY)(VREG_VIRTUAL_HKEY_BASE - (index << 1));
}

UINT_PTR VReg__IndexFromVirtualHKey(_In_opt_ const HKEY hKey)
{
    return (VREG_VIRTUAL_HKEY_BASE - (UINT_PTR)hKey) >> 1;
}
