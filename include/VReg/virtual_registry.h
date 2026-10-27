#pragma once

#include <VReg/hive.h>
#include <VReg/registry.h>

#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace VReg
{
/**
 * @brief A namespace of hives mounted at registry paths over an optional base
 *  registry.
 *
 *  A call lands in a hive when its key is a virtual handle of a mounted hive,
 *  or a predefined root plus a path at or below a mount point. Everything
 *  else goes to the base registry; with no base it answers
 *  `ERROR_FILE_NOT_FOUND` (or `ERROR_INVALID_HANDLE` for a handle that was
 *  never valid).
 *
 *  This class owns the caller-buffer rules (sizes in characters or bytes,
 *  terminators, `ERROR_MORE_DATA`) for every hive; the A forms come from
 *  @ref Registry over these W forms.
 *
 *  Mounting publishes a new mount table with one atomic store, and old tables
 *  are never freed, so calls in flight never take a lock to read it.
 */
class VirtualRegistry final : public Registry
{
public:
    /// @p pBase answers everything outside the mounts; null for a registry
    /// held entirely in memory.
    explicit VirtualRegistry(Registry* pBase = nullptr) noexcept;

    VirtualRegistry(const VirtualRegistry&) = delete;
    VirtualRegistry& operator=(const VirtualRegistry&) = delete;

    /// Mounts @p hive at @p path below the predefined key @p hRoot (an empty
    /// path mounts the root itself). @p hive must outlive this registry.
    /// Answers `ERROR_INVALID_HANDLE` for a root that is not predefined,
    /// `ERROR_BAD_PATHNAME` for a malformed path and `ERROR_ALREADY_EXISTS`
    /// for a path already mounted.
    LSTATUS Mount(HKEY hRoot, std::wstring_view path, Hive& hive) noexcept;

    LSTATUS OpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) noexcept
        override;
    LSTATUS CreateKeyExW(
        HKEY hKey,
        LPCWSTR lpSubKey,
        DWORD Reserved,
        LPWSTR lpClass,
        DWORD dwOptions,
        REGSAM samDesired,
        const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        PHKEY phkResult,
        LPDWORD lpdwDisposition
    ) noexcept override;
    LSTATUS CloseKey(HKEY hKey) noexcept override;
    LSTATUS QueryValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) noexcept override;
    LSTATUS SetValueExW(
        HKEY hKey,
        LPCWSTR lpValueName,
        DWORD Reserved,
        DWORD dwType,
        const BYTE* lpData,
        DWORD cbData
    ) noexcept override;
    LSTATUS DeleteValueW(HKEY hKey, LPCWSTR lpValueName) noexcept override;
    LSTATUS DeleteKeyExW(HKEY hKey, LPCWSTR lpSubKey, REGSAM samDesired, DWORD Reserved) noexcept override;
    LSTATUS DeleteTreeW(HKEY hKey, LPCWSTR lpSubKey) noexcept override;
    LSTATUS EnumKeyExW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpName,
        LPDWORD lpcchName,
        LPDWORD lpReserved,
        LPWSTR lpClass,
        LPDWORD lpcchClass,
        PFILETIME lpftLastWriteTime
    ) noexcept override;
    LSTATUS EnumValueW(
        HKEY hKey,
        DWORD dwIndex,
        LPWSTR lpValueName,
        LPDWORD lpcchValueName,
        LPDWORD lpReserved,
        LPDWORD lpType,
        LPBYTE lpData,
        LPDWORD lpcbData
    ) noexcept override;
    LSTATUS QueryInfoKeyW(
        HKEY hKey,
        LPWSTR lpClass,
        LPDWORD lpcchClass,
        LPDWORD lpReserved,
        LPDWORD lpcSubKeys,
        LPDWORD lpcbMaxSubKeyLen,
        LPDWORD lpcbMaxClassLen,
        LPDWORD lpcValues,
        LPDWORD lpcbMaxValueNameLen,
        LPDWORD lpcbMaxValueLen,
        LPDWORD lpcbSecurityDescriptor,
        PFILETIME lpftLastWriteTime
    ) noexcept override;
    LSTATUS LoadMUIStringW(
        HKEY hKey,
        LPCWSTR pszValue,
        LPWSTR pszOutBuf,
        DWORD cbOutBuf,
        LPDWORD pcbData,
        DWORD Flags,
        LPCWSTR pszDirectory
    ) noexcept override;
    LSTATUS GetValueW(
        HKEY hkey,
        LPCWSTR lpSubKey,
        LPCWSTR lpValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        PVOID pvData,
        LPDWORD pcbData
    ) noexcept override;
    LSTATUS NotifyChangeKeyValue(
        HKEY hKey,
        BOOL bWatchSubtree,
        DWORD dwNotifyFilter,
        HANDLE hEvent,
        BOOL fAsynchronous
    ) noexcept override;
    LSTATUS FlushKey(HKEY hKey) noexcept override;

private:
    struct MountPoint
    {
        HKEY hRoot;
        std::vector<std::wstring> path;
        Hive* pHive;
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
        /// The components below @ref pNode still to walk.
        std::vector<std::wstring_view> rest;
        /// The handle to pass to the base, when the key is outside every mount.
        HKEY hBase = nullptr;
        LSTATUS status = ERROR_SUCCESS;
    };

    [[nodiscard]] const MountTable& Mounts() const noexcept;
    [[nodiscard]] bool IsMounted(const Hive* pHive) const noexcept;

    /// Resolves @p hKey and @p subKey (null or empty for the key itself).
    [[nodiscard]] Target Resolve(HKEY hKey, std::wstring_view subKey) const;

    Registry* m_pBase;
    std::atomic<const MountTable*> m_pMounts;
    std::mutex m_mountMutex;
};
} // namespace VReg
