#pragma once

#include <VReg/registry.h>

#include <Windows.h>

// The functions below include Vista's (RegDeleteTreeW, RegGetValueW), which
// the SDK declares only from that target on.
#if _WIN32_WINNT < 0x0600
#error "VReg needs _WIN32_WINNT 0x0600 (Windows Vista) or later."
#endif

/// Applies `X` to the name of every wide `Ex` form and every function with
/// one form only: the virtual surface of @ref VReg::Registry, and the fields
/// of @ref VReg::NativeRegistry::Functions. It serves VReg's own sources and
/// tests; it is not part of the API.
#define VREG_INTERNAL_REGISTRY_VIRTUALS(X)                                     \
    X(RegOpenKeyExW)                                                           \
    X(RegCreateKeyExW)                                                         \
    X(RegCloseKey)                                                             \
    X(RegQueryValueExW)                                                        \
    X(RegSetValueExW)                                                          \
    X(RegDeleteValueW)                                                         \
    X(RegDeleteKeyExW)                                                         \
    X(RegDeleteTreeW)                                                          \
    X(RegEnumKeyExW)                                                           \
    X(RegEnumValueW)                                                           \
    X(RegQueryInfoKeyW)                                                        \
    X(RegLoadMUIStringW)                                                       \
    X(RegGetValueW)                                                            \
    X(RegNotifyChangeKeyValue)                                                 \
    X(RegFlushKey)

/// Applies `X` to the name of every other function @ref VReg::Registry
/// mirrors: the ANSI forms, the pre-`Ex` forms and the conveniences Win32
/// builds from the virtuals. The hooks emulate these too, but no registry
/// calls them, so their trampolines stay with the hooks.
#define VREG_INTERNAL_REGISTRY_FORMS(X)                                        \
    X(RegOpenKeyExA)                                                           \
    X(RegOpenKeyW)                                                             \
    X(RegOpenKeyA)                                                             \
    X(RegCreateKeyExA)                                                         \
    X(RegCreateKeyW)                                                           \
    X(RegCreateKeyA)                                                           \
    X(RegQueryValueExA)                                                        \
    X(RegQueryValueW)                                                          \
    X(RegQueryValueA)                                                          \
    X(RegSetValueExA)                                                          \
    X(RegSetValueW)                                                            \
    X(RegSetValueA)                                                            \
    X(RegDeleteValueA)                                                         \
    X(RegDeleteKeyExA)                                                         \
    X(RegDeleteKeyW)                                                           \
    X(RegDeleteKeyA)                                                           \
    X(RegDeleteTreeA)                                                          \
    X(RegEnumKeyExA)                                                           \
    X(RegEnumKeyW)                                                             \
    X(RegEnumKeyA)                                                             \
    X(RegEnumValueA)                                                           \
    X(RegQueryInfoKeyA)                                                        \
    X(RegLoadMUIStringA)                                                       \
    X(RegGetValueA)                                                            \
    X(RegSetKeyValueW)                                                         \
    X(RegSetKeyValueA)                                                         \
    X(RegDeleteKeyValueW)                                                      \
    X(RegDeleteKeyValueA)                                                      \
    X(RegQueryMultipleValuesW)                                                 \
    X(RegQueryMultipleValuesA)

/// Both lists: every function the @ref VReg::Registry surface mirrors.
#define VREG_INTERNAL_REGISTRY_FUNCTIONS(X)                                    \
    VREG_INTERNAL_REGISTRY_VIRTUALS(X)                                         \
    VREG_INTERNAL_REGISTRY_FORMS(X)

