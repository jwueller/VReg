#include <VReg/owned_values.h>

#include "retry.h"

#include <algorithm>

namespace VReg
{
LSTATUS QueryValue(
    const Registry& registry,
    const HKEY hKey,
    const LPCWSTR lpValueName,
    const LPDWORD lpType,
    std::vector<std::byte>& data
) noexcept
{
    return Guard(
        [&]
        {
            DWORD dwType{};
            DWORD cbData = 0;
            const auto sized = registry.RegQueryValueExW(
                hKey,
                lpValueName,
                nullptr,
                &dwType,
                nullptr,
                &cbData
            );
            if (sized != ERROR_SUCCESS)
                return sized;

            for (;;)
            {
                data.resize(cbData);
                DWORD cbRead = cbData;
                const auto status = registry.RegQueryValueExW(
                    hKey,
                    lpValueName,
                    nullptr,
                    &dwType,
                    reinterpret_cast<LPBYTE>(data.data()),
                    &cbRead
                );
                if (status == ERROR_MORE_DATA)
                {
                    cbData = Grown(cbData, cbRead);
                    continue;
                }
                if (status == ERROR_SUCCESS)
                {
                    data.resize(cbRead);
                    if (lpType)
                        *lpType = dwType;
                }
                return status;
            }
        }
    );
}

LSTATUS GetValue(
    const Registry& registry,
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const LPCWSTR lpValue,
    const DWORD dwFlags,
    const LPDWORD pdwType,
    std::vector<std::byte>& data
) noexcept
{
    return Guard(
        [&]
        {
            DWORD dwType{};
            DWORD cbData = 0;
            const auto sized = registry.RegGetValueW(
                hKey,
                lpSubKey,
                lpValue,
                dwFlags,
                &dwType,
                nullptr,
                &cbData
            );
            if (sized != ERROR_SUCCESS)
                return sized;

            for (;;)
            {
                data.resize(cbData);
                DWORD cbRead = cbData;
                const auto status = registry.RegGetValueW(
                    hKey,
                    lpSubKey,
                    lpValue,
                    dwFlags,
                    &dwType,
                    data.data(),
                    &cbRead
                );
                if (status == ERROR_MORE_DATA)
                {
                    cbData = Grown(cbData, cbRead);
                    continue;
                }
                if (status == ERROR_SUCCESS)
                {
                    data.resize(cbRead);
                    if (pdwType)
                        *pdwType = dwType;
                }
                return status;
            }
        }
    );
}

LSTATUS GetString(
    const Registry& registry,
    const HKEY hKey,
    const LPCWSTR lpSubKey,
    const LPCWSTR lpValue,
    const DWORD dwFlags,
    std::wstring& text
) noexcept
{
    return Guard(
        [&]
        {
            const DWORD dwTypeFlags = RRF_RT_REG_SZ | ((dwFlags & RRF_NOEXPAND) ? RRF_RT_REG_EXPAND_SZ : 0);
            std::vector<std::byte> data;
            const auto status = GetValue(
                registry,
                hKey,
                lpSubKey,
                lpValue,
                (dwFlags & ~RRF_RT_ANY) | dwTypeFlags,
                nullptr,
                data
            );
            if (status != ERROR_SUCCESS)
                return status;

            std::size_t cch = data.size() / sizeof(wchar_t);
            const auto* const pch = reinterpret_cast<const wchar_t*>(data.data());
            if (cch > 0 && pch[cch - 1] == L'\0')
                --cch;
            text.assign(pch, cch);
            return static_cast<LSTATUS>(ERROR_SUCCESS);
        }
    );
}

LSTATUS EnumKey(
    const Registry& registry,
    const HKEY hKey,
    const DWORD dwIndex,
    std::wstring& name
) noexcept
{
    return Guard(
        [&]
        {
            DWORD cchName = INITIAL_NAME_CHARS;
            for (;;)
            {
                name.assign(cchName, L'\0');
                DWORD cchRead = cchName;
                const auto status = registry.RegEnumKeyExW(
                    hKey,
                    dwIndex,
                    name.data(),
                    &cchRead,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr
                );
                if (status == ERROR_MORE_DATA && cchName < MAX_CLASS_CHARS)
                {
                    cchName = std::min(Grown(cchName, cchRead), MAX_CLASS_CHARS);
                    continue;
                }
                if (status == ERROR_SUCCESS)
                    name.resize(cchRead);
                return status;
            }
        }
    );
}

LSTATUS EnumValue(
    const Registry& registry,
    const HKEY hKey,
    const DWORD dwIndex,
    std::wstring& name,
    const LPDWORD lpType,
    std::vector<std::byte>* const pData
) noexcept
{
    return Guard(
        [&]
        {
            DWORD cchName = INITIAL_NAME_CHARS;
            DWORD cbData = 0;
            bool isSized = !pData;
            for (;;)
            {
                name.assign(cchName, L'\0');
                DWORD cchRead = cchName;
                DWORD cbRead = cbData;
                DWORD dwType{};
                const auto status = registry.RegEnumValueW(
                    hKey,
                    dwIndex,
                    name.data(),
                    &cchRead,
                    nullptr,
                    &dwType,
                    (pData && isSized) ? reinterpret_cast<LPBYTE>(pData->data()) : nullptr,
                    pData ? &cbRead : nullptr
                );
                if (status == ERROR_MORE_DATA)
                {
                    cchName = std::min(
                        Grown(cchName, cchRead + 1),
                        MAX_VALUE_NAME_CHARS
                    );
                    if (pData)
                        cbData = Grown(cbData, cbRead);
                    if (pData && isSized)
                        pData->resize(cbData);
                    continue;
                }
                if (status == ERROR_SUCCESS && !isSized)
                {
                    cbData = cbRead;
                    pData->resize(cbData);
                    isSized = true;
                    continue;
                }
                if (status == ERROR_SUCCESS)
                {
                    name.resize(cchRead);
                    if (pData)
                        pData->resize(cbRead);
                    if (lpType)
                        *lpType = dwType;
                }
                return status;
            }
        }
    );
}

LSTATUS LoadMUIString(
    const Registry& registry,
    const HKEY hKey,
    const LPCWSTR pszValue,
    const DWORD Flags,
    const LPCWSTR pszDirectory,
    std::wstring& text
) noexcept
{
    return Guard(
        [&]
        {
            DWORD cbBuffer = 256 * sizeof(wchar_t);
            for (;;)
            {
                text.assign(cbBuffer / sizeof(wchar_t), L'\0');
                DWORD cbData = 0;
                const auto status = registry.RegLoadMUIStringW(
                    hKey,
                    pszValue,
                    text.data(),
                    cbBuffer,
                    &cbData,
                    Flags,
                    pszDirectory
                );
                if (status == ERROR_MORE_DATA)
                {
                    cbBuffer = Grown(cbBuffer, cbData);
                    continue;
                }
                if (status == ERROR_SUCCESS)
                {
                    std::size_t cch = cbData / sizeof(wchar_t);
                    if (cch > 0 && text[cch - 1] == L'\0')
                        --cch;
                    text.resize(cch);
                }
                return status;
            }
        }
    );
}
} // namespace VReg
