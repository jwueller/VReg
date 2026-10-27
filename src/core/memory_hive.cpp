#include <VReg/memory_hive.h>

#include "tree.h"

#include <algorithm>
#include <new>

namespace VReg
{
namespace
{
/// The node @p path names below @p pNode, or null.
Node* Walk(Node* pNode, const KeyPath path) noexcept
{
    for (const auto name : path)
    {
        pNode = Tree::FindChild(*pNode, name);
        if (!pNode)
            return nullptr;
    }

    return pNode;
}
} // namespace

MemoryHive::MemoryHive()
    : m_pTree(std::make_unique<Tree>(*this))
{
}

MemoryHive::~MemoryHive() = default;

void MemoryHive::Freeze() noexcept
{
    m_pTree->Freeze();
}

bool MemoryHive::IsFrozen() const noexcept
{
    return m_pTree->IsFrozen();
}

Node* MemoryHive::Root() noexcept
{
    return m_pTree->Root();
}

LSTATUS MemoryHive::OpenKey(Node* const pParent, const KeyPath path, const REGSAM samDesired, OpenResult& result) noexcept
{
    (void)samDesired;

    Tree::ReadLock lock(*m_pTree);
    if (pParent->isDeleted)
        return ERROR_KEY_DELETED;

    auto* const pNode = Walk(pParent, path);
    if (!pNode)
        return ERROR_FILE_NOT_FOUND;

    result = {.pNode = pNode, .hLower = nullptr};
    return ERROR_SUCCESS;
}

LSTATUS MemoryHive::CreateKey(
    Node* const pParent,
    const KeyPath path,
    const std::wstring_view className,
    const DWORD dwOptions,
    const REGSAM samDesired,
    OpenResult& result,
    DWORD& dwDisposition
) noexcept
try
{
    if (IsFrozen())
    {
        const auto status = OpenKey(pParent, path, samDesired, result);
        if (status == ERROR_FILE_NOT_FOUND)
            return ERROR_ACCESS_DENIED;

        dwDisposition = REG_OPENED_EXISTING_KEY;
        return status;
    }

    Tree::WriteLock lock(*m_pTree);
    if (pParent->isDeleted)
        return ERROR_KEY_DELETED;

    const bool isVolatile = (dwOptions & REG_OPTION_VOLATILE) != 0;
    auto* pCurrent = pParent;
    dwDisposition = REG_OPENED_EXISTING_KEY;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        const auto name = path[i];
        if (auto* const pChild = Tree::FindChild(*pCurrent, name))
        {
            pCurrent = pChild;
            continue;
        }

        if (pCurrent->isVolatile && !isVolatile)
            return ERROR_CHILD_MUST_BE_VOLATILE;

        const bool isLast = i + 1 == path.size();
        pCurrent = m_pTree->AddChild(*pCurrent, name, isLast ? className : std::wstring_view(), isVolatile);
        if (!pCurrent)
            return ERROR_OUTOFMEMORY;

        dwDisposition = REG_CREATED_NEW_KEY;
    }

