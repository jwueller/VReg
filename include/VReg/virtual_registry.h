#pragma once

#include <VReg/api.h>
#include <VReg/hive.h>
#include <VReg/registry.h>

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @ref NtQueryKey, the one `ntdll` function a virtual registry uses to find
/// the native path of a real key.
typedef LONG (NTAPI* VRegNtQueryKeyFunction)(
    HANDLE hKey,
    ULONG keyInformationClass,
    PVOID pKeyInformation,
    ULONG cbKeyInformation,
    PULONG pcbResult
);

/**
 * @brief A namespace of hives mounted at registry paths over an optional base
 *  registry.
 *
 *  A call lands in a hive when it passes a virtual handle of a mounted hive,
 *  or a predefined root plus a path at or below a mount point. Everything
 *  else goes to the base registry; with no base it answers
 *  @ref ERROR_FILE_NOT_FOUND (or @ref ERROR_INVALID_HANDLE for a handle that
 *  was never valid).
 *
 *  With a base, a call on a real key handle whose subkey path names a mount
 *  point's last component is resolved by the key's native path
 *  (@ref NtQueryKey), so a handle opened above a mount reaches it. Only
 *  mounts below @ref HKEY_LOCAL_MACHINE, @ref HKEY_USERS and
 *  @ref HKEY_CURRENT_USER have a native path.
 *
 *  The virtual registry owns the caller-buffer rules (sizes in characters or
 *  bytes, terminators, @ref ERROR_MORE_DATA) for every hive.
 *
 *  Mounting publishes a new mount table with one atomic store, and old tables
 *  are kept until the registry is destroyed, so calls in flight never take a
 *  lock to read it.
 *
 *  @p pBase answers everything outside the mounts; null for a registry held
 *  entirely in memory. @p pfnNtQueryKey replaces the `ntdll` export for native
 *  path resolution (a test seam); resolution happens here, so no call ever
 *  loads a module. Freed with @ref VRegDestroyRegistry.
 */
VREGDEF LSTATUS WINAPI VRegCreateVirtualRegistry(
    _In_opt_ VRegRegistry* pBase,
    _In_opt_ VRegNtQueryKeyFunction pfnNtQueryKey,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegRegistry** ppRegistry
);

/// Mounts @p pHive at @p pszPath below the predefined key @p hRoot (a null or
/// empty path mounts the root itself) in a registry
/// @ref VRegCreateVirtualRegistry made. @p pHive MUST outlive the registry.
/// Answers @ref ERROR_INVALID_HANDLE for a root that is not predefined or a
/// registry that is not virtual, @ref ERROR_BAD_PATHNAME for a malformed path
/// and @ref ERROR_ALREADY_EXISTS for a path already mounted. @p samView limits
/// the calls the mount answers to those of one registry view:
/// @ref KEY_WOW64_64KEY or @ref KEY_WOW64_32KEY, or 0 for both; anything else
/// answers @ref ERROR_INVALID_PARAMETER.
VREGDEF LSTATUS WINAPI VRegMount(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hRoot,
    _In_opt_z_ LPCWSTR pszPath,
    _In_ VRegHive* pHive,
    _In_ REGSAM samView
);

#ifdef __cplusplus
} // extern "C"
#endif
