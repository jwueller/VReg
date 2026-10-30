#pragma once

#include "hive.h"

#include <memory>
#include <string>
#include <string_view>

namespace VReg
{
class Registry;
class Tree;

/// One entry of an overlay's upper layer, as @ref OverlayHive::SnapshotUpper
/// lists it.
struct UpperRecord
{
    enum class Kind
    {
        Key,
        Value,
    } kind{};

    /// The key, relative to the hive's root and separated by backslashes; empty
    /// for the root itself.
    std::wstring path;

    /// Values only; empty for the default value.
    std::wstring name;

    /// Values only.
    DWORD dwType = REG_NONE;

    /// Values only.
    std::vector<std::byte> data;

    /// A key or value of the lower layer that is deleted here.
    bool isTombstone = false;

    /// Keys only: the key hides everything the lower key of this path holds (it
    /// was deleted and created again, or emptied by
    /// @ref Registry::RegDeleteTreeW).
    bool isOpaque = false;
};

/**
 * @brief A hive layered over a key of another registry: reads fall through
 *  to the lower key, writes stay in memory.
 *
 *  The upper layer adds and overrides keys and values. Deleting a lower value
 *  or key leaves a tombstone that hides it; a key deleted and created again,
 *  or emptied with @ref Registry::RegDeleteTreeW, becomes opaque and hides
 *  everything below it in the lower layer. Enumeration lists the upper
 *  entries first, then the lower ones no upper entry shadows; counts cover
 *  both.
 *
 *  The lower registry is never called with the hive's lock held, so a lower
 *  call that loads a module or takes a long time cannot hold up the hive.
 *
 *  Frozen, the hive answers an open of a key it has nothing at or below with
 *  the lower registry's own handle (@ref OpenResult::hLower). A
 *  @ref VirtualRegistry sends later calls on such a handle to its base, so a
 *  frozen overlay's lower registry MUST be that base or answer the same
 *  handles.
 *
 * @note A hive MUST outlive every registry it is mounted in.
 */
class OverlayHive final : public Hive
{
public:
    /// Over the key @p lowerPath below @p hLowerRoot of @p lower, which MUST
    /// outlive the hive. @p samLowerView (@ref KEY_WOW64_64KEY,
    /// @ref KEY_WOW64_32KEY or 0) is added to every lower open. Throws
    /// @ref std::bad_alloc.
    OverlayHive(
        Registry& lower,
        HKEY hLowerRoot,
        std::wstring_view lowerPath,
        REGSAM samLowerView = 0
    );

    ~OverlayHive() override;

    void Freeze() noexcept override;
    [[nodiscard]] bool IsFrozen() const noexcept override;
    [[nodiscard]] Node* Root() const noexcept override;

    LSTATUS OpenKey(
        Node* pParent,
        KeyPath path,
        REGSAM samDesired,
        OpenResult& result
    ) const noexcept override;

    LSTATUS CreateKey(
        Node* pParent,
        KeyPath path,
        std::wstring_view className,
        DWORD dwOptions,
        REGSAM samDesired,
        OpenResult& result,
        DWORD& dwDisposition
    ) noexcept override;

    LSTATUS DeleteKey(
        Node* pParent,
        KeyPath path
    ) noexcept override;

    LSTATUS DeleteTree(
        Node* pNode
    ) noexcept override;

    LSTATUS QueryValue(
        Node* pNode,
        std::wstring_view name,
        DWORD& dwType,
        DWORD& cbData,
        std::vector<std::byte>* pData
    ) const noexcept override;

    LSTATUS SetValue(
        Node* pNode,
        std::wstring_view name,
        DWORD dwType,
        std::span<const std::byte> data
    ) noexcept override;

    LSTATUS DeleteValue(
        Node* pNode,
        std::wstring_view name
    ) noexcept override;

    LSTATUS SetMuiTextValue(
        Node* pNode,
        std::wstring_view name,
        std::wstring_view text
    ) noexcept override;

    /// A value from below is never marked.
    LSTATUS QueryMuiText(
        Node* pNode,
        std::wstring_view name,
        std::wstring& text
    ) const noexcept override;

    LSTATUS EnumKey(
        Node* pNode,
        DWORD dwIndex,
        std::wstring& name,
        std::wstring& className,
        FILETIME& lastWrite
    ) const noexcept override;

    LSTATUS EnumValue(
        Node* pNode,
        DWORD dwIndex,
        std::wstring& name,
        DWORD& dwType,
        DWORD& cbData,
        std::vector<std::byte>* pData
    ) const noexcept override;

    LSTATUS QueryInfo(
        Node* pNode,
        KeyInfo& info
    ) const noexcept override;

    LSTATUS CheckKey(
        Node* pNode
    ) const noexcept override;

    LSTATUS Watch(
        Node* pNode,
        bool isSubtree,
        DWORD dwFilter,
        HANDLE hEvent
    ) const noexcept override;

    /**
     * @brief Lists the upper layer: what the hive holds itself, as opposed to
     *  what shows through from below.
     *
     *  This is the seam `.reg` export reads. A key comes before its values,
     *  which come before its subkeys, which are in name order; values are in
     *  creation order. A key that only stands for a lower key, with no values
     *  of its own, is not listed; the upper keys below it are. Reads only the
     *  hive, never the lower registry.
     */
    LSTATUS SnapshotUpper(std::vector<UpperRecord>& records) const noexcept;

    [[nodiscard]] Registry* Lower() const noexcept override;

    LSTATUS OpenLowerValueKey(
        Node* pNode,
        std::wstring_view name,
        HKEY& hLower
    ) const noexcept override;

private:
    /// Whether a node's lower key shows through, and its path; read under
    /// the lock, used after it is released.
    struct LowerView
    {
        bool isVisible = false;
        std::wstring path;
    };

    [[nodiscard]] LowerView ViewOf(const Node& node) const;

    LSTATUS StoreValue(
        Node* pNode,
        std::wstring_view name,
        DWORD dwType,
        std::span<const std::byte> data,
        bool isMuiText
    ) noexcept;

    LSTATUS OpenLower(
        const std::wstring& path,
        REGSAM samDesired,
        HKEY& hKey
    ) const noexcept;

    /// The class of the lower key at @p path; empty when there is none.
    LSTATUS QueryLowerClass(
        const std::wstring& path,
        std::wstring& className
    ) const noexcept;

    Registry& m_lower;
    HKEY m_hLowerRoot;
    std::wstring m_lowerPath;
    REGSAM m_samLowerView;
    std::unique_ptr<Tree> m_pTree;
};
} // namespace VReg
