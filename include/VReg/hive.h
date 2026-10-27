#pragma once

#include <Windows.h>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace VReg
{
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
/// leaves untouched) a handle the lower registry opened, which the caller now
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

    virtual LSTATUS OpenKey(Node* pParent, KeyPath path, REGSAM samDesired, OpenResult& result) noexcept = 0;
    virtual LSTATUS CreateKey(
        Node* pParent,
        KeyPath path,
        std::wstring_view className,
        DWORD dwOptions,
        Node*& pNode,
        DWORD& dwDisposition
    ) noexcept = 0;
    /// Deletes the last component of @p path, which must have no subkeys.
    virtual LSTATUS DeleteKey(Node* pParent, KeyPath path) noexcept = 0;
    /// Deletes every subkey and value of @p pNode, leaving the node itself.
    virtual LSTATUS DeleteTree(Node* pNode) noexcept = 0;

    virtual LSTATUS QueryValue(Node* pNode, std::wstring_view name, DWORD& dwType, std::vector<std::byte>& data) noexcept = 0;
    virtual LSTATUS SetValue(Node* pNode, std::wstring_view name, DWORD dwType, std::span<const std::byte> data) noexcept = 0;
    virtual LSTATUS DeleteValue(Node* pNode, std::wstring_view name) noexcept = 0;

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
    /// What `FlushKey` and a change notification on @p pNode need from the
    /// hive: success, or `ERROR_KEY_DELETED`.
    virtual LSTATUS CheckKey(Node* pNode) noexcept = 0;
};
} // namespace VReg
