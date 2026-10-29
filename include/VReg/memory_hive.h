#pragma once

#include "hive.h"

#include <memory>

namespace VReg
{
class Tree;

/**
 * @brief A hive held entirely in memory; starts empty.
 *
 * @note A hive must outlive every registry it is mounted in. Destroying it
 *  turns its handles into invalid ones.
 */
class MemoryHive final : public Hive
{
public:
    /// Throws `std::bad_alloc`.
    MemoryHive();
    ~MemoryHive() override;

    void Freeze() noexcept override;
    [[nodiscard]] bool IsFrozen() const noexcept override;
    [[nodiscard]] Node* Root() noexcept override;

    LSTATUS OpenKey(
        Node* pParent,
        KeyPath path,
        REGSAM samDesired,
        OpenResult& result
    ) noexcept override;

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

    LSTATUS QueryValue(
        Node* pNode,
        std::wstring_view name,
        DWORD& dwType,
        std::vector<std::byte>& data
    ) const noexcept override;

    LSTATUS SetValue(
        Node* pNode,
        std::wstring_view name,
        DWORD dwType,
        std::span<const std::byte> data
    ) noexcept
        override;
    LSTATUS DeleteValue(Node* pNode, std::wstring_view name) noexcept override;

    LSTATUS SetMuiTextValue(
        Node* pNode,
        std::wstring_view name,
        std::wstring_view text
    ) noexcept override;

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
    ) const noexcept
        override;

    LSTATUS EnumValue(
        Node* pNode,
        DWORD dwIndex,
        std::wstring& name,
        DWORD& dwType,
        std::vector<std::byte>& data
    ) const noexcept
        override;
    LSTATUS QueryInfo(Node* pNode, KeyInfo& info) const noexcept override;
    LSTATUS CheckKey(Node* pNode) const noexcept override;

    LSTATUS Watch(
        Node* pNode,
        bool isSubtree,
        DWORD dwFilter,
        HANDLE hEvent
    ) noexcept override;

private:
    LSTATUS StoreValue(
        Node* pNode,
        std::wstring_view name,
        DWORD dwType,
        std::span<const std::byte> data,
        bool isMuiText
    ) noexcept;

    std::unique_ptr<Tree> m_pTree;
};
} // namespace VReg
