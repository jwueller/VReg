#pragma once

#include <VReg/hive.h>
#include <VReg/registry.h>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace VReg
{
/// @ref NtQueryKey, the one `ntdll` function a @ref VirtualRegistry uses to
/// find the native path of a real key.
using NtQueryKeyFunction = LONG(NTAPI*)(
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
 *  This class owns the caller-buffer rules (sizes in characters or bytes,
 *  terminators, @ref ERROR_MORE_DATA) for every hive; the ANSI forms come from
 *  @ref Registry over these wide forms.
 *
 *  Mounting publishes a new mount table with one atomic store, and old tables
 *  are never freed, so calls in flight never take a lock to read it.
 */
class VirtualRegistry final : public Registry
{
public:
    /// @p pBase answers everything outside the mounts; null for a registry
    /// held entirely in memory. @p pfnNtQueryKey replaces the `ntdll` export
    /// for native path resolution (a test seam); resolution happens here, so
    /// no call ever loads a module.
    explicit VirtualRegistry(
        _In_opt_ Registry* pBase = nullptr,
        _In_opt_ NtQueryKeyFunction pfnNtQueryKey = nullptr
    ) noexcept;

    VirtualRegistry(const VirtualRegistry&) = delete;
    VirtualRegistry& operator=(const VirtualRegistry&) = delete;

    /// Mounts @p hive at @p pszPath below the predefined key @p hRoot (an empty
    /// path mounts the root itself). @p hive MUST outlive this registry.
    /// Answers @ref ERROR_INVALID_HANDLE for a root that is not predefined,
    /// @ref ERROR_BAD_PATHNAME for a malformed path and
    /// @ref ERROR_ALREADY_EXISTS for a path already mounted. @p samView limits
    /// the calls the mount answers to those of one registry view:
    /// @ref KEY_WOW64_64KEY or @ref KEY_WOW64_32KEY, or 0 for both; anything
    /// else answers @ref ERROR_INVALID_PARAMETER.
    LSTATUS Mount(
        HKEY hRoot,
        _In_z_ LPCWSTR pszPath,
        Hive& hive,
        REGSAM samView = 0
    ) noexcept;

    /// @ref Mount for reading only: this registry calls only the const
    /// members of @p hive and answers every write through the mount
    /// @ref ERROR_ACCESS_DENIED, as a frozen hive does, while the hive itself
    /// stays writable to whoever holds it non-const. One hive is mounted
    /// either way within a registry, never both, since its handles belong to
    /// its nodes and not to a mount: mounting it the other way answers
    /// @ref ERROR_INVALID_PARAMETER.
    LSTATUS Mount(
        HKEY hRoot,
        _In_z_ LPCWSTR pszPath,
        const Hive& hive,
        REGSAM samView = 0
    ) noexcept;

    LSTATUS RegOpenKeyExW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_ DWORD ulOptions,
        _In_ REGSAM samDesired,
        _Out_ PHKEY phkResult
    ) noexcept override;

    LSTATUS RegCreateKeyExW(
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _Reserved_ DWORD Reserved,
        _In_opt_ LPWSTR lpClass,
        _In_ DWORD dwOptions,
        _In_ REGSAM samDesired,
        _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _Out_ PHKEY phkResult,
        _Out_opt_ LPDWORD lpdwDisposition
    ) noexcept override;

    LSTATUS RegCloseKey(
        _In_ HKEY hKey
    ) noexcept override;

    LSTATUS RegQueryValueExW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
    ) const noexcept override;

    LSTATUS RegSetValueExW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ DWORD Reserved,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* lpData,
        _In_ DWORD cbData
    ) noexcept override;

    LSTATUS RegDeleteValueW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName
    ) noexcept override;

    LSTATUS RegDeleteKeyExW(
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _In_ REGSAM samDesired,
        _Reserved_ DWORD Reserved
    ) noexcept override;

    LSTATUS RegDeleteTreeW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey
    ) noexcept override;

    LSTATUS RegEnumKeyExW(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPWSTR lpName,
        _Inout_ LPDWORD lpcchName,
        _Reserved_ LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Out_opt_ PFILETIME lpftLastWriteTime
    ) const noexcept override;

    LSTATUS RegEnumValueW(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPWSTR lpValueName,
        _Inout_ LPDWORD lpcchValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _Inout_opt_ LPDWORD lpcbData
    ) const noexcept override;

    LSTATUS RegQueryInfoKeyW(
        _In_ HKEY hKey,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpcSubKeys,
        _Out_opt_ LPDWORD lpcbMaxSubKeyLen,
        _Out_opt_ LPDWORD lpcbMaxClassLen,
        _Out_opt_ LPDWORD lpcValues,
        _Out_opt_ LPDWORD lpcbMaxValueNameLen,
        _Out_opt_ LPDWORD lpcbMaxValueLen,
        _Out_opt_ LPDWORD lpcbSecurityDescriptor,
        _Out_opt_ PFILETIME lpftLastWriteTime
    ) const noexcept override;

    LSTATUS RegLoadMUIStringW(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) LPWSTR pszOutBuf,
        _In_ DWORD cbOutBuf,
        _Out_opt_ LPDWORD pcbData,
        _In_ DWORD Flags,
        _In_opt_ LPCWSTR pszDirectory
    ) const noexcept override;

    LSTATUS RegGetValueW(
        _In_ HKEY hkey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_opt_ LPCWSTR lpValue,
        _In_ DWORD dwFlags,
        _Out_opt_ LPDWORD pdwType,
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
        _Out_writes_bytes_to_opt_(*pcbData, *pcbData) PVOID pvData,
        _Inout_opt_ LPDWORD pcbData
    ) const noexcept override;

    LSTATUS RegNotifyChangeKeyValue(
        _In_ HKEY hKey,
        _In_ BOOL bWatchSubtree,
        _In_ DWORD dwNotifyFilter,
        _In_opt_ HANDLE hEvent,
        _In_ BOOL fAsynchronous
    ) noexcept override;

    LSTATUS RegFlushKey(
        _In_ HKEY hKey
    ) noexcept override;

