#include <VReg/regfile.h>

#include <limits>
#include <new>

namespace VReg::RegFile
{
namespace
{
/// Closes a key opened through a registry on scope exit.
class KeyHandle
{
public:
    KeyHandle(Registry& registry) noexcept :
        m_registry(registry)
    {
    }

    ~KeyHandle()
    {
        if (m_hKey)
            m_registry.RegCloseKey(
                m_hKey
            );
    }

    KeyHandle(const KeyHandle&) = delete;
    KeyHandle& operator=(const KeyHandle&) = delete;

    PHKEY Receive() noexcept
    {
        return &m_hKey;
    }

    HKEY Get() const noexcept
    {
        return m_hKey;
    }

private:
    Registry& m_registry;
    HKEY m_hKey{};
};

LSTATUS NotFoundIsFine(const LSTATUS status) noexcept
{
    return status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND
               ? ERROR_SUCCESS
               : status;
}

LSTATUS Perform(
    const Operation& operation,
    Registry& registry
) noexcept
{
    // A script can be built by hand, so the parser's guarantees are checked again: a NUL would cut a path or name
    // short, and an empty path would delete a whole root.
    if (operation.path.find(L'\0') != std::wstring::npos || operation.name.find(L'\0') != std::wstring::npos)
        return ERROR_INVALID_PARAMETER;
    if (operation.kind == OperationKind::DeleteKey && operation.path.empty())
        return ERROR_INVALID_PARAMETER;

    const wchar_t* const pszPath = operation.path.c_str();
    switch (operation.kind)
    {
    case OperationKind::CreateKey:
    {
        KeyHandle key(registry);
        return registry.RegCreateKeyExW(
            operation.hRoot,
            pszPath,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_CREATE_SUB_KEY,
            nullptr,
            key.Receive(),
            nullptr
        );
    }
    case OperationKind::DeleteKey:
    {
        const auto status = NotFoundIsFine(
            registry.RegDeleteTreeW(
                operation.hRoot,
                pszPath
            )
        );
        if (status != ERROR_SUCCESS)
            return status;
        return NotFoundIsFine(
            registry.RegDeleteKeyExW(
                operation.hRoot,
                pszPath,
                0,
                0
            )
        );
    }
    case OperationKind::SetValue:
    {
        if (operation.data.size() > std::numeric_limits<DWORD>::max())
            return ERROR_INVALID_PARAMETER;
        KeyHandle key(registry);
        const auto status = registry.RegOpenKeyExW(
            operation.hRoot,
            pszPath,
            0,
            KEY_SET_VALUE,
            key.Receive()
        );
        if (status != ERROR_SUCCESS)
            return status;
        // Native reads past unterminated string data for a terminator and may store one more wchar if it
        // finds a zero there, so such data goes in with nonzero bytes after it to store exactly what the
        // file says.
        const auto& data = operation.data;
        const bool isString = operation.dwType == REG_SZ
            || operation.dwType == REG_EXPAND_SZ
            || operation.dwType == REG_MULTI_SZ;
        const bool isTerminated = data.size() >= sizeof(wchar_t)
            && data[data.size() - 1] == std::byte{0}
            && data[data.size() - 2] == std::byte{0};
        if (isString && !isTerminated)
        {
            std::vector<std::byte> padded;
            try
            {
                padded = data;
                padded.insert(padded.end(), 4, std::byte{0x11});
            }
            catch (const std::bad_alloc&)
            {
                return ERROR_OUTOFMEMORY;
            }
            return registry.RegSetValueExW(
                key.Get(),
                operation.name.c_str(),
                0,
                operation.dwType,
                reinterpret_cast<const BYTE*>(padded.data()),
                static_cast<DWORD>(data.size())
            );
        }
        return registry.RegSetValueExW(
            key.Get(),
            operation.name.c_str(),
            0,
            operation.dwType,
            reinterpret_cast<const BYTE*>(data.data()),
            static_cast<DWORD>(data.size())
        );
    }
    case OperationKind::DeleteValue:
    {
        KeyHandle key(registry);
        const auto status = registry.RegOpenKeyExW(
            operation.hRoot,
            pszPath,
            0,
            KEY_SET_VALUE,
            key.Receive()
        );
        if (status != ERROR_SUCCESS)
            return NotFoundIsFine(status);
        return NotFoundIsFine(
            registry.RegDeleteValueW(
                key.Get(),
                operation.name.c_str()
            )
        );
    }
    }
    return ERROR_INVALID_PARAMETER;
}
} // namespace

LSTATUS Apply(
    const Script& script,
    Registry& registry,
    std::size_t* const pFailed
) noexcept
{
    for (std::size_t i = 0; i < script.operations.size(); ++i)
    {
        const auto status = Perform(script.operations[i], registry);
        if (status != ERROR_SUCCESS)
        {
            if (pFailed)
                *pFailed = i;
            return status;
        }
    }
    return ERROR_SUCCESS;
}
} // namespace VReg::RegFile
