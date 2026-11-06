#pragma once

#include <VReg/api.h>
#include <VReg/registry.h>

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Storage for one subtree of a virtual namespace: a tree with one
 *  anonymous root, mounted at a path by a virtual registry, held in memory
 *  on its own or layered over a key of another registry.
 *
 *  On its own, the hive starts empty and holds everything written to it.
 *  Layered over a lower key, reads fall through to that key and writes stay
 *  in memory: the hive's own layer adds and overrides keys and values.
 *  Deleting a lower value or key leaves a tombstone that hides it; a key
 *  deleted and created again, or emptied with `RegDeleteTreeW`, becomes
 *  opaque and hides everything below it in the lower layer. Enumeration
 *  lists the hive's own entries first, then the lower ones no entry shadows;
 *  counts cover both.
 *
 *  Each key has exactly one virtual handle, issued when the key is created.
 *  Readers take the hive's lock shared and writers exclusive, until
 *  @ref VRegFreezeHive; after that nothing writes and readers take no lock
 *  at all. The lower registry is never called with the lock held, so a lower
 *  call that loads a module or takes a long time cannot hold up the hive.
 *
 *  Frozen, a layered hive answers an open of a key it has nothing at or below
 *  with the lower registry's own handle. A virtual registry sends later calls
 *  on such a handle to its base, so a frozen hive's lower registry MUST be
 *  that base or answer the same handles.
 *
 * @note A hive MUST outlive every registry it is mounted in. Destroying it
 *  turns its handles into invalid ones.
 *
 * @note Once hooked, every call on a hive may run under a foreign loader
 *  lock: nothing here loads a module, creates a thread or waits on a lock a
 *  loader-lock holder could own.
 */
typedef struct VRegHive VRegHive;

/// A key in a hive. Opaque outside the hive that owns it; never freed before
/// the hive, so a pointer to one stays valid for the hive's life.
typedef struct VReg__Node VReg__Node;

/// A hive held entirely in memory; starts empty. @p pAllocContext is passed
/// to every allocation the hive makes.
VREGDEF LSTATUS WINAPI VRegCreateHive(_In_opt_ void* pAllocContext, _Outptr_ VRegHive** ppHive);

/// A hive over the key @p pszLowerPath below @p hLowerRoot of @p pLower,
/// which MUST outlive the hive. @p samLowerView (@ref KEY_WOW64_64KEY,
/// @ref KEY_WOW64_32KEY or 0) is added to every lower open.
VREGDEF LSTATUS WINAPI VRegCreateLayeredHive(
    _In_ VRegRegistry* pLower,
    _In_ HKEY hLowerRoot,
    _In_opt_z_ LPCWSTR pszLowerPath,
    _In_ REGSAM samLowerView,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegHive** ppHive
);

/// Frees the hive and unbinds every handle it issued; null is ignored.
VREGDEF void WINAPI VRegDestroyHive(_In_opt_ VRegHive* pHive);

/// Makes the hive immutable from now on, so its readers take no lock.
/// Safe while it is mounted and in use; there is no way back.
VREGDEF void WINAPI VRegFreezeHive(_In_ VRegHive* pHive);

VREGDEF BOOL WINAPI VRegIsHiveFrozen(_In_ VRegHive* pHive);

/**
 * @brief Creates the key @p pszPath below the hive's root if needed and sets
 *  its value @p pszName to @p pszText as a @ref REG_SZ marked as MUI text.
 *
 *  `RegLoadMUIStringW` answers an unmarked hive value @ref ERROR_INVALID_DATA,
 *  since resolving an `@` resource reference would load a module. A marked
 *  one stands for the text such a reference resolves to and answers with
 *  @p pszText itself. Any other write of the value drops the mark, and a
 *  `.reg` export carries it as a plain string.
 */
VREGDEF LSTATUS WINAPI VRegSetHiveMuiTextValue(
    _In_ VRegHive* pHive,
    _In_opt_z_ LPCWSTR pszPath,
    _In_opt_z_ LPCWSTR pszName,
    _In_z_ LPCWSTR pszText
);

#ifdef __cplusplus
} // extern "C"
#endif
