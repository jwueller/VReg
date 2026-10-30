#include <VReg/virtual_registry.h>

#include <VReg/handle.h>

#include "export_table.h"
#include "handle_space.h"
#include "lock.h"
#include "status.h"
#include "tree.h"
#include "value_type.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <iterator>
#include <span>

#include <sddl.h>

namespace VReg
{
namespace
{
bool IsPredefinedHKey(const HKEY hKey) noexcept
{
    const auto value = reinterpret_cast<ULONG_PTR>(hKey);
    return value >= reinterpret_cast<ULONG_PTR>(HKEY_CLASSES_ROOT)
        && value <= reinterpret_cast<ULONG_PTR>(HKEY_CURRENT_USER_LOCAL_SETTINGS);
}

/// The longest key name component the registry accepts, in characters.
constexpr std::size_t MAX_KEY_NAME_CHARS = 255;

/// Splits @p path at backslashes as the native registry does: empty
/// components are skipped, a trailing backslash is ignored, and a leading one
/// is a bad path name.
LSTATUS SplitPath(
    const std::wstring_view path,
    std::vector<std::wstring_view>& components
)
{
    components.clear();
    if (!path.empty() && path.front() == L'\\')
        return ERROR_BAD_PATHNAME;

    std::size_t start = 0;
    while (start < path.size())
    {
        const auto end = std::min(path.find(L'\\', start), path.size());
        const auto component = path.substr(start, end - start);
        if (!component.empty())
        {
            if (component.size() > MAX_KEY_NAME_CHARS)
                return ERROR_INVALID_PARAMETER;

            components.push_back(component);
        }

        start = end + 1;
    }

    return ERROR_SUCCESS;
}

std::wstring_view View(const LPCWSTR psz) noexcept
{
    return psz ? std::wstring_view(psz) : std::wstring_view();
}

/// Copies @p value into a caller buffer of @p cbBuffer bytes that is known to
/// hold it. Native adds a terminator after a string that lacks one, when it
/// fits, without counting it in the size.
void CopyValue(
    const LPBYTE pbBuffer,
    const DWORD cbBuffer,
    const DWORD dwType,
    const std::vector<std::byte>& value
) noexcept
{
    std::memcpy(pbBuffer, value.data(), value.size());
    if (!IsStringType(dwType))
        return;

    const auto cbWhole = value.size() / sizeof(wchar_t) * sizeof(wchar_t);
    if (cbWhole == 0)
        return;

    const bool isTerminated = value[cbWhole - 2] == std::byte{0} && value[cbWhole - 1] == std::byte{0};
    if (!isTerminated && cbWhole + sizeof(wchar_t) <= cbBuffer)
        std::memset(pbBuffer + cbWhole, 0, sizeof(wchar_t));
}

std::span<const std::byte> AsBytes(
    const BYTE* const pb,
    const DWORD cb
) noexcept
{
    return {reinterpret_cast<const std::byte*>(pb), pb ? cb : 0};
}

bool IsWow6432Node(const std::wstring_view name) noexcept
{
    return CompareNames(name, L"WOW6432Node") == 0;
}

constexpr MountView PROCESS_VIEW = sizeof(void*) == 8 ? MountView::Only64 : MountView::Only32;

MountView ViewFromSam(const REGSAM samDesired) noexcept
{
    if (samDesired & KEY_WOW64_64KEY)
        return MountView::Only64;
    if (samDesired & KEY_WOW64_32KEY)
        return MountView::Only32;

    return PROCESS_VIEW;
}

bool Admits(const MountView mountView, const MountView callView) noexcept
{
    return mountView == MountView::Both || mountView == callView;
}

/// The string form of the current user's SID; empty when it cannot be read.
std::wstring CurrentUserSid()
{
    HANDLE hToken{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return {};

    std::wstring sid;
    DWORD cbUser = 0;
    GetTokenInformation(hToken, TokenUser, nullptr, 0, &cbUser);
    std::vector<std::byte> user(cbUser);
    if (cbUser && GetTokenInformation(hToken, TokenUser, user.data(), cbUser, &cbUser))
    {
        LPWSTR pszSid = nullptr;
        if (ConvertSidToStringSidW(reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid, &pszSid))
        {
            sid = pszSid;
            LocalFree(pszSid);
        }
    }

    CloseHandle(hToken);
    return sid;
}

/// The native path of a predefined root as components; empty for a root with
/// none.
std::vector<std::wstring> NativeRootPath(const HKEY hRoot)
{
    if (hRoot == HKEY_LOCAL_MACHINE)
        return {L"REGISTRY", L"MACHINE"};
    if (hRoot == HKEY_USERS)
        return {L"REGISTRY", L"USER"};
    if (hRoot == HKEY_CURRENT_USER)
    {
        auto sid = CurrentUserSid();
        if (!sid.empty())
            return {L"REGISTRY", L"USER", std::move(sid)};
    }

    return {};
}

constexpr ULONG KEY_NAME_INFORMATION_CLASS = 3;
constexpr LONG NT_STATUS_BUFFER_OVERFLOW = static_cast<LONG>(0x80000005);
constexpr LONG NT_STATUS_BUFFER_TOO_SMALL = static_cast<LONG>(0xC0000023);

/// Whether @p full, with `WOW6432Node` components skipped, starts with
/// @p mountPath; @p iRest then indexes the first component past the match and
/// @p isWow says a `WOW6432Node` was skipped.
bool MatchesNative(
    const std::vector<std::wstring>& mountPath,
    const std::vector<std::wstring_view>& full,
    bool& isWow,
    std::size_t& iRest
) noexcept
{
    std::size_t iMount = 0;
    std::size_t i = 0;
    isWow = false;
    for (; i < full.size() && iMount < mountPath.size(); ++i)
    {
        if (IsWow6432Node(full[i]))
        {
            isWow = true;
            continue;
        }
        if (CompareNames(full[i], mountPath[iMount]) != 0)
            return false;

        ++iMount;
    }

    iRest = i;
    return iMount == mountPath.size();
}
} // namespace

VirtualRegistry::VirtualRegistry(
    Registry* const pBase,
    const NtQueryKeyFunction pfnNtQueryKey
) noexcept :
    m_pBase(pBase),
    m_pfnNtQueryKey(pfnNtQueryKey),
    m_pMounts(nullptr)
{
    if (!m_pfnNtQueryKey)
    {
        m_pfnNtQueryKey = reinterpret_cast<NtQueryKeyFunction>(
            FindExport(GetModuleHandleW(L"ntdll.dll"), "NtQueryKey")
        );
    }
}

LSTATUS VirtualRegistry::Mount(
    const HKEY hRoot,
    const std::wstring_view path,
    Hive& hive,
    const MountView view
) noexcept
{
    return MountHive(hRoot, path, hive, &hive, view);
}

LSTATUS VirtualRegistry::Mount(
    const HKEY hRoot,
    const std::wstring_view path,
    const Hive& hive,
    const MountView view
) noexcept
{
    return MountHive(hRoot, path, hive, nullptr, view);
}

LSTATUS VirtualRegistry::MountHive(
    const HKEY hRoot,
    const std::wstring_view path,
    const Hive& hive,
    Hive* const pWritable,
    const MountView view
) noexcept
try
{
    if (!IsPredefinedHKey(hRoot))
        return ERROR_INVALID_HANDLE;

    std::vector<std::wstring_view> components;
    if (SplitPath(path, components) != ERROR_SUCCESS)
        return ERROR_BAD_PATHNAME;

    MountPoint mount{
        .hRoot = hRoot,
        .path = {components.begin(), components.end()},
        .pHive = &hive,
        .pWritable = pWritable,
        .view = view,
        .nativePath = NativeRootPath(hRoot),
    };
    if (!mount.nativePath.empty())
    {
        for (const auto& component : mount.path)
        {
            if (!IsWow6432Node(component))
                mount.nativePath.push_back(component);
        }
    }

    ExclusiveLock lock(m_mountLock);
    const auto* const pOld = m_pMounts.load(std::memory_order_acquire);
    auto* const pNew = new MountTable(pOld ? *pOld : MountTable{});
    for (const auto& existing : pNew->mounts)
    {
        if (existing.pHive == &hive && !existing.pWritable != !pWritable)
        {
            delete pNew;
            return ERROR_INVALID_PARAMETER;
        }

        if (existing.hRoot == hRoot && std::ranges::equal(
            existing.path,
            mount.path,
            [](const auto& a, const auto& b)
            {
                return CompareNames(a, b) == 0;
            }
        ))
        {
            delete pNew;
            return ERROR_ALREADY_EXISTS;
        }
    }

    pNew->mounts.push_back(std::move(mount));

    // The old table is leaked on purpose: a call in flight may still read it.
    m_pMounts.store(pNew, std::memory_order_release);
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

const VirtualRegistry::MountTable& VirtualRegistry::Mounts() const noexcept
{
    static const MountTable s_empty;
    const auto* const pMounts = m_pMounts.load(std::memory_order_acquire);
    return pMounts ? *pMounts : s_empty;
}

const VirtualRegistry::MountPoint* VirtualRegistry::MountOf(
    const Hive* const pHive
) const noexcept
{
    const auto& mounts = Mounts().mounts;
    const auto it = std::ranges::find(mounts, pHive, &MountPoint::pHive);
    return it == mounts.end() ? nullptr : &*it;
}

bool VirtualRegistry::ResolveNative(
    const HKEY hKey,
    const std::vector<std::wstring_view>& subComponents,
    const REGSAM samDesired,
    Target& target
) const
{
    const auto& mounts = Mounts().mounts;
    const bool isGated = std::ranges::any_of(
        mounts,
        [&](const MountPoint& mount)
        {
            return !mount.path.empty()
                && std::ranges::any_of(
                    subComponents,
                    [&](const std::wstring_view component)
                    {
                        return CompareNames(component, mount.path.back()) == 0;
                    }
                );
        }
    );
    if (!isGated || !m_pfnNtQueryKey || IsVirtualHKey(hKey))
        return false;

    // The name is a KEY_NAME_INFORMATION: a byte count, then the characters.
    std::vector<std::byte> buffer(1024);
    for (;;)
    {
        ULONG cbResult = 0;
        const auto ntStatus
            = m_pfnNtQueryKey(
                hKey,
                KEY_NAME_INFORMATION_CLASS,
                buffer.data(),
                static_cast<ULONG>(buffer.size()),
                &cbResult
            );
        if (ntStatus >= 0)
            break;
        if (
            (
                ntStatus != NT_STATUS_BUFFER_OVERFLOW
                && ntStatus != NT_STATUS_BUFFER_TOO_SMALL
            )
            || cbResult <= buffer.size()
        )
            return false;

        buffer.resize(cbResult);
    }

    ULONG cbName{};
    std::memcpy(&cbName, buffer.data(), sizeof(cbName));
    if (cbName > buffer.size() - sizeof(ULONG))
        return false;

    auto pText = std::make_unique<const std::wstring>(
        reinterpret_cast<const wchar_t*>(buffer.data() + sizeof(ULONG)),
        cbName / sizeof(wchar_t)
    );

    std::vector<std::wstring_view> full;
    for (std::size_t start = 0; start < pText->size();)
    {
        const auto end = std::min(pText->find(L'\\', start), pText->size());
        if (end > start)
            full.emplace_back(pText->data() + start, end - start);

        start = end + 1;
    }

    full.insert(full.end(), subComponents.begin(), subComponents.end());

    const auto callView = ViewFromSam(samDesired);
    const MountPoint* pBest = nullptr;
    std::size_t iBestRest = 0;
    for (const auto& mount : mounts)
    {
        bool isWow = false;
        std::size_t iRest = 0;
        if (mount.nativePath.empty() || !MatchesNative(mount.nativePath, full, isWow, iRest)
            || !Admits(mount.view, isWow ? MountView::Only32 : callView))
            continue;

        if (!pBest || mount.nativePath.size() > pBest->nativePath.size())
        {
            pBest = &mount;
            iBestRest = iRest;
        }
    }

    if (!pBest)
        return false;

    target.pNode = pBest->pHive->Root();
    target.pHive = pBest->pHive;
    target.pWritable = pBest->pWritable;
    target.rest.assign(
        full.begin() + static_cast<std::ptrdiff_t>(iBestRest),
        full.end()
    );
    target.pNativeText = std::move(pText);
    return true;
}

VirtualRegistry::Target VirtualRegistry::Resolve(
    const HKEY hKey,
    const std::wstring_view subKey,
    const REGSAM samDesired
) const
{
    Target target;
    std::vector<std::wstring_view> components;
    const auto splitStatus = SplitPath(subKey, components);
    const bool isSplit = splitStatus == ERROR_SUCCESS;

    if (auto* const pNode = Hive::NodeOf(hKey))
    {
        if (const auto* const pMount = MountOf(Hive::HiveOf(pNode)))
        {
            if (isSplit)
            {
                target.pNode = pNode;
                target.pHive = pMount->pHive;
                target.pWritable = pMount->pWritable;
                target.rest = std::move(components);
            }
            else
            {
                target.status = splitStatus;
            }
            return target;
        }
    }
    else if (HandleSpace::Process().Owns(hKey))
    {
        target.status = ERROR_INVALID_HANDLE;
        return target;
    }

    if (IsPredefinedHKey(hKey))
    {
        const auto callView = ViewFromSam(samDesired);
        const MountPoint* pBest = nullptr;
        for (const auto& mount : Mounts().mounts)
        {
            if (!isSplit || mount.hRoot != hKey || mount.path.size() > components.size()
                || !Admits(mount.view, callView))
                continue;

            if (!std::equal(
                mount.path.begin(),
                mount.path.end(),
                components.begin(),
                [](const auto& a, const auto& b)
                {
                    return CompareNames(a, b) == 0;
                }
            ))
                continue;

            if (!pBest || mount.path.size() > pBest->path.size())
                pBest = &mount;
        }

        if (pBest)
        {
            target.pNode = pBest->pHive->Root();
            target.pHive = pBest->pHive;
            target.pWritable = pBest->pWritable;
            target.rest.assign(
                components.begin() + pBest->path.size(),
                components.end()
            );
            return target;
        }

        if (m_pBase)
            target.isBase = true;
        else
            target.status = isSplit ? ERROR_FILE_NOT_FOUND : splitStatus;

        return target;
    }

    if (!m_pBase)
    {
        target.status = ERROR_INVALID_HANDLE;
        return target;
    }

    if (!isSplit || !ResolveNative(hKey, components, samDesired, target))
        target.isBase = true;

    return target;
}

LSTATUS VirtualRegistry::RegOpenKeyExW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const DWORD ulOptions,
    const REGSAM samDesired,
    const PHKEY phkResult
) noexcept
try
{
    const auto target = Resolve(hKey, View(lpSubKey), samDesired);
    if (target.isBase)
        return m_pBase->RegOpenKeyExW(
            hKey,
            lpSubKey,
            ulOptions,
            samDesired,
            phkResult
        );

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;

    *phkResult = nullptr;
    if (!target.pNode)
        return target.status;

    if (!samDesired)
        return ERROR_ACCESS_DENIED;

    OpenResult result;
    const auto status = target.pHive->OpenKey(
        target.pNode,
        target.rest,
        samDesired,
        result
    );
    if (status != ERROR_SUCCESS)
        return status;

    *phkResult = result.hLower ? result.hLower : Hive::HandleOf(result.pNode);
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegCreateKeyExW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const DWORD Reserved,
    const LPWSTR lpClass,
    const DWORD dwOptions,
    const REGSAM samDesired,
    const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    const PHKEY phkResult,
    const LPDWORD lpdwDisposition
) noexcept
try
{
    const auto target = Resolve(hKey, View(lpSubKey), samDesired);
    if (target.isBase)
    {
        return m_pBase->RegCreateKeyExW(
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
    }

    if (!phkResult)
        return ERROR_BADKEY;

    // Native leaves the result alone for these two and nulls it for every other failure.
    if (Reserved)
        return ERROR_INVALID_PARAMETER;
    if (target.status == ERROR_INVALID_HANDLE)
        return target.status;

    *phkResult = nullptr;
    if (!target.pNode)
        return target.status;

    constexpr DWORD KNOWN_OPTIONS = 0x1F;
    if (dwOptions & ~KNOWN_OPTIONS)
        return ERROR_INVALID_PARAMETER;

    OpenResult result;
    DWORD dwDisposition = REG_OPENED_EXISTING_KEY;
    const auto status = target.pWritable
                            ? target.pWritable->CreateKey(
                                target.pNode,
                                target.rest,
                                View(lpClass),
                                dwOptions,
                                samDesired,
                                result,
                                dwDisposition
                            )
                            : target.pHive->OpenKey(
                                target.pNode,
                                target.rest,
                                samDesired,
                                result
                            );
    if (status == ERROR_FILE_NOT_FOUND && !target.pWritable)
        return ERROR_ACCESS_DENIED;
    if (status != ERROR_SUCCESS)
        return status;

    *phkResult = result.hLower ? result.hLower : Hive::HandleOf(result.pNode);
    if (lpdwDisposition)
        *lpdwDisposition = dwDisposition;
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegCloseKey(
    const HKEY hKey
) noexcept
{
    if (Hive::NodeOf(hKey) || IsPredefinedHKey(hKey))
        return ERROR_SUCCESS;

    if (HandleSpace::Process().Owns(hKey) || !m_pBase)
        return ERROR_INVALID_HANDLE;

    return m_pBase->RegCloseKey(
        hKey
    );
}

LSTATUS VirtualRegistry::RegQueryValueExW(
    const HKEY hKey,
    const LPCWSTR lpValueName,
    const LPDWORD lpReserved,
    const LPDWORD lpType,
    const LPBYTE lpData,
    const LPDWORD lpcbData
) const noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.isBase)
        return m_pBase->RegQueryValueExW(
            hKey,
            lpValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData
        );

    if ((lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;
    if (!target.pNode)
        return target.status;

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    DWORD dwType{};
    DWORD cbValue = 0;
    std::vector<std::byte> value;
    const auto status = target.pHive->QueryValue(
        target.pNode,
        View(lpValueName),
        dwType,
        cbValue,
        lpData ? &value : nullptr
    );
    if (status != ERROR_SUCCESS)
        return status;

    if (lpType)
        *lpType = dwType;

    if (!lpData)
    {
        if (lpcbData)
            *lpcbData = cbValue;
        return ERROR_SUCCESS;
    }

    const DWORD cbBuffer = *lpcbData;
    *lpcbData = cbValue;
    if (cbValue > cbBuffer)
        return ERROR_MORE_DATA;

    CopyValue(lpData, cbBuffer, dwType, value);
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegSetValueExW(
    const HKEY hKey,
    const LPCWSTR lpValueName,
    const DWORD Reserved,
    const DWORD dwType,
    const BYTE* const lpData,
    const DWORD cbData
) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.isBase)
        return m_pBase->RegSetValueExW(
            hKey,
            lpValueName,
            Reserved,
            dwType,
            lpData,
            cbData
        );

    if (Reserved)
        return ERROR_INVALID_PARAMETER;
    if (!target.pNode)
        return target.status;

    if (!lpData && cbData)
        return ERROR_NOACCESS;

    if (!target.pWritable)
        return ERROR_ACCESS_DENIED;

    return target.pWritable->SetValue(
        target.pNode,
        View(lpValueName),
        dwType,
        AsBytes(lpData, cbData)
    );
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegDeleteValueW(
    const HKEY hKey,
    const LPCWSTR lpValueName
) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.isBase)
        return m_pBase->RegDeleteValueW(
            hKey,
            lpValueName
        );

    if (!target.pNode)
        return target.status;

    if (!target.pWritable)
        return ERROR_ACCESS_DENIED;

    return target.pWritable->DeleteValue(
        target.pNode,
        View(lpValueName)
    );
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegDeleteKeyExW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const REGSAM samDesired,
    const DWORD Reserved
) noexcept
try
{
    const auto target = Resolve(hKey, View(lpSubKey), samDesired);
    if (target.isBase)
        return m_pBase->RegDeleteKeyExW(
            hKey,
            lpSubKey,
            samDesired,
            Reserved
        );

    if (Reserved)
        return ERROR_INVALID_PARAMETER;
    if (!target.pNode)
        return target.status;

    if (!target.pWritable)
        return ERROR_ACCESS_DENIED;

    return target.pWritable->DeleteKey(target.pNode, target.rest);
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegDeleteTreeW(
    const HKEY hKey,
    const LPCWSTR lpSubKey
) noexcept
try
{
    const auto target = Resolve(hKey, View(lpSubKey));
    if (target.isBase)
        return m_pBase->RegDeleteTreeW(
            hKey,
            lpSubKey
        );

    if (!target.pNode)
        return target.status;

    const auto* const pHive = target.pHive;
    OpenResult result;
    const auto opened = pHive->OpenKey(
        target.pNode,
        target.rest,
        KEY_ALL_ACCESS,
        result
    );
    if (opened != ERROR_SUCCESS)
        return opened;

    // Only a frozen hive hands out lower keys, and it refuses every write.
    if (result.hLower)
    {
        pHive->Lower()->RegCloseKey(
            result.hLower
        );
        return ERROR_ACCESS_DENIED;
    }

    if (!target.pWritable)
        return ERROR_ACCESS_DENIED;

    const auto status = target.pWritable->DeleteTree(result.pNode);
    if (status != ERROR_SUCCESS || target.rest.empty())
        return status;

    return target.pWritable->DeleteKey(target.pNode, target.rest);
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegEnumKeyExW(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPWSTR lpName,
    const LPDWORD lpcchName,
    const LPDWORD lpReserved,
    const LPWSTR lpClass,
    const LPDWORD lpcchClass,
    const PFILETIME lpftLastWriteTime
) const noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.isBase)
    {
        return m_pBase
            ->RegEnumKeyExW(
                hKey,
                dwIndex,
                lpName,
                lpcchName,
                lpReserved,
                lpClass,
                lpcchClass,
                lpftLastWriteTime
            );
    }

    if (lpReserved || !lpcchName || (lpClass && !lpcchClass))
        return ERROR_INVALID_PARAMETER;
    if (!target.pNode)
        return target.status;

    const DWORD cchClassBuffer = lpClass ? *lpcchClass : 0;
    if (lpcchClass)
        *lpcchClass = 0;

    std::wstring name;
    std::wstring className;
    FILETIME lastWrite{};
    const auto status = target.pHive->EnumKey(
        target.pNode,
        dwIndex,
        name,
        className,
        lastWrite
    );
    if (status != ERROR_SUCCESS)
        return status;

    if (lpftLastWriteTime)
        *lpftLastWriteTime = lastWrite;

    const bool isNameFit = lpName && name.size() < *lpcchName;
    const bool isClassFit = !lpClass || className.size() < cchClassBuffer;
    if (!isNameFit || !isClassFit)
        return ERROR_MORE_DATA;

    std::memcpy(lpName, name.c_str(), (name.size() + 1) * sizeof(wchar_t));
    *lpcchName = static_cast<DWORD>(name.size());
    if (lpClass)
    {
        std::memcpy(
            lpClass,
            className.c_str(),
            (className.size() + 1) * sizeof(wchar_t)
        );
        *lpcchClass = static_cast<DWORD>(className.size());
    }

    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegEnumValueW(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPWSTR lpValueName,
    const LPDWORD lpcchValueName,
    const LPDWORD lpReserved,
    const LPDWORD lpType,
    const LPBYTE lpData,
    const LPDWORD lpcbData
) const noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.isBase)
    {
        return m_pBase
            ->RegEnumValueW(
                hKey,
                dwIndex,
                lpValueName,
                lpcchValueName,
                lpReserved,
                lpType,
                lpData,
                lpcbData
            );
    }