#ifdef __cplusplus
extern "C" {
#endif

/// The entry points the native registry calls and the hooks attach to: the
/// wide `Ex` forms and the functions with one form only. The ANSI, pre-`Ex`
/// and convenience forms are derived from these, as everywhere.
typedef struct VRegNativeFunctions
{
    LSTATUS (WINAPI* RegOpenKeyExW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey,
        _In_ DWORD ulOptions,
        _In_ REGSAM samDesired,
        _Out_ PHKEY phkResult
    );

    LSTATUS (WINAPI* RegCreateKeyExW)(
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _Reserved_ DWORD Reserved,
        _In_opt_ LPWSTR lpClass,
        _In_ DWORD dwOptions,
        _In_ REGSAM samDesired,
        _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        _Out_ PHKEY phkResult,
        _Out_opt_ LPDWORD lpdwDisposition
    );

    LSTATUS (WINAPI* RegCloseKey)(
        _In_ HKEY hKey
    );

    LSTATUS (WINAPI* RegQueryValueExW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _When_(lpData == NULL, _Out_opt_) _When_(lpData != NULL, _Inout_opt_) LPDWORD lpcbData
    );

    LSTATUS (WINAPI* RegSetValueExW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName,
        _Reserved_ DWORD Reserved,
        _In_ DWORD dwType,
        _In_reads_bytes_opt_(cbData) const BYTE* lpData,
        _In_ DWORD cbData
    );

    LSTATUS (WINAPI* RegDeleteValueW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpValueName
    );

    LSTATUS (WINAPI* RegDeleteKeyExW)(
        _In_ HKEY hKey,
        _In_ LPCWSTR lpSubKey,
        _In_ REGSAM samDesired,
        _Reserved_ DWORD Reserved
    );

    LSTATUS (WINAPI* RegDeleteTreeW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR lpSubKey
    );

    LSTATUS (WINAPI* RegEnumKeyExW)(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchName, *lpcchName + 1) LPWSTR lpName,
        _Inout_ LPDWORD lpcchName,
        _Reserved_ LPDWORD lpReserved,
        _Out_writes_to_opt_(*lpcchClass, *lpcchClass + 1) LPWSTR lpClass,
        _Inout_opt_ LPDWORD lpcchClass,
        _Out_opt_ PFILETIME lpftLastWriteTime
    );

    LSTATUS (WINAPI* RegEnumValueW)(
        _In_ HKEY hKey,
        _In_ DWORD dwIndex,
        _Out_writes_to_opt_(*lpcchValueName, *lpcchValueName + 1) LPWSTR lpValueName,
        _Inout_ LPDWORD lpcchValueName,
        _Reserved_ LPDWORD lpReserved,
        _Out_opt_ LPDWORD lpType,
        _Out_writes_bytes_to_opt_(*lpcbData, *lpcbData) __out_data_source(REGISTRY) LPBYTE lpData,
        _Inout_opt_ LPDWORD lpcbData
    );

    LSTATUS (WINAPI* RegQueryInfoKeyW)(
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
    );

    LSTATUS (WINAPI* RegLoadMUIStringW)(
        _In_ HKEY hKey,
        _In_opt_ LPCWSTR pszValue,
        _Out_writes_bytes_opt_(cbOutBuf) LPWSTR pszOutBuf,
        _In_ DWORD cbOutBuf,
        _Out_opt_ LPDWORD pcbData,
        _In_ DWORD Flags,
        _In_opt_ LPCWSTR pszDirectory
    );

    LSTATUS (WINAPI* RegGetValueW)(
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
    );

    LSTATUS (WINAPI* RegNotifyChangeKeyValue)(
        _In_ HKEY hKey,
        _In_ BOOL bWatchSubtree,
        _In_ DWORD dwNotifyFilter,
        _In_opt_ HANDLE hEvent,
        _In_ BOOL fAsynchronous
    );

    LSTATUS (WINAPI* RegFlushKey)(
        _In_ HKEY hKey
    );
} VRegNativeFunctions;

/// Fills @p pFunctions from the export tables of `KernelBase.dll` and, for
/// the pre-`Ex` forms only it exports, `advapi32.dll`, never through
/// @ref GetProcAddress: an apphelp shim can make that answer a stub instead of
/// the entry point. Resolution never loads a module (VReg links advapi32, so
/// both are loaded before it runs), which makes it safe under the loader
/// lock. A function neither module exports stays null.
void WINAPI VRegResolveNativeFunctions(_Out_ VRegNativeFunctions* pFunctions);

/// The real registry, reached through @p pFunctions, or through
/// @ref VRegResolveNativeFunctions when that is null. The table is copied; the
/// live copy, which the hooks attach to, is @ref VRegGetNativeFunctions.
/// Freed with @ref VRegDestroyRegistry.
LSTATUS WINAPI VRegCreateNativeRegistry(
    _In_opt_ const VRegNativeFunctions* pFunctions,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegRegistry** ppRegistry
);

/// The live table of a registry @ref VRegCreateNativeRegistry made: once the
/// hooks are committed it holds the trampolines, and the registry is the one
/// below them.
_Ret_notnull_ VRegNativeFunctions* WINAPI VRegGetNativeFunctions(_In_ VRegRegistry* pRegistry);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

namespace VReg
{
/**
 * @brief The real registry, reached through a table of function pointers.
 *
 *  The default table is resolved from the export tables of `KernelBase.dll`
 *  and, for the pre-`Ex` forms only it exports, `advapi32.dll`, never through
 *  @ref GetProcAddress: an apphelp shim can make that answer a stub instead of
 *  the entry point. Resolution never loads a module (VReg links advapi32, so
 *  both are loaded before it runs), which makes it safe under the loader
 *  lock.
 *
 *  The table holds the virtual surface of @ref Registry, the functions this
 *  class calls; the ANSI, pre-`Ex` and convenience forms come from
 *  @ref Registry over them, as everywhere. The hooks attach to these
 *  fields, so once the hooks are committed the fields hold the trampolines
 *  and this registry is the one below them.
 */
class NativeRegistry final : public Registry
{
public:
    using Functions = VRegNativeFunctions;

    /// The default table. A function neither module exports stays null.
    [[nodiscard]] static Functions ResolveFunctions() noexcept;

    /// Over @ref ResolveFunctions. Throws @ref std::bad_alloc.
    NativeRegistry();
    explicit NativeRegistry(const Functions& functions);

    ~NativeRegistry() override;

    /// The live table: what the hooks attach to.
    [[nodiscard]] Functions& GetFunctions() noexcept;

    /// The C registry this one stands for.
    [[nodiscard]] VRegRegistry* CObject() noexcept override;

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
    VRegRegistry* m_pRegistry;
};
} // namespace VReg

#endif // __cplusplus
