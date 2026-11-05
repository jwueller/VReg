#pragma once

#include <VReg/hive.h>

#include <Windows.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace VReg
{
struct Value
{
    std::wstring name;
    DWORD dwType = REG_NONE;
    std::vector<std::byte> data;

    /// Hides the lower value of this name; only in a hive with a lower
    /// registry.
    bool isTombstone = false;

    /// Stored by @ref Hive::SetMuiTextValue: a terminated @ref REG_SZ.
    bool isMuiText = false;
};

/// A pending change notification: fires once, then is gone.
struct ChangeWatch
{
    /// Our own duplicate of the caller's event; closed when the watch fires or
    /// the hive goes.
    HANDLE hEvent = nullptr;

    bool isSubtree = false;
    DWORD dwFilter = 0;
};

/// The flags below @ref isDeleted describe a node's relation to the lower
/// registry and stay clear in a hive without one.
struct Node
{
    Hive* pHive = nullptr;
    Node* pParent = nullptr;
    HKEY hKey = nullptr;
    std::wstring name;
    std::wstring className;
    FILETIME lastWrite{};
    bool isVolatile = false;
    bool isDeleted = false;

    /// The key exists because the lower key at its path does; the node is
    /// just the way to it. Writing to it makes it an upper key.
    bool isTransparent = false;

    /// Hides the lower key of this name; never handed out.
    bool isTombstone = false;

    /// Hides every lower subkey and value (a key deleted and created again,
    /// or emptied by @ref Registry::RegDeleteTreeW).
    bool isOpaque = false;

    /// The lower key at this path exists and its class shows through (the
    /// node was made from it, not created over it).
    bool isLowerBacked = false;

    /// Sorted by @ref CompareNames, the order the native registry enumerates
    /// subkeys in.
    std::vector<Node*> children;

    /// In creation order, as the native registry enumerates values.
    std::vector<Value> values;

    /// Guarded by the hive's write lock.
    std::vector<ChangeWatch> watches;
};

/// Ordinal comparison of uppercased names, the registry's own collation:
/// negative, zero or positive.
[[nodiscard]] int CompareNames(
    std::wstring_view left,
    std::wstring_view right
) noexcept;

/// Whether @p left sorts before @p right by @ref CompareNames.
[[nodiscard]] inline bool NameLess(
    const std::wstring_view left,
    const std::wstring_view right
) noexcept
{
    return CompareNames(left, right) < 0;
}

/// @p text as the data of a terminated @ref REG_SZ. Throws @ref std::bad_alloc.
[[nodiscard]] std::vector<std::byte> StringData(std::wstring_view text);

/// What @ref Hive::QueryMuiText answers for @p value. Throws
/// @ref std::bad_alloc.
LSTATUS MuiTextOf(const Value& value, std::wstring& text);
} // namespace VReg
