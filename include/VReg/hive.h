#pragma once

#include <Windows.h>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace VReg
{
class Registry;

/// A key in a hive. Opaque outside the hive that owns it; never freed, so a
/// pointer to one stays valid for the life of the process.
struct Node;

/// A path below a key, one name per component, already split and validated.
using KeyPath = std::span<const std::wstring_view>;

/// What `QueryInfoKey` reports, as owned values. Lengths are in characters
/// without terminators, except @ref cbMaxValue.
struct KeyInfo
{
    std::wstring className;
    DWORD cSubKeys = 0;
    DWORD cchMaxSubKey = 0;
    DWORD cchMaxClass = 0;
    DWORD cValues = 0;
    DWORD cchMaxValueName = 0;
    DWORD cbMaxValue = 0;
    FILETIME lastWrite{};
};

/// Where an open landed: a node of the hive, or (for a key a frozen overlay
/// leaves untouched) a handle @ref Hive::Lower opened, which the caller now
/// owns.
struct OpenResult
{
    Node* pNode = nullptr;
    HKEY hLower = nullptr;
};

/**
 * @brief Storage for one subtree of a virtual namespace: a tree with one
 *  anonymous root, mounted at a path by `VirtualRegistry`.
 *
 *  The interface is node-level with owned values. Caller buffers, A and W,
 *  and handles stay in `VirtualRegistry`, once for every hive. Every member
 *  answers the `LSTATUS` the native registry answers for the same operation;
 *  a call on a deleted node answers `ERROR_KEY_DELETED`, and a write to a
 *  frozen hive `ERROR_ACCESS_DENIED`.
 *
 *  Each node has exactly one virtual handle, issued when the node is created
 *  (@ref HandleOf).
 */
class Hive
{
public:
    virtual ~Hive() = default;

    /// Makes the hive immutable from now on, so its readers take no lock.
    /// Safe while it is mounted and in use; there is no way back.
    virtual void Freeze() noexcept = 0;
    [[nodiscard]] virtual bool IsFrozen() const noexcept = 0;

    [[nodiscard]] virtual Node* Root() noexcept = 0;

    /// The virtual handle of @p pNode.
    [[nodiscard]] static HKEY HandleOf(const Node* pNode) noexcept;
    /// The node a virtual handle of this copy of VReg belongs to, and its
    /// hive; null for any other handle.
    [[nodiscard]] static Node* NodeOf(HKEY hKey) noexcept;
    [[nodiscard]] static Hive* HiveOf(const Node* pNode) noexcept;

    virtual LSTATUS OpenKey(
        Node* pParent,
        KeyPath path,
        REGSAM samDesired,
        OpenResult& result
    ) noexcept = 0;

    /// On a frozen hive an existing key opens as with @ref OpenKey, and a
    /// missing one answers `ERROR_ACCESS_DENIED`.
    virtual LSTATUS CreateKey(
        Node* pParent,
        KeyPath path,
        std::wstring_view className,
        DWORD dwOptions,
        REGSAM samDesired,
        OpenResult& result,
        DWORD& dwDisposition
    ) noexcept = 0;

    /// Deletes the last component of @p path, which must have no subkeys.
    virtual LSTATUS DeleteKey(Node* pParent, KeyPath path) noexcept = 0;
    /// Deletes every subkey and value of @p pNode, leaving the node itself.
    virtual LSTATUS DeleteTree(Node* pNode) noexcept = 0;

    virtual LSTATUS QueryValue(
        Node* pNode,
        std::wstring_view name,
        DWORD& dwType,
        std::vector<std::byte>& data
    ) noexcept = 0;

    virtual LSTATUS SetValue(
        Node* pNode,
        std::wstring_view name,
        DWORD dwType,
        std::span<const std::byte> data
    ) noexcept = 0;

    virtual LSTATUS DeleteValue(
        Node* pNode,
        std::wstring_view name
    ) noexcept = 0;

    /**
     * @brief Sets value @p name of @p pNode to @p text as a `REG_SZ` and
     *  marks it MUI text.
     *
     *  `RegLoadMUIString` answers an unmarked hive value `ERROR_INVALID_DATA`,
     *  since resolving an `@` resource reference would load a module. A
     *  marked one stands for the text such a reference resolves to and
     *  answers with @p text itself. Any other write of the value drops the
     *  mark, and a `.reg` export carries it as a plain string.
     */
    virtual LSTATUS SetMuiTextValue(
        Node* pNode,
        std::wstring_view name,
        std::wstring_view text
    ) noexcept = 0;

    /// The text of value @p name of @p pNode if @ref SetMuiTextValue stored
    /// it, or `ERROR_INVALID_DATA` for any other value.
    virtual LSTATUS QueryMuiText(
        Node* pNode,
        std::wstring_view name,
        std::wstring& text
    ) noexcept = 0;

    virtual LSTATUS EnumKey(
        Node* pNode,
        DWORD dwIndex,
        std::wstring& name,
        std::wstring& className,
        FILETIME& lastWrite
    ) noexcept = 0;

    virtual LSTATUS EnumValue(
        Node* pNode,
        DWORD dwIndex,
        std::wstring& name,
        DWORD& dwType,
        std::vector<std::byte>& data
    ) noexcept = 0;

    virtual LSTATUS QueryInfo(Node* pNode, KeyInfo& info) noexcept = 0;
    /// What `FlushKey` on @p pNode needs from the hive: success, or
    /// `ERROR_KEY_DELETED`.
    virtual LSTATUS CheckKey(Node* pNode) noexcept = 0;

    /**
     * @brief Registers a one-shot change notification on @p pNode.
     *
     *  @p hEvent is signaled once, by the first write that @p dwFilter selects
     *  (`REG_NOTIFY_CHANGE_NAME` for a subkey added or deleted,
     *  `REG_NOTIFY_CHANGE_LAST_SET` for a value set or deleted; the other
     *  bits never fire) on the key, or below it when @p isSubtree. Deleting a
     *  watched key fires its watches too. The hive duplicates @p hEvent, so
     *  the caller may close its own handle. A frozen hive accepts the
     *  registration and never signals. Answers `ERROR_KEY_DELETED` for a
     *  deleted key, or the error of duplicating @p hEvent.
     *
     * @note Only writes made through the hive are seen; a change in the
     *  registry below an `OverlayHive` is not.
     */
    virtual LSTATUS Watch(
        Node* pNode,
        bool isSubtree,
        DWORD dwFilter,
        HANDLE hEvent
    ) noexcept = 0;

    /// The registry below this hive, which answers the handles in
    /// @ref OpenResult::hLower; null for a hive with nothing below it.
    [[nodiscard]] virtual Registry* Lower() noexcept;

    /// A key of @ref Lower holding value @p name of @p pNode, when that value
    /// shows through from below; null (and success) when the hive holds it
    /// or nothing does. The caller closes it through @ref Lower.
    virtual LSTATUS OpenLowerValueKey(
        Node* pNode,
        std::wstring_view name,
        HKEY& hLower
    ) noexcept;
};
} // namespace VReg