    if (lpReserved || !lpValueName || !lpcchValueName || (lpData && !lpcbData))
        return ERROR_INVALID_PARAMETER;
    if (!target.pNode)
        return target.status;

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    std::wstring name;
    DWORD dwType{};
    DWORD cbValue = 0;
    std::vector<std::byte> value;
    const auto status = target.pHive->EnumValue(
        target.pNode,
        dwIndex,
        name,
        dwType,
        cbValue,
        lpData ? &value : nullptr
    );
    if (status != ERROR_SUCCESS)
        return status;

    if (lpType)
        *lpType = dwType;

    bool isDataFit = true;
    DWORD cbDataBuffer = 0;
    if (lpcbData)
    {
        cbDataBuffer = *lpcbData;
        isDataFit = !lpData || cbValue <= cbDataBuffer;
        *lpcbData = cbValue;
    }

    const bool isNameFit = name.size() < *lpcchValueName;
    if (isNameFit)
    {
        std::memcpy(
            lpValueName,
            name.c_str(),
            (name.size() + 1) * sizeof(wchar_t)
        );
        *lpcchValueName = static_cast<DWORD>(name.size());
    }

    if (!isNameFit || !isDataFit)
        return ERROR_MORE_DATA;

    if (lpData)
        CopyValue(lpData, cbDataBuffer, dwType, value);
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegQueryInfoKeyW(
    const HKEY hKey,
    const LPWSTR lpClass,
    const LPDWORD lpcchClass,
    const LPDWORD lpReserved,
    const LPDWORD lpcSubKeys,
    const LPDWORD lpcbMaxSubKeyLen,
    const LPDWORD lpcbMaxClassLen,
    const LPDWORD lpcValues,
    const LPDWORD lpcbMaxValueNameLen,
    const LPDWORD lpcbMaxValueLen,
    const LPDWORD lpcbSecurityDescriptor,
    const PFILETIME lpftLastWriteTime
) const noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.isBase)
    {
        return m_pBase->RegQueryInfoKeyW(
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
    }

    if (lpReserved || (lpClass && !lpcchClass))
        return ERROR_INVALID_PARAMETER;
    if (!target.pNode)
        return target.status;

    KeyInfo info;
    const auto status = target.pHive->QueryInfo(
        target.pNode,
        info
    );
    if (status != ERROR_SUCCESS)
        return status;

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

    const auto cchClass = static_cast<DWORD>(info.className.size());
    if (lpClass && *lpcchClass != 0)
    {
        if (*lpcchClass < cchClass)
        {
            *lpcchClass = cchClass;
            return ERROR_MORE_DATA;
        }
        if (*lpcchClass == cchClass)
        {
            *lpcchClass = cchClass - 1;
            return ERROR_INSUFFICIENT_BUFFER;
        }
        std::memcpy(
            lpClass,
            info.className.c_str(),
            (info.className.size() + 1) * sizeof(wchar_t)
        );
    }

    if (lpClass && lpcchClass && (*lpcchClass != 0 || lpcbMaxClassLen))
        *lpcchClass = cchClass;
    else if (!lpClass && lpcchClass)
        *lpcchClass = 0;
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegLoadMUIStringW(
    const HKEY hKey,
    const LPCWSTR pszValue,
    const LPWSTR pszOutBuf,
    const DWORD cbOutBuf,
    const LPDWORD pcbData,
    const DWORD Flags,
    const LPCWSTR pszDirectory
) const noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.isBase)
        return m_pBase->RegLoadMUIStringW(
            hKey,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );

