#include <VReg/regfile.h>

#include "text.h"
#include "writer.h"

#include <algorithm>
#include <new>

namespace VReg::RegFile
{
namespace
{
const wchar_t* RootName(const HKEY hRoot) noexcept
{
    if (hRoot == HKEY_CLASSES_ROOT)
        return L"HKEY_CLASSES_ROOT";
    if (hRoot == HKEY_CURRENT_USER)
        return L"HKEY_CURRENT_USER";
    if (hRoot == HKEY_LOCAL_MACHINE)
        return L"HKEY_LOCAL_MACHINE";
    if (hRoot == HKEY_USERS)
        return L"HKEY_USERS";
    if (hRoot == HKEY_CURRENT_CONFIG)
        return L"HKEY_CURRENT_CONFIG";
    return nullptr;
}

bool NameLess(const std::wstring& a, const std::wstring& b) noexcept
{
    return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) ==
           CSTR_LESS_THAN;
}

class SnapshotWalker
{
public:
    explicit SnapshotWalker(Registry& registry) noexcept
        : m_registry(registry)
    {
    }

    /// Appends the key @p hKey, named @p fullName, then its values, then its subkeys in order.
    LSTATUS Walk(const HKEY hKey, const std::wstring& fullName, const unsigned depth = 0)
    {
        // The registry nests at most 512 levels; anything deeper is a cycle through some other registry.
        constexpr unsigned kMaxDepth = 512;
        if (depth > kMaxDepth)
            return ERROR_CANTREAD;

        m_records.push_back({Record::Kind::Key, fullName, {}, 0, {}});

        std::vector<Record> values;
        for (DWORD dwIndex = 0;; ++dwIndex)
        {
            Record value{Record::Kind::Value, {}, {}, 0, {}};
            const auto status = m_registry.EnumValue(hKey, dwIndex, value.name, &value.dwType, &value.data);
            if (status == ERROR_NO_MORE_ITEMS)
                break;
            if (status != ERROR_SUCCESS)
                return status;
            values.push_back(std::move(value));
        }
        std::ranges::sort(values, NameLess, &Record::name);
        for (auto& value : values)
            m_records.push_back(std::move(value));

        std::vector<std::wstring> subkeys;
        for (DWORD dwIndex = 0;; ++dwIndex)
        {
            std::wstring name;
            const auto status = m_registry.EnumKey(hKey, dwIndex, name);
            if (status == ERROR_NO_MORE_ITEMS)
                break;
            if (status != ERROR_SUCCESS)
                return status;
            subkeys.push_back(std::move(name));
        }
        std::ranges::sort(subkeys, NameLess);
        for (const auto& subkey : subkeys)
        {
            HKEY hSubkey{};
            // A symbolic link is exported as the key it is (its SymbolicLinkValue), never followed: following one
            // can leave the subtree or loop.
            if (const auto status = m_registry.OpenKeyExW(hKey, subkey.c_str(), REG_OPTION_OPEN_LINK, KEY_READ, &hSubkey);
                status != ERROR_SUCCESS)
                return status;
            const auto status = Walk(hSubkey, fullName + L'\\' + subkey, depth + 1);
            m_registry.CloseKey(hSubkey);
            if (status != ERROR_SUCCESS)
                return status;
        }
        return ERROR_SUCCESS;
    }

    std::vector<Record>& Records() noexcept
    {
        return m_records;
    }

private:
    Registry& m_registry;
    std::vector<Record> m_records;
};
} // namespace

LSTATUS ExportSnapshot(
    Registry& registry,
    const HKEY hRoot,
    const std::wstring_view path,
    const Format format,
    std::vector<std::byte>& file
) noexcept
{
    const wchar_t* const pszRoot = RootName(hRoot);
    if (!pszRoot)
        return ERROR_INVALID_HANDLE;

    try
    {
        const std::wstring subkey(path);
        HKEY hKey{};
        if (const auto status = registry.OpenKeyExW(hRoot, subkey.c_str(), 0, KEY_READ, &hKey); status != ERROR_SUCCESS)
            return status;

        std::wstring fullName = pszRoot;
        if (!path.empty())
            fullName.append(L"\\").append(path);
        SnapshotWalker walker(registry);
        const auto walked = walker.Walk(hKey, fullName);
        registry.CloseKey(hKey);
        if (walked != ERROR_SUCCESS)
            return walked;
        std::vector<std::byte> bytes;
        if (const auto status = WriteRecords(walker.Records(), format, bytes); status != ERROR_SUCCESS)
            return status;
        file = std::move(bytes);
        return ERROR_SUCCESS;
    }
    catch (const std::bad_alloc&)
    {
        return ERROR_OUTOFMEMORY;
    }
}

LSTATUS ExportSnapshotToFile(
    Registry& registry,
    const HKEY hRoot,
    const std::wstring_view path,
    const Format format,
    const std::filesystem::path& fileName
) noexcept
{
    try
    {
        std::vector<std::byte> file;
        if (const auto status = ExportSnapshot(registry, hRoot, path, format, file); status != ERROR_SUCCESS)
            return status;
        return WriteWholeFile(fileName, file);
    }
    catch (const std::bad_alloc&)
    {
        return ERROR_OUTOFMEMORY;
    }
}
} // namespace VReg::RegFile
