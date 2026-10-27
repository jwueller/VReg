#include <VReg/virtual_registry.h>

#include "handle_space.h"
#include "tree.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <iterator>
#include <span>

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
constexpr std::size_t kMaxKeyNameChars = 255;

/// Splits @p path at backslashes as the native registry does: empty
/// components are skipped, a trailing backslash is ignored, and a leading one
/// is a bad path name.
LSTATUS SplitPath(const std::wstring_view path, std::vector<std::wstring_view>& components)
{
    components.clear();
    if (!path.empty() && path.front() == L'\\')
        return ERROR_BAD_PATHNAME;

    std::size_t start = 0;
    while (start < path.size())
    {
        const auto end = std::min(path.find(L'\\', start), path.size());
        if (const auto component = path.substr(start, end - start); !component.empty())
        {
            if (component.size() > kMaxKeyNameChars)
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
    if (dwType != REG_SZ && dwType != REG_EXPAND_SZ && dwType != REG_MULTI_SZ)
        return;

    const auto cbWhole = value.size() / sizeof(wchar_t) * sizeof(wchar_t);
    if (cbWhole == 0)
        return;

    const bool isTerminated = value[cbWhole - 2] == std::byte{0} && value[cbWhole - 1] == std::byte{0};
    if (!isTerminated && cbWhole + sizeof(wchar_t) <= cbBuffer)
        std::memset(pbBuffer + cbWhole, 0, sizeof(wchar_t));
}

std::span<const std::byte> AsBytes(const BYTE* const pb, const DWORD cb) noexcept
{
    return {reinterpret_cast<const std::byte*>(pb), pb ? cb : 0};
}
} // namespace

VirtualRegistry::VirtualRegistry(Registry* const pBase) noexcept
    : m_pBase(pBase)
    , m_pMounts(nullptr)
{
}

LSTATUS VirtualRegistry::Mount(const HKEY hRoot, const std::wstring_view path, Hive& hive) noexcept
try
{
    if (!IsPredefinedHKey(hRoot))
        return ERROR_INVALID_HANDLE;

    std::vector<std::wstring_view> components;
    if (SplitPath(path, components) != ERROR_SUCCESS)
        return ERROR_BAD_PATHNAME;

    MountPoint mount{.hRoot = hRoot, .path = {components.begin(), components.end()}, .pHive = &hive};

    std::scoped_lock lock(m_mountMutex);
    const auto* const pOld = m_pMounts.load(std::memory_order_acquire);
    auto* const pNew = new MountTable(pOld ? *pOld : MountTable{});
    for (const auto& existing : pNew->mounts)
    {
        if (existing.hRoot == hRoot && std::ranges::equal(existing.path, mount.path, [](const auto& a, const auto& b) {
                return CompareNames(a, b) == 0;
            }))
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
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

const VirtualRegistry::MountTable& VirtualRegistry::Mounts() const noexcept
{
    static const MountTable s_empty;
    const auto* const pMounts = m_pMounts.load(std::memory_order_acquire);
    return pMounts ? *pMounts : s_empty;
}

bool VirtualRegistry::IsMounted(const Hive* const pHive) const noexcept
{
    return std::ranges::any_of(Mounts().mounts, [&](const MountPoint& mount) { return mount.pHive == pHive; });
}

VirtualRegistry::Target VirtualRegistry::Resolve(const HKEY hKey, const std::wstring_view subKey) const
{
    Target target;
    std::vector<std::wstring_view> components;
    const auto splitStatus = SplitPath(subKey, components);
    const bool isSplit = splitStatus == ERROR_SUCCESS;

    if (auto* const pNode = Hive::NodeOf(hKey))
    {
        if (IsMounted(Hive::HiveOf(pNode)))
        {
            if (isSplit)
            {
                target.pNode = pNode;
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
        const MountPoint* pBest = nullptr;
        for (const auto& mount : Mounts().mounts)
        {
            if (!isSplit || mount.hRoot != hKey || mount.path.size() > components.size())
                continue;

            if (!std::equal(mount.path.begin(), mount.path.end(), components.begin(), [](const auto& a, const auto& b) {
                    return CompareNames(a, b) == 0;
                }))
                continue;

            if (!pBest || mount.path.size() > pBest->path.size())
                pBest = &mount;
        }

        if (pBest)
        {
            target.pNode = pBest->pHive->Root();
            target.rest.assign(components.begin() + pBest->path.size(), components.end());
            return target;
        }

        if (m_pBase)
            target.hBase = hKey;
        else
            target.status = isSplit ? ERROR_FILE_NOT_FOUND : splitStatus;

        return target;
    }

    if (m_pBase)
        target.hBase = hKey;
    else
        target.status = ERROR_INVALID_HANDLE;

    return target;
}

LSTATUS VirtualRegistry::OpenKeyExW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const DWORD ulOptions,
    const REGSAM samDesired,
    const PHKEY phkResult
) noexcept
try
{
    const auto target = Resolve(hKey, View(lpSubKey));
    if (target.hBase)
        return m_pBase->OpenKeyExW(target.hBase, lpSubKey, ulOptions, samDesired, phkResult);

    if (!phkResult)
        return ERROR_INVALID_PARAMETER;

    *phkResult = nullptr;
    if (!target.pNode)
        return target.status;

    if (!samDesired)
        return ERROR_ACCESS_DENIED;

    OpenResult result;
    if (const auto status = Hive::HiveOf(target.pNode)->OpenKey(target.pNode, target.rest, samDesired, result);
        status != ERROR_SUCCESS)
        return status;

    *phkResult = result.hLower ? result.hLower : Hive::HandleOf(result.pNode);
    return ERROR_SUCCESS;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::CreateKeyExW(
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
    const auto target = Resolve(hKey, View(lpSubKey));
    if (target.hBase)
    {
        return m_pBase->CreateKeyExW(
            target.hBase,
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

    constexpr DWORD kKnownOptions = 0x1F;
    if (dwOptions & ~kKnownOptions)
        return ERROR_INVALID_PARAMETER;

    Node* pNode = nullptr;
    DWORD dwDisposition = 0;
    if (const auto status = Hive::HiveOf(target.pNode)
                                ->CreateKey(target.pNode, target.rest, View(lpClass), dwOptions, pNode, dwDisposition);
        status != ERROR_SUCCESS)
        return status;

    *phkResult = Hive::HandleOf(pNode);
    if (lpdwDisposition)
        *lpdwDisposition = dwDisposition;
    return ERROR_SUCCESS;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::CloseKey(const HKEY hKey) noexcept
{
    if (Hive::NodeOf(hKey) || IsPredefinedHKey(hKey))
        return ERROR_SUCCESS;

    if (HandleSpace::Process().Owns(hKey) || !m_pBase)
        return ERROR_INVALID_HANDLE;

    return m_pBase->CloseKey(hKey);
}

LSTATUS VirtualRegistry::QueryValueExW(
    const HKEY hKey,
    const LPCWSTR lpValueName,
    const LPDWORD lpReserved,
    const LPDWORD lpType,
    const LPBYTE lpData,
    const LPDWORD lpcbData
) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.hBase)
        return m_pBase->QueryValueExW(target.hBase, lpValueName, lpReserved, lpType, lpData, lpcbData);

    if ((lpData && !lpcbData) || lpReserved)
        return ERROR_INVALID_PARAMETER;
    if (!target.pNode)
        return target.status;

    if (lpType)
        *lpType = 0;
    if (lpcbData && !lpData)
        *lpcbData = 0;

    DWORD dwType{};
    std::vector<std::byte> value;
    if (const auto status = Hive::HiveOf(target.pNode)->QueryValue(target.pNode, View(lpValueName), dwType, value);
        status != ERROR_SUCCESS)
        return status;

    if (lpType)
        *lpType = dwType;

    const auto cbValue = static_cast<DWORD>(value.size());
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
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::SetValueExW(
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
    if (target.hBase)
        return m_pBase->SetValueExW(target.hBase, lpValueName, Reserved, dwType, lpData, cbData);

    if (Reserved)
        return ERROR_INVALID_PARAMETER;
    if (!target.pNode)
        return target.status;

    if (!lpData && cbData)
        return ERROR_NOACCESS;

    return Hive::HiveOf(target.pNode)->SetValue(target.pNode, View(lpValueName), dwType, AsBytes(lpData, cbData));
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::DeleteValueW(const HKEY hKey, const LPCWSTR lpValueName) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.hBase)
        return m_pBase->DeleteValueW(target.hBase, lpValueName);

    if (!target.pNode)
        return target.status;

    return Hive::HiveOf(target.pNode)->DeleteValue(target.pNode, View(lpValueName));
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::DeleteKeyExW(
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const REGSAM samDesired,
    const DWORD Reserved
) noexcept
try
{
    const auto target = Resolve(hKey, View(lpSubKey));
    if (target.hBase)
        return m_pBase->DeleteKeyExW(target.hBase, lpSubKey, samDesired, Reserved);

    if (Reserved)
        return ERROR_INVALID_PARAMETER;
    if (!target.pNode)
        return target.status;

    return Hive::HiveOf(target.pNode)->DeleteKey(target.pNode, target.rest);
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::DeleteTreeW(const HKEY hKey, const LPCWSTR lpSubKey) noexcept
try
{
    const auto target = Resolve(hKey, View(lpSubKey));
    if (target.hBase)
        return m_pBase->DeleteTreeW(target.hBase, lpSubKey);

    if (!target.pNode)
        return target.status;

    auto* const pHive = Hive::HiveOf(target.pNode);
    OpenResult result;
    if (const auto status = pHive->OpenKey(target.pNode, target.rest, KEY_ALL_ACCESS, result); status != ERROR_SUCCESS)
        return status;

    if (const auto status = pHive->DeleteTree(result.pNode); status != ERROR_SUCCESS || target.rest.empty())
        return status;

    return pHive->DeleteKey(target.pNode, target.rest);
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::EnumKeyExW(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPWSTR lpName,
    const LPDWORD lpcchName,
    const LPDWORD lpReserved,
    const LPWSTR lpClass,
    const LPDWORD lpcchClass,
    const PFILETIME lpftLastWriteTime
) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.hBase)
    {
        return m_pBase
            ->EnumKeyExW(target.hBase, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime);
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
    if (const auto status = Hive::HiveOf(target.pNode)->EnumKey(target.pNode, dwIndex, name, className, lastWrite);
        status != ERROR_SUCCESS)
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
        std::memcpy(lpClass, className.c_str(), (className.size() + 1) * sizeof(wchar_t));
        *lpcchClass = static_cast<DWORD>(className.size());
    }

    return ERROR_SUCCESS;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::EnumValueW(
    const HKEY hKey,
    const DWORD dwIndex,
    const LPWSTR lpValueName,
    const LPDWORD lpcchValueName,
    const LPDWORD lpReserved,
    const LPDWORD lpType,
    const LPBYTE lpData,
    const LPDWORD lpcbData
) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.hBase)
    {
        return m_pBase
            ->EnumValueW(target.hBase, dwIndex, lpValueName, lpcchValueName, lpReserved, lpType, lpData, lpcbData);
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
    std::vector<std::byte> value;
    if (const auto status = Hive::HiveOf(target.pNode)->EnumValue(target.pNode, dwIndex, name, dwType, value);
        status != ERROR_SUCCESS)
        return status;

    if (lpType)
        *lpType = dwType;

    const auto cbValue = static_cast<DWORD>(value.size());
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
        std::memcpy(lpValueName, name.c_str(), (name.size() + 1) * sizeof(wchar_t));
        *lpcchValueName = static_cast<DWORD>(name.size());
    }

    if (!isNameFit || !isDataFit)
        return ERROR_MORE_DATA;

    if (lpData)
        CopyValue(lpData, cbDataBuffer, dwType, value);
    return ERROR_SUCCESS;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::QueryInfoKeyW(
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
) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.hBase)
    {
        return m_pBase->QueryInfoKeyW(
            target.hBase,
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
    if (const auto status = Hive::HiveOf(target.pNode)->QueryInfo(target.pNode, info); status != ERROR_SUCCESS)
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
        std::memcpy(lpClass, info.className.c_str(), (info.className.size() + 1) * sizeof(wchar_t));
    }

    if (lpClass && lpcchClass && (*lpcchClass != 0 || lpcbMaxClassLen))
        *lpcchClass = cchClass;
    else if (!lpClass && lpcchClass)
        *lpcchClass = 0;
    return ERROR_SUCCESS;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::LoadMUIStringW(
    const HKEY hKey,
    const LPCWSTR pszValue,
    const LPWSTR pszOutBuf,
    const DWORD cbOutBuf,
    const LPDWORD pcbData,
    const DWORD Flags,
    const LPCWSTR pszDirectory
) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.hBase)
        return m_pBase->LoadMUIStringW(target.hBase, pszValue, pszOutBuf, cbOutBuf, pcbData, Flags, pszDirectory);

    if (!hKey)
        return ERROR_INVALID_PARAMETER;
    if (!target.pNode)
        return target.status;

    if ((Flags & ~static_cast<DWORD>(REG_MUI_STRING_TRUNCATE)) || ((Flags & REG_MUI_STRING_TRUNCATE) != 0) != !pcbData
        || (cbOutBuf % sizeof(wchar_t)) != 0)
        return ERROR_INVALID_PARAMETER;

    DWORD dwType{};
    std::vector<std::byte> value;
    if (const auto status = Hive::HiveOf(target.pNode)->QueryValue(target.pNode, View(pszValue), dwType, value);
        status != ERROR_SUCCESS)
        return status;

    return ERROR_INVALID_DATA;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::GetValueW(
    const HKEY hkey,
    const LPCWSTR lpSubKey,
    const LPCWSTR lpValue,
    const DWORD dwFlags,
    const LPDWORD pdwType,
    const PVOID pvData,
    const LPDWORD pcbData
) noexcept
try
{
    const auto target = Resolve(hkey, View(lpSubKey));
    if (target.hBase)
        return m_pBase->GetValueW(target.hBase, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);

    constexpr DWORD kBothViews = RRF_SUBKEY_WOW6464KEY | RRF_SUBKEY_WOW6432KEY;
    if (!hkey || (pvData && !pcbData) || !(dwFlags & RRF_RT_ANY) || (dwFlags & kBothViews) == kBothViews)
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

    auto* const pHive = Hive::HiveOf(target.pNode);
    auto* pNode = target.pNode;
    if (!target.rest.empty())
    {
        OpenResult result;
        if (const auto status = pHive->OpenKey(target.pNode, target.rest, KEY_QUERY_VALUE, result);
            status != ERROR_SUCCESS)
            return fail(status);

        pNode = result.pNode;
    }

    if (pdwType)
        *pdwType = 0;
    if (pcbData && !pvData)
        *pcbData = 0;

    DWORD dwType{};
    std::vector<std::byte> value;
    if (const auto status = pHive->QueryValue(pNode, View(lpValue), dwType, value); status != ERROR_SUCCESS)
        return fail(status);

    const bool isString = dwType == REG_SZ || dwType == REG_EXPAND_SZ || dwType == REG_MULTI_SZ;

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
            const DWORD cwchExpanded = ExpandEnvironmentStringsW(pszRaw, nullptr, 0);
            stages[2].resize(static_cast<std::size_t>(cwchExpanded) * sizeof(wchar_t));
            ExpandEnvironmentStringsW(pszRaw, reinterpret_cast<wchar_t*>(stages[2].data()), cwchExpanded);
            const auto cbExpanded = static_cast<DWORD>(stages[2].size());
            cbReported = std::max(cbReported, cbExpanded);
            dwType = REG_SZ;
        }
    }

    constexpr DWORD kTypeFlags[] = {
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
    const DWORD dwTypeFlag = dwType < std::size(kTypeFlags) ? kTypeFlags[dwType] : 0;
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
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::NotifyChangeKeyValue(
    const HKEY hKey,
    const BOOL bWatchSubtree,
    const DWORD dwNotifyFilter,
    const HANDLE hEvent,
    const BOOL fAsynchronous
) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.hBase)
        return m_pBase->NotifyChangeKeyValue(target.hBase, bWatchSubtree, dwNotifyFilter, hEvent, fAsynchronous);

    if (!target.pNode)
        return target.status;

    constexpr DWORD kKnownFilters = REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES | REG_NOTIFY_CHANGE_LAST_SET
                                    | REG_NOTIFY_CHANGE_SECURITY | REG_NOTIFY_THREAD_AGNOSTIC;
    if ((dwNotifyFilter & ~kKnownFilters) || (fAsynchronous && !hEvent))
        return ERROR_INVALID_PARAMETER;

    return Hive::HiveOf(target.pNode)->CheckKey(target.pNode);
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS VirtualRegistry::FlushKey(const HKEY hKey) noexcept
try
{
    const auto target = Resolve(hKey, {});
    if (target.hBase)
        return m_pBase->FlushKey(target.hBase);

    if (!target.pNode)
        return target.status;

    return Hive::HiveOf(target.pNode)->CheckKey(target.pNode);
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}
} // namespace VReg