    if (!hKey)
        return ERROR_INVALID_PARAMETER;
    if (!target.pNode)
        return target.status;

    if (
        Flags & ~static_cast<DWORD>(REG_MUI_STRING_TRUNCATE)
        || ((Flags & REG_MUI_STRING_TRUNCATE) != 0) != !pcbData
        || cbOutBuf % sizeof(wchar_t) != 0
        || (!pszOutBuf && cbOutBuf != 0)
    )
        return ERROR_INVALID_PARAMETER;

    const auto* const pHive = target.pHive;
    HKEY hLower{};
    const auto opened = pHive->OpenLowerValueKey(
        target.pNode,
        View(pszValue),
        hLower
    );
    if (opened != ERROR_SUCCESS)
        return opened;

    // A value from below is resolved there, as the system would. One the hive
    // holds would need a resource module to resolve, unless it is marked as
    // the text itself.
    if (hLower)
    {
        auto* const pLower = pHive->Lower();
        const auto status = pLower->RegLoadMUIStringW(
            hLower,
            pszValue,
            pszOutBuf,
            cbOutBuf,
            pcbData,
            Flags,
            pszDirectory
        );
        pLower->RegCloseKey(
            hLower
        );
        return status;
    }

    std::wstring text;
    const auto status = pHive->QueryMuiText(
        target.pNode,
        View(pszValue),
        text
    );
    if (status != ERROR_SUCCESS)
        return status;

