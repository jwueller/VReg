#pragma once

#include "hive.h"

#include <memory>

namespace VReg
{
class Tree;

/**
 * @brief A hive held entirely in memory; starts empty.
 *
 * @note A hive MUST outlive every registry it is mounted in. Destroying it
 *  turns its handles into invalid ones.
 */
class MemoryHive final : public Hive
{
public:
    /// Throws @ref std::bad_alloc.
    MemoryHive();
    ~MemoryHive() override;

    void Freeze() noexcept override;
    [[nodiscard]] bool IsFrozen() const noexcept override;
    [[nodiscard]] _Ret_notnull_ Node* Root() const noexcept override;

    LSTATUS OpenKey(
        _In_ Node* pParent,
        KeyPath path,
        REGSAM samDesired,
        OpenResult& result
    ) const noexcept override;

    LSTATUS CreateKey(
        _Inout_ Node* pParent,
        KeyPath path,
        _In_z_ LPCWSTR pszClassName,
        DWORD dwOptions,
        REGSAM samDesired,
        OpenResult& result,
        DWORD& dwDisposition
    ) noexcept override;

    LSTATUS DeleteKey(
        _Inout_ Node* pParent,
        KeyPath path
    ) noexcept override;

    LSTATUS DeleteTree(
        _Inout_ Node* pNode
    ) noexcept override;

    LSTATUS QueryValue(
        _In_ Node* pNode,
        _In_z_ LPCWSTR pszName,
        DWORD& dwType,
        DWORD& cbData,
        _Out_opt_ std::vector<std::byte>* pData
    ) const noexcept override;

    LSTATUS SetValue(
        _Inout_ Node* pNode,
        _In_z_ LPCWSTR pszName,
        DWORD dwType,
        std::span<const std::byte> data
    ) noexcept override;

    LSTATUS DeleteValue(
        _Inout_ Node* pNode,
        _In_z_ LPCWSTR pszName
    ) noexcept override;

    LSTATUS SetMuiTextValue(
        _Inout_ Node* pNode,
        _In_z_ LPCWSTR pszName,
        _In_z_ LPCWSTR pszText
    ) noexcept override;

    LSTATUS QueryMuiText(
        _In_ Node* pNode,
        _In_z_ LPCWSTR pszName,
        std::wstring& text
    ) const noexcept override;

    LSTATUS EnumKey(
        _In_ Node* pNode,
        DWORD dwIndex,
        std::wstring& name,
        std::wstring& className,
        FILETIME& lastWrite
    ) const noexcept override;

    LSTATUS EnumValue(
        _In_ Node* pNode,
        DWORD dwIndex,
        std::wstring& name,
        DWORD& dwType,
        DWORD& cbData,
        _Out_opt_ std::vector<std::byte>* pData
    ) const noexcept override;

    LSTATUS QueryInfo(
        _In_ Node* pNode,
        KeyInfo& info
    ) const noexcept override;

    LSTATUS CheckKey(
        _In_ Node* pNode
    ) const noexcept override;

    LSTATUS Watch(
        _In_ Node* pNode,
        BOOL bWatchSubtree,
        DWORD dwFilter,
        HANDLE hEvent
    ) const noexcept override;

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
