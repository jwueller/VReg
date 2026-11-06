#pragma once

#include <VReg/handle.h>

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// One more than the highest index @ref VReg__VirtualHKeyFromIndex accepts.
#define VREG__VIRTUAL_HKEY_COUNT ((VREG_VIRTUAL_HKEY_BASE >> 1) + 1)

#define VREG__HANDLE_BLOCK_SIZE ((UINT_PTR)4096)
#define VREG__HANDLE_BLOCK_COUNT (VREG__VIRTUAL_HKEY_COUNT / VREG__HANDLE_BLOCK_SIZE)
#define VREG__HANDLE_DIRECTORY_SIZE ((UINT_PTR)512)

/// @ref VReg__HandleSpaceShared::magic once a copy has claimed the section:
/// "VRg1".
#define VREG__HANDLE_SPACE_MAGIC 0x56526731L

/// The handle section's contents. Separately built copies of VReg read this
/// layout, so it never changes; a different layout needs a different magic
/// value and section name.
typedef struct VReg__HandleSpaceShared
{
    /// @ref VREG__HANDLE_SPACE_MAGIC once a copy has claimed the section; zero
    /// before.
    LONG magic;
    LONG nextBlock;
} VReg__HandleSpaceShared;

typedef struct VReg__HandleDirectory VReg__HandleDirectory;

/**
 * @brief Hands out virtual handles from the process-wide namespace that every
 *  copy of VReg in the process shares.
 *
 *  Each statically linked copy has its own instance, and all instances in a
 *  process meet in one named section that holds the next free block of
 *  handle values. An instance reserves whole blocks from it and hands out
 *  values from its own blocks without further shared traffic; it owns a
 *  handle exactly when the handle falls in one of its blocks.
 *
 *  Initialized by @ref VReg__HandleSpaceInit; the members are private to
 *  `handle_space.c`.
 *
 * @note The section is mapped once and never unmapped or closed, so an
 *  instance must outlive every handle it issued: in practice, it is never
 *  destroyed (@ref VReg__ProcessHandleSpace).
 */
typedef struct VReg__HandleSpace
{
    VReg__HandleSpaceShared* pShared;

    SRWLOCK lock;
    UINT_PTR nextIndex;
    UINT_PTR endIndex;

    // Two levels of block directories, filled in as blocks are reserved and
    // never freed: a lookup is two acquire loads and an index.
    VReg__HandleDirectory* volatile directories[VREG__HANDLE_BLOCK_COUNT / VREG__HANDLE_DIRECTORY_SIZE];
} VReg__HandleSpace;

/// Opens or creates the section @p pszName. A failure leaves the instance
/// unusable: @ref VReg__HandleSpaceAllocate then answers null.
VREGDEF void VReg__HandleSpaceInit(_Out_ VReg__HandleSpace* pSpace, _In_z_ LPCWSTR pszName);

/// This copy's instance over this process's section, created on first use
/// and never destroyed.
VREGDEF _Ret_notnull_ VReg__HandleSpace* VReg__ProcessHandleSpace(void);

VREGDEF BOOL VReg__HandleSpaceIsUsable(_In_ const VReg__HandleSpace* pSpace);

/// A fresh handle owned by @p pSpace and bound to @p pTarget, or null when
/// the section is unusable, the namespace is exhausted or memory ran out.
VREGDEF _Ret_maybenull_ HKEY VReg__HandleSpaceAllocate(_Inout_ VReg__HandleSpace* pSpace, _In_opt_ void* pTarget);

/// Whether @p hKey is a virtual handle from one of @p pSpace's blocks.
/// Lock-free.
VREGDEF BOOL VReg__HandleSpaceOwns(_In_ const VReg__HandleSpace* pSpace, _In_opt_ HKEY hKey);

/// Unbinds @p hKey from its target. The value is never handed out again;
/// later lookups answer null.
VREGDEF void VReg__HandleSpaceRelease(_Inout_ VReg__HandleSpace* pSpace, _In_opt_ HKEY hKey);

/// The target @p hKey was allocated for; null when @p pSpace does not own
/// it. Lock-free.
VREGDEF _Ret_maybenull_ void* VReg__HandleSpaceFind(_In_ const VReg__HandleSpace* pSpace, _In_opt_ HKEY hKey);

/// The handle for @p index, counting down from @ref VREG_VIRTUAL_HKEY_BASE in
/// odd steps; null when @p index is out of range.
VREGDEF _Ret_maybenull_ HKEY VReg__VirtualHKeyFromIndex(UINT_PTR index);

/// The inverse of @ref VReg__VirtualHKeyFromIndex; only meaningful when
/// @ref VRegIsVirtualHKey holds.
VREGDEF UINT_PTR VReg__IndexFromVirtualHKey(_In_opt_ HKEY hKey);

#ifdef __cplusplus
} // extern "C"
#endif