    // Truncation fills whatever buffer there is, terminated; otherwise the
    // size is reported and the text copied only when all of it fits.
    const auto cbText = static_cast<DWORD>((text.size() + 1) * sizeof(wchar_t));
    if (!pcbData)
    {
        if (!pszOutBuf)
            return ERROR_MORE_DATA;
        if (cbOutBuf == 0)
            return ERROR_SUCCESS;

        const auto cch = std::min<std::size_t>(
            text.size(),
            cbOutBuf / sizeof(wchar_t) - 1
        );
        std::memcpy(pszOutBuf, text.data(), cch * sizeof(wchar_t));
        pszOutBuf[cch] = L'\0';
        return ERROR_SUCCESS;
    }

    *pcbData = cbText;
    if (!pszOutBuf || cbOutBuf < cbText)
        return ERROR_MORE_DATA;

    std::memcpy(pszOutBuf, text.c_str(), cbText);
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegGetValueW(
    const HKEY hkey,
    const LPCWSTR lpSubKey,
    const LPCWSTR lpValue,
    const DWORD dwFlags,
    const LPDWORD pdwType,
    const PVOID pvData,
    const LPDWORD pcbData
) const noexcept
try
{
    const auto target = Resolve(
        hkey,
        View(lpSubKey),
        (dwFlags & RRF_SUBKEY_WOW6464KEY ? KEY_WOW64_64KEY : 0)
        | (dwFlags & RRF_SUBKEY_WOW6432KEY ? KEY_WOW64_32KEY : 0)
    );
    if (target.isBase)
        return m_pBase->RegGetValueW(
            hkey,
            lpSubKey,
            lpValue,
            dwFlags,
            pdwType,
            pvData,
            pcbData
        );

    constexpr DWORD BOTH_VIEWS = RRF_SUBKEY_WOW6464KEY | RRF_SUBKEY_WOW6432KEY;
    if (!hkey || (pvData && !pcbData) || !(dwFlags & RRF_RT_ANY) || (dwFlags & BOTH_VIEWS) == BOTH_VIEWS)
        return ERROR_INVALID_PARAMETER;

    const DWORD cbBuffer = pcbData ? *pcbData : 0;
    const auto fail = [&](const LSTATUS status)
    {
        if (pvData && (dwFlags & RRF_ZEROONFAILURE))
            std::memset(pvData, 0, cbBuffer);
        return status;
    };

    if (!target.pNode)
        return fail(target.status);

    const auto* const pHive = target.pHive;
    auto* pNode = target.pNode;
    if (!target.rest.empty())
    {
        OpenResult result;
        const auto opened = pHive->OpenKey(
            target.pNode,
            target.rest,
            KEY_QUERY_VALUE,
            result
        );
        if (opened != ERROR_SUCCESS)
            return fail(opened);

        if (result.hLower)
        {
            auto* const pLower = pHive->Lower();
            const auto status = pLower->RegGetValueW(
                result.hLower,
                nullptr,
                lpValue,
                dwFlags,
                pdwType,
                pvData,
                pcbData
            );
            pLower->RegCloseKey(
                result.hLower
            );
            return status;
        }

        pNode = result.pNode;
    }

    if (pdwType)
        *pdwType = 0;
    if (pcbData && !pvData)
        *pcbData = 0;

    DWORD dwType{};
    DWORD cbQueried = 0;
    std::vector<std::byte> value;
    const auto queried = pHive->QueryValue(
        pNode,
        View(lpValue),
        dwType,
        cbQueried,
        &value
    );
    if (queried != ERROR_SUCCESS)
        return fail(queried);

    const bool isString = IsStringType(dwType);

    // Native copies the stored bytes first, then the string with the terminators it lacks, then the expansion, each
    // over the previous one and only if it fits.
    std::vector<std::byte> stages[3];
    stages[0] = value;
    DWORD cbReported = static_cast<DWORD>(value.size());
    if (isString)
    {
        value.resize(value.size() / sizeof(wchar_t) * sizeof(wchar_t));
        const auto isNulAt = [&](const std::size_t iwch)
        {
            wchar_t ch;
            std::memcpy(&ch, value.data() + iwch * sizeof(wchar_t), sizeof(ch));
            return ch == wchar_t{};
        };
        const auto cwch = value.size() / sizeof(wchar_t);
        std::size_t cwchAdded = 0;
        if (cwch == 0 || !isNulAt(cwch - 1))
            cwchAdded = dwType == REG_MULTI_SZ ? 2 : 1;
        else if (dwType == REG_MULTI_SZ && (cwch == 1 || !isNulAt(cwch - 2)))
            cwchAdded = 1;
        value.resize(value.size() + cwchAdded * sizeof(wchar_t), std::byte{0});
        stages[1] = value;
        cbReported = static_cast<DWORD>(value.size());

        if (dwType == REG_EXPAND_SZ && !(dwFlags & RRF_NOEXPAND))
        {
            const auto* const pszRaw = reinterpret_cast<const wchar_t*>(value.data());
            const DWORD cwchExpanded = ExpandEnvironmentStringsW(
                pszRaw,
                nullptr,
                0
            );
            stages[2].resize(static_cast<std::size_t>(cwchExpanded) * sizeof(wchar_t));
            ExpandEnvironmentStringsW(
                pszRaw,
                reinterpret_cast<wchar_t*>(stages[2].data()),
                cwchExpanded
            );
            const auto cbExpanded = static_cast<DWORD>(stages[2].size());
            cbReported = std::max(cbReported, cbExpanded);
            dwType = REG_SZ;
        }
    }

    constexpr DWORD TYPE_FLAGS[] = {
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
    const DWORD dwTypeFlag = dwType < std::size(TYPE_FLAGS) ? TYPE_FLAGS[dwType] : 0;
    const bool isAllowed = dwTypeFlag ? (dwFlags & dwTypeFlag) != 0 : (dwFlags & RRF_RT_ANY) == RRF_RT_ANY;

    const auto& final = stages[2].empty() ? (stages[1].empty() ? stages[0] : stages[1]) : stages[2];
    const bool isFit = pvData && final.size() <= cbBuffer;
    if (pvData)
    {
        for (const auto& stage : stages)
        {
            if (!stage.empty() && stage.size() <= cbBuffer)
                std::memcpy(pvData, stage.data(), stage.size());
        }

        // An expansion that does not fit leaves the unexpanded text with its first character cleared.
        if (!stages[2].empty() && !isFit && stages[1].size() <= cbBuffer)
            std::memset(pvData, 0, sizeof(wchar_t));
    }

    if (pdwType)
        *pdwType = dwType;
    if (pcbData)
    {
        // A string that cannot be returned reports room for the terminators it may lack.
        const DWORD cbTerminators = dwType == REG_MULTI_SZ ? 4 : (isString ? 2 : 0);
        const DWORD cbReserved = static_cast<DWORD>(stages[0].size()) + cbTerminators;
        *pcbData = !stages[2].empty() ? cbReported : (isFit ? cbReported : std::max(cbReported, cbReserved));
    }

    // Binary data passes as a DWORD only when it is empty or has that size. Agent's choice: native was measured
    // with sizes 0 and 5 only; rejecting every other size is a guess for 1 to 3 and 6 or more.
    const bool isSizeMismatch = dwType == REG_BINARY && (dwFlags & RRF_RT_ANY) == RRF_RT_DWORD
        && !stages[0].empty() && stages[0].size() != sizeof(DWORD);

    LSTATUS status = ERROR_SUCCESS;
    if (!isAllowed)
        status = ERROR_UNSUPPORTED_TYPE;
    else if (isSizeMismatch)
        status = ERROR_DATATYPE_MISMATCH;
    else if (pvData && !isFit)
        status = ERROR_MORE_DATA;
    return status == ERROR_SUCCESS ? status : fail(status);
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegNotifyChangeKeyValue(
    const HKEY hKey,
    const BOOL bWatchSubtree,
    const DWORD dwNotifyFilter,
    const HANDLE hEvent,
    const BOOL fAsynchronous
) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.isBase)
        return m_pBase->RegNotifyChangeKeyValue(
            hKey,
            bWatchSubtree,
            dwNotifyFilter,
            hEvent,
            fAsynchronous
        );

    if (!target.pNode)
        return target.status;

    constexpr DWORD KNOWN_FILTERS =
        REG_NOTIFY_CHANGE_NAME
        | REG_NOTIFY_CHANGE_ATTRIBUTES
        | REG_NOTIFY_CHANGE_LAST_SET
        | REG_NOTIFY_CHANGE_SECURITY
        | REG_NOTIFY_THREAD_AGNOSTIC;
    if ((dwNotifyFilter & ~KNOWN_FILTERS) || (fAsynchronous && !hEvent))
        return ERROR_INVALID_PARAMETER;

    const auto& hive = *target.pHive;
    if (fAsynchronous)
        return hive.Watch(
            target.pNode,
            bWatchSubtree != FALSE,
            dwNotifyFilter,
            hEvent
        );

    // A synchronous call waits for the change like the native one, on a private
    // event, so a watcher thread written for the native registry behaves the
    // same (and, as there, never returns on a key nothing changes).
    const HANDLE hOwn = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hOwn)
        return GetLastError();

    const auto status = hive.Watch(
        target.pNode,
        bWatchSubtree != FALSE,
        dwNotifyFilter,
        hOwn
    );
    if (status == ERROR_SUCCESS)
        WaitForSingleObject(hOwn, INFINITE);

    CloseHandle(hOwn);
    return status;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS VirtualRegistry::RegFlushKey(
    const HKEY hKey
) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.isBase)
        return m_pBase->RegFlushKey(
            hKey
        );

    if (!target.pNode)
        return target.status;

    return target.pHive->CheckKey(target.pNode);
}
catch (...)
{
    return StatusFromException();
}
} // namespace VReg
