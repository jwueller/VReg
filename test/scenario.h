#pragma once

// Helpers shared by the scenario files. Writing a scenario: fill out-buffers
// and out-sizes with `kFill` / `kFillSize` before each call so untouched
// outputs are recorded too; record every out-parameter; never record handle
// values or timestamps (they differ between runs), only whether they were
// written; close every handle opened, or the hive cannot unload.
//
// Record through RECORD, never by calling the recorder with the call inline:
// the order in which a call's arguments are evaluated is unspecified, and
// MSVC evaluates the out-sizes before the call that fills them.

#include "oracle.h"
#include "win32_api.h"

#include <VReg/native_registry.h>

#include <Windows.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

/// Runs @p call, then records its status and the values the remaining
/// arguments (out-sizes and buffers) hold after it. The recorder is the
/// variable named `record`.
#define RECORD(label, call, ...)                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        const LSTATUS vregStatus = (call);                                                                             \
        record(label, vregStatus, ##__VA_ARGS__);                                                                      \
    } while (false)

namespace VReg::Test
{
constexpr BYTE kFill = 0xCC;
constexpr DWORD kFillSize = 0xCCCCCCCC;

inline HKEY FillKey() noexcept
{
    return reinterpret_cast<HKEY>(static_cast<std::uintptr_t>(kFillSize));
}

/// 0 if untouched, 1 if null, 2 for any other handle.
inline std::uint64_t KeyState(const HKEY hKey) noexcept
{
    if (hKey == FillKey())
        return 0;
    return hKey == nullptr ? 1 : 2;
}

inline std::uint64_t Written(const FILETIME& ft) noexcept
{
    return ft.dwLowDateTime != kFillSize || ft.dwHighDateTime != kFillSize;
}

inline FILETIME FillTime() noexcept
{
    return FILETIME{kFillSize, kFillSize};
}

/// Offset of @p p in @p base, or all ones for a null pointer.
inline std::uint64_t Offset(const void* const p, const void* const base) noexcept
{
    if (p == nullptr)
        return ~std::uint64_t{};
    return static_cast<std::uint64_t>(static_cast<const BYTE*>(p) - static_cast<const BYTE*>(base));
}

inline std::string Narrow(const std::wstring_view text)
{
    std::string narrow;
    for (const auto ch : text)
        narrow.push_back(static_cast<char>(ch));
    return narrow;
}

template <class T> const BYTE* Raw(const T* p) noexcept
{
    return reinterpret_cast<const BYTE*>(p);
}

/// Closes @p hKey unless it is @p hParent (the pre-`Ex` open forms hand the
/// parent back for an empty subkey), recording which it was.
template <class Api>
void CloseUnlessParent(Api& api, Recorder& record, const std::string& label, const HKEY hKey, const HKEY hParent)
{
    if (hKey == hParent)
        return;
    RECORD(label + " close", api.CloseKey(hKey));
}

/// A stored value: what to put into the registry.
struct StoredValue
{
    std::wstring name;
    DWORD type;
    std::vector<BYTE> data;
};

/// Sets @p value. The bytes just past an unterminated string are set to a
/// nonzero guard: the native W function reads one wide character past the
/// data to decide whether the string is terminated, so what follows the
/// buffer would otherwise decide what is stored.
template <class Api> LSTATUS SetStored(Api& api, const HKEY hKey, const StoredValue& value)
{
    auto guarded = value.data;
    guarded.insert(guarded.end(), 4, BYTE{0x58});
    return api
        .SetValueExW(hKey, value.name.c_str(), 0, value.type, guarded.data(), static_cast<DWORD>(value.data.size()));
}

template <class T> std::vector<BYTE> BytesOf(const T* const p, const std::size_t count)
{
    const auto* const pb = reinterpret_cast<const BYTE*>(p);
    return std::vector<BYTE>(pb, pb + count * sizeof(T));
}

template <class Api>
LSTATUS QueryEx(Api& api, HKEY hKey, LPCWSTR pszName, LPDWORD pdwType, LPBYTE pData, LPDWORD pcbData)
{
    return api.QueryValueExW(hKey, pszName, nullptr, pdwType, pData, pcbData);
}

template <class Api>
LSTATUS QueryEx(Api& api, HKEY hKey, LPCSTR pszName, LPDWORD pdwType, LPBYTE pData, LPDWORD pcbData)
{
    return api.QueryValueExA(hKey, pszName, nullptr, pdwType, pData, pcbData);
}

/// Queries one value in every way the caller can vary the buffers: type
/// only, size only, existence only, a range of buffer sizes, no type
/// pointer, data without a size pointer.
template <class Api, class Char>
void QueryMatrix(Api& api, Recorder& record, const HKEY hKey, const Char* const pszName, const std::string& label)
{
    DWORD dwType = kFillSize;
    RECORD(label + " type only", QueryEx(api, hKey, pszName, &dwType, nullptr, nullptr), {dwType});

    dwType = kFillSize;
    DWORD cbData = kFillSize;
    RECORD(label + " size", QueryEx(api, hKey, pszName, &dwType, nullptr, &cbData), {dwType, cbData});

    cbData = kFillSize;
    RECORD(label + " size only", QueryEx(api, hKey, pszName, nullptr, nullptr, &cbData), {cbData});

    RECORD(label + " existence", QueryEx(api, hKey, pszName, nullptr, nullptr, nullptr));

    for (const DWORD cbBuffer : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 9u, 12u, 24u, 64u})
    {
        std::array<BYTE, 64> data;
        data.fill(kFill);
        dwType = kFillSize;
        cbData = cbBuffer;
        RECORD(
            label + std::format(" buffer {}", cbBuffer),
            QueryEx(api, hKey, pszName, &dwType, data.data(), &cbData),
            {dwType, cbData},
            {Bytes(data)}
        );
    }

    {
        std::array<BYTE, 64> data;
        data.fill(kFill);
        cbData = 64;
        RECORD(label + " no type", QueryEx(api, hKey, pszName, nullptr, data.data(), &cbData), {cbData}, {Bytes(data)});

        dwType = kFillSize;
        data.fill(kFill);
        RECORD(
            label + " data without size",
            QueryEx(api, hKey, pszName, &dwType, data.data(), nullptr),
            {dwType},
            {Bytes(data)}
        );
    }
}
} // namespace VReg::Test