    result = {.pNode = pCurrent, .hLower = nullptr};
    return ERROR_SUCCESS;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS MemoryHive::DeleteKey(Node* const pParent, const KeyPath path) noexcept
{
    if (IsFrozen())
        return ERROR_ACCESS_DENIED;

    Tree::WriteLock lock(*m_pTree);
    if (pParent->isDeleted)
        return ERROR_KEY_DELETED;

    auto* const pNode = Walk(pParent, path);
    if (!pNode)
        return ERROR_FILE_NOT_FOUND;

    if (!pNode->pParent || !pNode->children.empty())
        return ERROR_ACCESS_DENIED;

    Tree::Remove(*pNode);
    return ERROR_SUCCESS;
}

LSTATUS MemoryHive::DeleteTree(Node* const pNode) noexcept
{
    if (IsFrozen())
        return ERROR_ACCESS_DENIED;

    Tree::WriteLock lock(*m_pTree);
    if (pNode->isDeleted)
        return ERROR_KEY_DELETED;

    while (!pNode->children.empty())
        Tree::Remove(*pNode->children.back());

    pNode->values.clear();
    Tree::Touch(*pNode);
    return ERROR_SUCCESS;
}

LSTATUS MemoryHive::QueryValue(
    Node* const pNode,
    const std::wstring_view name,
    DWORD& dwType,
    std::vector<std::byte>& data
) noexcept
try
{
    Tree::ReadLock lock(*m_pTree);
    if (pNode->isDeleted)
        return ERROR_KEY_DELETED;

    const auto* const pValue = Tree::FindValue(*pNode, name);
    if (!pValue)
        return ERROR_FILE_NOT_FOUND;

    dwType = pValue->dwType;
    data = pValue->data;
    return ERROR_SUCCESS;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS MemoryHive::SetValue(
    Node* const pNode,
    const std::wstring_view name,
    const DWORD dwType,
    const std::span<const std::byte> data
) noexcept
try
{
    if (IsFrozen())
        return ERROR_ACCESS_DENIED;

    Tree::WriteLock lock(*m_pTree);
    if (pNode->isDeleted)
        return ERROR_KEY_DELETED;

    std::vector<std::byte> bytes(data.begin(), data.end());
    if (auto* const pValue = Tree::FindValue(*pNode, name))
    {
        pValue->dwType = dwType;
        pValue->data = std::move(bytes);
    }
    else
    {
        pNode->values.push_back({.name = std::wstring(name), .dwType = dwType, .data = std::move(bytes)});
    }

    Tree::Touch(*pNode);
    return ERROR_SUCCESS;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS MemoryHive::DeleteValue(Node* const pNode, const std::wstring_view name) noexcept
{
    if (IsFrozen())
        return ERROR_ACCESS_DENIED;

    Tree::WriteLock lock(*m_pTree);
    if (pNode->isDeleted)
        return ERROR_KEY_DELETED;

    const auto* const pValue = Tree::FindValue(*pNode, name);
    if (!pValue)
        return ERROR_FILE_NOT_FOUND;

    pNode->values.erase(pNode->values.begin() + (pValue - pNode->values.data()));
    Tree::Touch(*pNode);
    return ERROR_SUCCESS;
}

LSTATUS MemoryHive::EnumKey(
    Node* const pNode,
    const DWORD dwIndex,
    std::wstring& name,
    std::wstring& className,
    FILETIME& lastWrite
) noexcept
try
{
    Tree::ReadLock lock(*m_pTree);
    if (pNode->isDeleted)
        return ERROR_KEY_DELETED;

    if (dwIndex >= pNode->children.size())
        return ERROR_NO_MORE_ITEMS;

    const auto* const pChild = pNode->children[dwIndex];
    name = pChild->name;
    className = pChild->className;
    lastWrite = pChild->lastWrite;
    return ERROR_SUCCESS;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS MemoryHive::EnumValue(
    Node* const pNode,
    const DWORD dwIndex,
    std::wstring& name,
    DWORD& dwType,
    std::vector<std::byte>& data
) noexcept
try
{
    Tree::ReadLock lock(*m_pTree);
    if (pNode->isDeleted)
        return ERROR_KEY_DELETED;

    if (dwIndex >= pNode->values.size())
        return ERROR_NO_MORE_ITEMS;

    const auto& value = pNode->values[dwIndex];
    name = value.name;
    dwType = value.dwType;
    data = value.data;
    return ERROR_SUCCESS;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS MemoryHive::QueryInfo(Node* const pNode, KeyInfo& info) noexcept
try
{
    Tree::ReadLock lock(*m_pTree);
    if (pNode->isDeleted)
        return ERROR_KEY_DELETED;

    info = {};
    info.className = pNode->className;
    info.cSubKeys = static_cast<DWORD>(pNode->children.size());
    for (const auto* const pChild : pNode->children)
    {
        info.cchMaxSubKey = std::max(info.cchMaxSubKey, static_cast<DWORD>(pChild->name.size()));
        info.cchMaxClass = std::max(info.cchMaxClass, static_cast<DWORD>(pChild->className.size()));
    }

    info.cValues = static_cast<DWORD>(pNode->values.size());
    for (const auto& value : pNode->values)
    {
        info.cchMaxValueName = std::max(info.cchMaxValueName, static_cast<DWORD>(value.name.size()));
        info.cbMaxValue = std::max(info.cbMaxValue, static_cast<DWORD>(value.data.size()));
    }

    info.lastWrite = pNode->lastWrite;
    return ERROR_SUCCESS;
}
catch (const std::bad_alloc&)
{
    return ERROR_OUTOFMEMORY;
}

LSTATUS MemoryHive::CheckKey(Node* const pNode) noexcept
{
    Tree::ReadLock lock(*m_pTree);
    return pNode->isDeleted ? ERROR_KEY_DELETED : ERROR_SUCCESS;
}
} // namespace VReg