private:
    struct MountPoint
    {
        HKEY hRoot;
        std::vector<std::wstring> path;
        const Hive* pHive;

        /// @ref pHive for writing; null for a read-only mount.
        Hive* pWritable;

        /// @ref KEY_WOW64_64KEY, @ref KEY_WOW64_32KEY or 0 for both views.
        REGSAM samView;

        /// The native key path as components without `WOW6432Node`; empty
        /// when the mount has none.
        std::vector<std::wstring> nativePath;
    };

    struct MountTable
    {
        std::vector<MountPoint> mounts;
    };

    /// Where a key handle plus a subkey path lands.
    struct Target
    {
        /// The node, when the key is inside a mount.
        Node* pNode = nullptr;

        /// The hive of @ref pNode, and the same hive for writing when it is
        /// mounted writable here.
        const Hive* pHive = nullptr;
        Hive* pWritable = nullptr;

        /// The components below @ref pNode still to walk.
        std::vector<std::wstring_view> rest;

        /// Whether the call goes to the base registry, because the key is
        /// outside every mount.
        bool isBase = false;

        LSTATUS status = ERROR_SUCCESS;

        /// Holds the native path text @ref rest may point into.
        std::unique_ptr<const std::wstring> pNativeText;
    };

    [[nodiscard]] const MountTable& Mounts() const noexcept;

    /// A mount of @p pHive here; null when it is not mounted.
    [[nodiscard]] const MountPoint* MountOf(const Hive* pHive) const noexcept;

    LSTATUS MountHive(
        HKEY hRoot,
        std::wstring_view path,
        const Hive& hive,
        Hive* pWritable,
        REGSAM samView
    ) noexcept;

    /// Resolves @p hKey and @p subKey (null or empty for the key itself).
    /// @p samDesired supplies the registry view of the call.
    [[nodiscard]] Target Resolve(
        HKEY hKey,
        std::wstring_view subKey,
        REGSAM samDesired = 0
    ) const;

    /// Looks for a mount a real key plus @p subComponents lands in, by native
    /// path; fills @p target and answers true when it does.
    [[nodiscard]] bool ResolveNative(
        HKEY hKey,
        const std::vector<std::wstring_view>& subComponents,
        REGSAM samDesired,
        Target& target
    ) const;

    Registry* m_pBase;
    NtQueryKeyFunction m_pfnNtQueryKey;
    std::atomic<const MountTable*> m_pMounts;
    SRWLOCK m_mountLock = SRWLOCK_INIT;
};
} // namespace VReg
