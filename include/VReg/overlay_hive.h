#pragma once

#include <VReg/hive.h>

#include <memory>
#include <string>
#include <string_view>

namespace VReg
{
class Registry;
class Tree;

/**
 * @brief A hive layered over a key of another registry: reads fall through
 *  to the lower key, writes stay in memory.
 *
 *  The upper layer adds and overrides keys and values. Deleting a lower value
 *  or key leaves a tombstone that hides it; a key deleted and created again,
 *  or emptied with `RegDeleteTree`, becomes opaque and hides everything below
 *  it in the lower layer. Enumeration lists the upper entries first, then the
 *  lower ones no upper entry shadows; counts cover both.
 *
 *  The lower registry is never called with the hive's lock held, so a lower
 *  call that loads a module or takes a long time cannot hold up the hive.
 *
 *  Frozen, the hive answers an open of a key it has nothing at or below with
 *  the lower registry's own handle (@ref OpenResult::hLower). A
 *  `VirtualRegistry` sends later calls on such a handle to its base, so a
 *  frozen overlay's lower registry must be that base or answer the same
 *  handles.
 *
 * @note A hive must outlive every registry it is mounted in.
 */
class OverlayHive final : public Hive
{
public:
    /// Over the key @p lowerPath below @p hLowerRoot of @p lower, which must
    /// outlive the hive. @p samLowerView (`KEY_WOW64_64KEY`,
    /// `KEY_WOW64_32KEY` or 0) is added to every lower open. Throws
    /// `std::bad_alloc`.
    OverlayHive(Registry& lower, HKEY hLowerRoot, std::wstring_view lowerPath, REGSAM samLowerView = 0);
    ~OverlayHive() override;

    void Freeze() noexcept override;
    [[nodiscard]] bool IsFrozen() const noexcept override;
    [[nodiscard]] Node* Root() noexcept override;

    LSTATUS OpenKey(Node* pParent, KeyPath path, REGSAM samDesired, OpenResult& result) noexcept override;
    LSTATUS CreateKey(
        Node* pParent,
        KeyPath path,
        std::wstring_view className,
        DWORD dwOptions,
        REGSAM samDesired,
        OpenResult& result,
        DWORD& dwDisposition
    ) noexcept override;
    LSTATUS DeleteKey(Node* pParent, KeyPath path) noexcept override;
    LSTATUS DeleteTree(Node* pNode) noexcept override;

    LSTATUS QueryValue(Node* pNode, std::wstring_view name, DWORD& dwType, std::vector<std::byte>& data) noexcept
        override;
    LSTATUS SetValue(Node* pNode, std::wstring_view name, DWORD dwType, std::span<const std::byte> data) noexcept
        override;
    LSTATUS DeleteValue(Node* pNode, std::wstring_view name) noexcept override;

    LSTATUS EnumKey(Node* pNode, DWORD dwIndex, std::wstring& name, std::wstring& className, FILETIME& lastWrite) noexcept
        override;
    LSTATUS EnumValue(Node* pNode, DWORD dwIndex, std::wstring& name, DWORD& dwType, std::vector<std::byte>& data) noexcept
        override;
    LSTATUS QueryInfo(Node* pNode, KeyInfo& info) noexcept override;
    LSTATUS CheckKey(Node* pNode) noexcept override;

    [[nodiscard]] Registry* Lower() noexcept override;
    LSTATUS OpenLowerValueKey(Node* pNode, std::wstring_view name, HKEY& hLower) noexcept override;

private:
    /// Whether a node's lower key shows through, and its path; read under
    /// the lock, used after it is released.
    struct LowerView
    {
        bool isVisible = false;
        std::wstring path;
    };

    [[nodiscard]] LowerView ViewOf(const Node& node) const;
    LSTATUS OpenLower(const std::wstring& path, REGSAM samDesired, HKEY& hKey) noexcept;
    /// The class of the lower key at @p path; empty when there is none.
    LSTATUS QueryLowerClass(const std::wstring& path, std::wstring& className) noexcept;

    Registry& m_lower;
    HKEY m_hLowerRoot;
    std::wstring m_lowerPath;
    REGSAM m_samLowerView;
    std::unique_ptr<Tree> m_pTree;
};
} // namespace VReg
