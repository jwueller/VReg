#include <VReg/hive.h>

#include <VReg/owned_values.h>
#include <VReg/registry.h>

#include "core/handle_space.h"
#include "core/node.h"
#include "core/status.h"

#include <algorithm>
#include <array>
#include <new>

namespace VReg
{
namespace
{
std::wstring JoinPath(std::wstring path, const KeyPath rest)
{
    for (const auto name : rest)
    {
        if (!path.empty())
            path += L'\\';
        path += name;
    }

    return path;
}

/// Makes @p node and its transparent ancestors upper keys, as a write to a
/// key below them needs. The root stays as it is: it is the mount point.
void Materialize(Node& node) noexcept
{
    for (
        auto* pNode = &node;
        pNode->pParent && pNode->isTransparent;
        pNode = pNode->pParent
    )
        pNode->isTransparent = false;
}

/// Whether @p names, sorted by @ref CompareNames, holds @p name.
bool ContainsName(
    const std::vector<std::wstring>& names,
    const std::wstring_view name
) noexcept
{
    return std::ranges::binary_search(names, name, NameLess);
}

/// Appends @p node and everything below it to @p records, leaving out a
/// transparent node without values (the way to a lower key, which only the root
/// can be while it has upper keys below it).
void ListUpper(
    const Node& node,
    std::wstring& path,
    std::vector<UpperRecord>& records
)
{
    const auto iFirst = records.size();
    records.push_back(
        {
            .kind = UpperRecord::Kind::Key,
            .path = path,
            .name = {},
            .dwType = REG_NONE,
            .data = {},
            .isTombstone = false,
            .isOpaque = node.isOpaque
        }
    );
    for (const auto& value : node.values)
    {
        records.push_back(
            {
                .kind = UpperRecord::Kind::Value,
                .path = path,
                .name = value.name,
                .dwType = value.dwType,
                .data = value.data,
                .isTombstone = value.isTombstone
            }
        );
    }

    const bool isBare = node.isTransparent && node.values.empty();
    const auto cchPath = path.size();
    for (const auto* const pChild : node.children)
    {
        if (!path.empty())
            path += L'\\';
        path += pChild->name;
        if (pChild->isTombstone)
            records.push_back(
                {
                    .kind = UpperRecord::Kind::Key,
                    .path = path,
                    .name = {},
                    .dwType = REG_NONE,
                    .data = {},
                    .isTombstone = true,
                    .isOpaque = false
                }
            );
        else
            ListUpper(*pChild, path, records);

        path.resize(cchPath);
    }

    if (isBare)
        records.erase(records.begin() + static_cast<std::ptrdiff_t>(iFirst));
}

/// A lower key, closed through its registry.
class LowerKey
{
public:
    explicit LowerKey(Registry& registry) noexcept :
        m_registry(registry)
    {
    }

    ~LowerKey()
    {
        if (m_hKey)
            m_registry.RegCloseKey(
                m_hKey
            );
    }

    LowerKey(const LowerKey&) = delete;
    LowerKey& operator=(const LowerKey&) = delete;

    [[nodiscard]] HKEY Get() const noexcept
    {
        return m_hKey;
    }

    [[nodiscard]] HKEY& Receive() noexcept
    {
        return m_hKey;
    }

    [[nodiscard]] HKEY Release() noexcept
    {
        return std::exchange(m_hKey, nullptr);
    }

private:
    Registry& m_registry;
    HKEY m_hKey = nullptr;
};

/// Whether @p status says the thing asked about exists: found, or found but
/// not readable this way.
bool Exists(const LSTATUS status) noexcept
{
    return
        status == ERROR_SUCCESS
        || status == ERROR_MORE_DATA
        || status == ERROR_ACCESS_DENIED;
}

void DropWatches(Node& node) noexcept
{
    for (const auto& watch : node.watches)
        CloseHandle(watch.hEvent);

    node.watches.clear();
}

/// Signals and drops the watches of @p node that @p dwFilter selects;
/// @p isInherited limits them to those watching their subtree.
void Fire(Node& node, const DWORD dwFilter, const bool isInherited) noexcept
{
    std::erase_if(
        node.watches,
        [&](const ChangeWatch& watch)
        {
            if (
                !(watch.dwFilter & dwFilter)
                || (isInherited && !watch.isSubtree)
            )
                return false;

            SetEvent(watch.hEvent);
            CloseHandle(watch.hEvent);
            return true;
        }
    );
}
} // namespace

int CompareNames(
    const std::wstring_view left,
    const std::wstring_view right
) noexcept
{
    if (left.empty() || right.empty())
        return left.empty() == right.empty() ? 0 : (left.empty() ? -1 : 1);

    return CompareStringOrdinal(
        left.data(),
        static_cast<int>(left.size()),
        right.data(),
        static_cast<int>(right.size()),
        TRUE
    ) - CSTR_EQUAL;
}

std::vector<std::byte> StringData(const std::wstring_view text)
{
    const auto* const pb = reinterpret_cast<const std::byte*>(text.data());
    std::vector<std::byte> data(pb, pb + text.size() * sizeof(wchar_t));
    data.resize(data.size() + sizeof(wchar_t));
    return data;
}

LSTATUS MuiTextOf(const Value& value, std::wstring& text)
{
    if (!value.isMuiText)
        return ERROR_INVALID_DATA;

    text.assign(
        reinterpret_cast<const wchar_t*>(value.data.data()),
        value.data.size() / sizeof(wchar_t) - 1
    );
    return ERROR_SUCCESS;
}

HKEY Hive::HandleOf(_In_ const Node* const pNode) noexcept
{
    return pNode->hKey;
}

_Ret_maybenull_ Node* Hive::NodeOf(const HKEY hKey) noexcept
{
    return static_cast<Node*>(HandleSpace::Process().Find(hKey));
}

_Ret_notnull_ Hive* Hive::HiveOf(_In_ const Node* const pNode) noexcept
{
    return pNode->pHive;
}

Hive::Hive(
    _In_opt_ Registry* const pLower,
    const HKEY hLowerRoot,
    const std::wstring_view lowerPath,
    const REGSAM samLowerView
) :
    m_pLower(pLower),
    m_hLowerRoot(hLowerRoot),
    m_lowerPath(lowerPath),
    m_samLowerView(samLowerView)
{
    auto pRoot = std::make_unique<Node>();
    pRoot->pHive = this;
    pRoot->hKey = HandleSpace::Process().Allocate(pRoot.get());
    if (!pRoot->hKey)
        throw std::bad_alloc();

    Touch(*pRoot);
    if (m_pLower)
    {
        pRoot->isTransparent = true;
        pRoot->isLowerBacked = true;
    }

    m_pRoot = pRoot.get();
    m_nodes.push_back(std::move(pRoot));
}

Hive::Hive() :
    Hive(nullptr, nullptr, {}, 0)
{
}

Hive::Hive(
    Registry& lower,
    const HKEY hLowerRoot,
    _In_z_ const LPCWSTR pszLowerPath,
    const REGSAM samLowerView
) :
    Hive(&lower, hLowerRoot, pszLowerPath, samLowerView)
{
}

Hive::~Hive()
{
    for (const auto& pNode : m_nodes)
    {
        DropWatches(*pNode);
        HandleSpace::Process().Release(pNode->hKey);
    }
}

void Hive::Freeze() noexcept
{
    // Taking the lock once more waits out every writer still inside; readers
    // that see the flag afterwards skip the lock and see all their writes.
    AcquireSRWLockExclusive(&m_lock);
    m_isFrozen.store(true, std::memory_order_release);
    for (const auto& pNode : m_nodes)
        DropWatches(*pNode);

    ReleaseSRWLockExclusive(&m_lock);
}

bool Hive::IsFrozen() const noexcept
{
    return m_isFrozen.load(std::memory_order_acquire);
}

_Ret_notnull_ Node* Hive::Root() const noexcept
{
    return m_pRoot;
}

_Ret_maybenull_ Registry* Hive::Lower() const noexcept
{
    return m_pLower;
}

Hive::LowerView Hive::ViewOf(const Node& node) const
{
    LowerView view;
    if (!m_pLower)
        return view;

    std::vector<const Node*> chain;
    for (const auto* pNode = &node; pNode; pNode = pNode->pParent)
    {
        if (pNode->isOpaque)
            return view;
        if (pNode->pParent)
            chain.push_back(pNode);
    }

    view.isVisible = true;
    view.path = m_lowerPath;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        if (!view.path.empty())
            view.path += L'\\';
        view.path += (*it)->name;
    }

    return view;
}

LSTATUS Hive::OpenLower(
    const std::wstring& path,
    const REGSAM samDesired,
    HKEY& hKey
) const noexcept
{
    return m_pLower->RegOpenKeyExW(
        m_hLowerRoot,
        path.empty() ? nullptr : path.c_str(),
        0,
        samDesired | m_samLowerView,
        &hKey
    );
}

LSTATUS Hive::QueryLowerClass(
    const std::wstring& path,
    std::wstring& className
) const noexcept
try
{
    className.clear();
    LowerKey lowerKey(*m_pLower);
    const auto opened = OpenLower(path, KEY_QUERY_VALUE, lowerKey.Receive());
    if (opened != ERROR_SUCCESS)
        return opened == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : opened;

    std::wstring buffer(256, L'\0');
    for (;;)
    {
        DWORD cchClass = static_cast<DWORD>(buffer.size());
        const auto status = m_pLower->RegQueryInfoKeyW(
            lowerKey.Get(),
            buffer.data(),
            &cchClass,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        );
        if (status == ERROR_MORE_DATA || status == ERROR_INSUFFICIENT_BUFFER)
        {
            buffer.resize(buffer.size() * 2);
            continue;
        }
        if (status != ERROR_SUCCESS)
            return status;

        className.assign(buffer.data(), cchClass);
        return ERROR_SUCCESS;
    }
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::OpenKey(
    _In_ Node* const pParent,
    const KeyPath path,
    const REGSAM samDesired,
    OpenResult& result
) const noexcept
try
{
    Node* pCurrent = pParent;
    KeyPath rest;
    LowerView view;
    {
        ReadLock lock(*this);
        if (pParent->isDeleted)
            return ERROR_KEY_DELETED;

        std::size_t i = 0;
        for (; i < path.size(); ++i)
        {
            auto* const pChild = FindChild(*pCurrent, path[i]);
            if (!pChild)
                break;
            if (pChild->isTombstone)
                return ERROR_FILE_NOT_FOUND;

            pCurrent = pChild;
        }

        if (i == path.size())
        {
            result = {.pNode = pCurrent, .hLower = nullptr};
            return ERROR_SUCCESS;
        }

        view = ViewOf(*pCurrent);
        if (!view.isVisible)
            return ERROR_FILE_NOT_FOUND;

        rest = path.subspan(i);
    }

    // Nothing of the upper layer is at or below the key, so it is the lower
    // key's to answer.
    const auto lowerPath = JoinPath(std::move(view.path), rest);
    LowerKey lowerKey(*m_pLower);
    const REGSAM samLower =
        IsFrozen()
            ? samDesired
            : KEY_QUERY_VALUE | (samDesired & KEY_WOW64_RES);
    const auto status = OpenLower(lowerPath, samLower, lowerKey.Receive());
    if (status != ERROR_SUCCESS)
        return status;

    if (IsFrozen())
    {
        result = {.pNode = nullptr, .hLower = lowerKey.Release()};
        return ERROR_SUCCESS;
    }

    // Writable: every open inside the mount answers with a node, or a write
    // through a lower handle handed out now would miss a later copy-up.
    WriteLock lock(*this);
    if (!lock)
        return OpenKey(pParent, path, samDesired, result);

    if (pCurrent->isDeleted)
        return ERROR_KEY_DELETED;

    for (const auto name : rest)
    {
        if (auto* const pChild = FindChild(*pCurrent, name))
        {
            if (pChild->isTombstone)
                return ERROR_FILE_NOT_FOUND;

            pCurrent = pChild;
            continue;
        }

        pCurrent = AddChild(*pCurrent, name, {}, false);
        if (!pCurrent)
            return ERROR_OUTOFMEMORY;

        pCurrent->isTransparent = true;
        pCurrent->isLowerBacked = true;
    }

    result = {.pNode = pCurrent, .hLower = nullptr};
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::CreateKey(
    _Inout_ Node* const pParent,
    const KeyPath path,
    _In_z_ const LPCWSTR pszClassName,
    const DWORD dwOptions,
    const REGSAM samDesired,
    OpenResult& result,
    DWORD& dwDisposition
) noexcept
try
{
    const std::wstring_view className(pszClassName);
    if (IsFrozen())
    {
        const auto status = OpenKey(pParent, path, samDesired, result);
        if (status == ERROR_FILE_NOT_FOUND)
            return ERROR_ACCESS_DENIED;

        dwDisposition = REG_OPENED_EXISTING_KEY;
        return status;
    }

    // How far the path exists: in the upper layer first, then below it.
    std::size_t cUpper = 0;
    bool isBehindTombstone = false;
    LowerView view;
    {
        ReadLock lock(*this);
        if (pParent->isDeleted)
            return ERROR_KEY_DELETED;

        const Node* pCurrent = pParent;
        for (; cUpper < path.size(); ++cUpper)
        {
            const auto* const pChild = FindChild(*pCurrent, path[cUpper]);
            if (!pChild)
                break;
            if (pChild->isTombstone)
            {
                isBehindTombstone = true;
                break;
            }

            pCurrent = pChild;
        }

        if (cUpper < path.size() && !isBehindTombstone)
            view = ViewOf(*pCurrent);
    }

    std::size_t cLower = cUpper;
    if (view.isVisible)
    {
        for (; cLower < path.size(); ++cLower)
        {
            LowerKey lowerKey(*m_pLower);
            const auto status = OpenLower(
                JoinPath(view.path, path.subspan(cUpper, cLower + 1 - cUpper)),
                KEY_QUERY_VALUE,
                lowerKey.Receive()
            );
            if (status == ERROR_FILE_NOT_FOUND)
                break;
            if (!Exists(status))
                return status;
        }
    }

    const bool isVolatile = (dwOptions & REG_OPTION_VOLATILE) != 0;
    WriteLock lock(*this);
    if (!lock)
    {
        return CreateKey(
            pParent,
            path,
            pszClassName,
            dwOptions,
            samDesired,
            result,
            dwDisposition
        );
    }

    if (pParent->isDeleted)
        return ERROR_KEY_DELETED;

    auto* pCurrent = pParent;
    dwDisposition = REG_OPENED_EXISTING_KEY;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        const bool isLast = i + 1 == path.size();
        auto* const pChild = FindChild(*pCurrent, path[i]);
        if (pChild && !pChild->isTombstone)
        {
            pCurrent = pChild;
            continue;
        }

        if (pChild)
        {
            // Created again over a deleted lower key: nothing below shows.
            pChild->isTombstone = false;
            pChild->isOpaque = true;
            pChild->className = isLast ? className : std::wstring_view();
            Touch(*pChild);
            Materialize(*pCurrent);
            Notify(*pCurrent, REG_NOTIFY_CHANGE_NAME);
            pCurrent = pChild;
            if (isLast)
                dwDisposition = REG_CREATED_NEW_KEY;
            continue;
        }

        const bool isInLower = i >= cUpper && i < cLower;
        if (!isInLower && pCurrent->isVolatile && !isVolatile)
            return ERROR_CHILD_MUST_BE_VOLATILE;

        auto* const pNew = AddChild(
            *pCurrent,
            path[i],
            isLast && !isInLower ? className : std::wstring_view(),
            !isInLower && isVolatile
        );
        if (!pNew)
            return ERROR_OUTOFMEMORY;

        if (isInLower)
        {
            pNew->isTransparent = true;
            pNew->isLowerBacked = true;
        }
        else
        {
            Materialize(*pCurrent);
            Notify(*pCurrent, REG_NOTIFY_CHANGE_NAME);
            if (isLast)
                dwDisposition = REG_CREATED_NEW_KEY;
        }

        pCurrent = pNew;
    }

    result = {.pNode = pCurrent, .hLower = nullptr};
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::DeleteKey(
    _Inout_ Node* const pParent,
    const KeyPath path
) noexcept
try
{
    if (IsFrozen())
        return ERROR_ACCESS_DENIED;

    OpenResult opened;
    const auto located = OpenKey(pParent, path, KEY_READ, opened);
    if (located != ERROR_SUCCESS)
        return located;

    // Frozen since the check above, so the open answered with a lower key.
    if (!opened.pNode)
    {
        LowerKey lowerKey(*m_pLower);
        lowerKey.Receive() = opened.hLower;
        return ERROR_ACCESS_DENIED;
    }

    auto* const pNode = opened.pNode;
    std::vector<std::wstring> tombstones;
    LowerView view;
    LowerView parentView;
    std::wstring keyName;
    {
        ReadLock lock(*this);
        if (pNode->isDeleted)
            return ERROR_KEY_DELETED;
        if (!pNode->pParent)
            return ERROR_ACCESS_DENIED;

        for (const auto* const pChild : pNode->children)
        {
            if (!pChild->isTombstone)
                return ERROR_ACCESS_DENIED;

            tombstones.push_back(pChild->name);
        }

        view = ViewOf(*pNode);
        parentView = ViewOf(*pNode->pParent);
        keyName = pNode->name;
    }

    // Whether a lower key sits at the node's path decides if a tombstone must
    // replace the node, even when the node is opaque and hides what is below
    // it. A lower subkey that no tombstone hides still makes the key non-empty.
    bool hasLower = false;
    if (parentView.isVisible)
    {
        LowerKey lowerKey(*m_pLower);
        const auto lowerPath = JoinPath(
            parentView.path,
            std::array<std::wstring_view, 1>{keyName}
        );
        const auto status = OpenLower(
            lowerPath,
            KEY_ENUMERATE_SUB_KEYS,
            lowerKey.Receive()
        );
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
            return status;

        hasLower = status == ERROR_SUCCESS;
        for (DWORD i = 0; hasLower && view.isVisible; ++i)
        {
            std::wstring name;
            const auto enumStatus = VReg::EnumKey(
                *m_pLower,
                lowerKey.Get(),
                i,
                name
            );
            if (enumStatus == ERROR_NO_MORE_ITEMS)
                break;
            if (enumStatus != ERROR_SUCCESS)
                return enumStatus;
            if (!ContainsName(tombstones, name))
                return ERROR_ACCESS_DENIED;
        }
    }

    WriteLock lock(*this);
    if (!lock)
        return ERROR_ACCESS_DENIED;
    if (pNode->isDeleted)
        return ERROR_KEY_DELETED;
    if (
        std::ranges::any_of(
            pNode->children,
            [](const Node* pChild) { return !pChild->isTombstone; }
        )
    )
        return ERROR_ACCESS_DENIED;

    auto& parent = *pNode->pParent;
    const std::wstring name = pNode->name;
    Remove(*pNode);
    if (hasLower)
    {
        auto* const pTombstone = AddChild(parent, name, {}, false);
        if (!pTombstone)
            return ERROR_OUTOFMEMORY;

        pTombstone->isTombstone = true;
    }

    Materialize(parent);
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::DeleteTree(
    _Inout_ Node* const pNode
) noexcept
{
    WriteLock lock(*this);
    if (!lock)
        return ERROR_ACCESS_DENIED;
    if (pNode->isDeleted)
        return ERROR_KEY_DELETED;

    while (!pNode->children.empty())
        Remove(*pNode->children.back());

    pNode->values.clear();
    pNode->isOpaque = m_pLower != nullptr;
    pNode->isTransparent = false;
    pNode->isLowerBacked = false;
    Materialize(*pNode);
    Touch(*pNode);
    Notify(*pNode, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET);
    return ERROR_SUCCESS;
}

LSTATUS Hive::QueryValue(
    _In_ Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    DWORD& dwType,
    DWORD& cbData,
    _Out_opt_ std::vector<std::byte>* const pData
) const noexcept
try
{
    const std::wstring_view name(pszName);
    LowerView view;
    {
        ReadLock lock(*this);
        if (pNode->isDeleted)
            return ERROR_KEY_DELETED;

        if (const auto* const pValue = FindValue(*pNode, name))
        {
            if (pValue->isTombstone)
                return ERROR_FILE_NOT_FOUND;

            dwType = pValue->dwType;
            cbData = static_cast<DWORD>(pValue->data.size());
            if (pData)
                *pData = pValue->data;
            return ERROR_SUCCESS;
        }

        view = ViewOf(*pNode);
    }

    if (!view.isVisible)
        return ERROR_FILE_NOT_FOUND;

    LowerKey lowerKey(*m_pLower);
    const auto status = OpenLower(
        view.path,
        KEY_QUERY_VALUE,
        lowerKey.Receive()
    );
    if (status != ERROR_SUCCESS)
        return status;

    const std::wstring valueName(name);
    if (!pData)
    {
        cbData = 0;
        return m_pLower->RegQueryValueExW(
            lowerKey.Get(),
            valueName.c_str(),
            nullptr,
            &dwType,
            nullptr,
            &cbData
        );
    }

    const auto queried = VReg::QueryValue(
        *m_pLower,
        lowerKey.Get(),
        valueName.c_str(),
        &dwType,
        *pData
    );
    if (queried == ERROR_SUCCESS)
        cbData = static_cast<DWORD>(pData->size());
    return queried;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::SetValue(
    _Inout_ Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    const DWORD dwType,
    const std::span<const std::byte> data
) noexcept
{
    const std::wstring_view name(pszName);
    return StoreValue(pNode, name, dwType, data, false);
}

LSTATUS Hive::SetMuiTextValue(
    _Inout_ Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _In_z_ const LPCWSTR pszText
) noexcept
try
{
    const std::wstring_view name(pszName);
    const std::wstring_view text(pszText);
    return StoreValue(pNode, name, REG_SZ, StringData(text), true);
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::QueryMuiText(
    _In_ Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    std::wstring& text
) const noexcept
try
{
    const std::wstring_view name(pszName);
    {
        ReadLock lock(*this);
        if (pNode->isDeleted)
            return ERROR_KEY_DELETED;

        if (const auto* const pValue = FindValue(*pNode, name))
            return pValue->isTombstone
                       ? ERROR_FILE_NOT_FOUND
                       : MuiTextOf(*pValue, text);
    }

    DWORD dwType{};
    DWORD cbData = 0;
    const auto status = QueryValue(pNode, pszName, dwType, cbData, nullptr);
    return status == ERROR_SUCCESS ? ERROR_INVALID_DATA : status;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::StoreValue(
    Node* const pNode,
    const std::wstring_view name,
    const DWORD dwType,
    const std::span<const std::byte> data,
    const bool isMuiText
) noexcept
try
{
    std::vector<std::byte> bytes(data.begin(), data.end());
    WriteLock lock(*this);
    if (!lock)
        return ERROR_ACCESS_DENIED;
    if (pNode->isDeleted)
        return ERROR_KEY_DELETED;

    if (auto* const pValue = FindValue(*pNode, name))
        *pValue = {
            .name = std::move(pValue->name),
            .dwType = dwType,
            .data = std::move(bytes),
            .isTombstone = false,
            .isMuiText = isMuiText
        };
    else
        pNode->values.push_back(
            {
                .name = std::wstring(name),
                .dwType = dwType,
                .data = std::move(bytes),
                .isMuiText = isMuiText
            }
        );

    Materialize(*pNode);
    Touch(*pNode);
    Notify(*pNode, REG_NOTIFY_CHANGE_LAST_SET);
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::DeleteValue(
    _Inout_ Node* const pNode,
    _In_z_ const LPCWSTR pszName
) noexcept
try
{
    const std::wstring_view name(pszName);
    if (IsFrozen())
        return ERROR_ACCESS_DENIED;

    bool isInUpper = false;
    LowerView view;
    {
        ReadLock lock(*this);
        if (pNode->isDeleted)
            return ERROR_KEY_DELETED;

        if (const auto* const pValue = FindValue(*pNode, name))
        {
            if (pValue->isTombstone)
                return ERROR_FILE_NOT_FOUND;

            isInUpper = true;
        }

        view = ViewOf(*pNode);
    }

    bool isInLower = false;
    if (view.isVisible)
    {
        LowerKey lowerKey(*m_pLower);
        const auto status = OpenLower(
            view.path,
            KEY_QUERY_VALUE,
            lowerKey.Receive()
        );
        if (status == ERROR_SUCCESS)
        {
            const std::wstring valueName(name);
            const auto queryStatus = m_pLower->RegQueryValueExW(
                lowerKey.Get(),
                valueName.c_str(),
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );
            if (queryStatus != ERROR_FILE_NOT_FOUND && !Exists(queryStatus))
                return queryStatus;

            isInLower = Exists(queryStatus);
        }
        else if (status != ERROR_FILE_NOT_FOUND)
        {
            return status;
        }
    }

    if (!isInUpper && !isInLower)
        return ERROR_FILE_NOT_FOUND;

    WriteLock lock(*this);
    if (!lock)
        return ERROR_ACCESS_DENIED;
    if (pNode->isDeleted)
        return ERROR_KEY_DELETED;

    auto* const pValue = FindValue(*pNode, name);
    if (isInLower)
    {
        if (pValue)
            *pValue = {
                .name = std::move(pValue->name),
                .dwType = REG_NONE,
                .data = {},
                .isTombstone = true
            };
        else
            pNode->values.push_back(
                {
                    .name = std::wstring(name),
                    .dwType = REG_NONE,
                    .data = {},
                    .isTombstone = true
                }
            );
    }
    else if (pValue)
    {
        pNode->values.erase(
            pNode->values.begin() + (pValue - pNode->values.data())
        );
    }

    Materialize(*pNode);
    Touch(*pNode);
    Notify(*pNode, REG_NOTIFY_CHANGE_LAST_SET);
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::EnumKey(
    _In_ Node* const pNode,
    const DWORD dwIndex,
    std::wstring& name,
    std::wstring& className,
    FILETIME& lastWrite
) const noexcept
try
{
    std::vector<std::wstring> shadowed;
    DWORD cUpper = 0;
    LowerView view;
    LowerView classView;
    {
        ReadLock lock(*this);
        if (pNode->isDeleted)
            return ERROR_KEY_DELETED;

        for (const auto* const pChild : pNode->children)
        {
            // A transparent child stands for a lower key; it is listed with
            // the lower ones.
            if (pChild->isTransparent)
                continue;

            if (!pChild->isTombstone && cUpper++ == dwIndex)
            {
                name = pChild->name;
                className = pChild->className;
                lastWrite = pChild->lastWrite;
                if (!pChild->isLowerBacked)
                    return ERROR_SUCCESS;

                classView = ViewOf(*pChild);
                if (!classView.isVisible)
                    return ERROR_SUCCESS;

                break;
            }

            shadowed.push_back(pChild->name);
        }

        if (!classView.isVisible)
            view = ViewOf(*pNode);
    }

    // An upper key that was made from a lower one has no class of its own.
    if (classView.isVisible)
        return QueryLowerClass(classView.path, className);

    if (!view.isVisible)
        return ERROR_NO_MORE_ITEMS;

    LowerKey lowerKey(*m_pLower);
    const auto opened = OpenLower(
        view.path,
        KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE,
        lowerKey.Receive()
    );
    if (opened != ERROR_SUCCESS)
        return opened == ERROR_FILE_NOT_FOUND ? ERROR_NO_MORE_ITEMS : opened;

    DWORD cchMaxClass = 0;
    const auto queried = m_pLower->RegQueryInfoKeyW(
        lowerKey.Get(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &cchMaxClass,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
    if (queried != ERROR_SUCCESS)
        return queried;

    // The children were listed in name order, so the shadow list is sorted.
    auto dwSkip = dwIndex - cUpper;
    std::wstring nameBuffer(256, L'\0');
    std::wstring classBuffer(cchMaxClass + 1, L'\0');
    for (DWORD i = 0;; ++i)
    {
        DWORD cchName = static_cast<DWORD>(nameBuffer.size());
        DWORD cchClass = static_cast<DWORD>(classBuffer.size());
        FILETIME ft{};
        const auto status = m_pLower->RegEnumKeyExW(
            lowerKey.Get(),
            i,
            nameBuffer.data(),
            &cchName,
            nullptr,
            classBuffer.data(),
            &cchClass,
            &ft
        );
        if (status != ERROR_SUCCESS)
            return status;

        const std::wstring_view lowerName(nameBuffer.data(), cchName);
        if (ContainsName(shadowed, lowerName) || dwSkip-- != 0)
            continue;

        name = lowerName;
        className.assign(classBuffer.data(), cchClass);
        lastWrite = ft;
        return ERROR_SUCCESS;
    }
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::EnumValue(
    _In_ Node* const pNode,
    const DWORD dwIndex,
    std::wstring& name,
    DWORD& dwType,
    DWORD& cbData,
    _Out_opt_ std::vector<std::byte>* const pData
) const noexcept
try
{
    std::vector<std::wstring> shadowed;
    DWORD cUpper = 0;
    LowerView view;
    {
        ReadLock lock(*this);
        if (pNode->isDeleted)
            return ERROR_KEY_DELETED;

        for (const auto& value : pNode->values)
        {
            if (!value.isTombstone && cUpper++ == dwIndex)
            {
                name = value.name;
                dwType = value.dwType;
                cbData = static_cast<DWORD>(value.data.size());
                if (pData)
                    *pData = value.data;
                return ERROR_SUCCESS;
            }

            shadowed.push_back(value.name);
        }

        view = ViewOf(*pNode);
    }

    if (!view.isVisible)
        return ERROR_NO_MORE_ITEMS;

    LowerKey lowerKey(*m_pLower);
    const auto opened = OpenLower(
        view.path,
        KEY_QUERY_VALUE,
        lowerKey.Receive()
    );
    if (opened != ERROR_SUCCESS)
        return opened == ERROR_FILE_NOT_FOUND ? ERROR_NO_MORE_ITEMS : opened;

    std::ranges::sort(shadowed, NameLess);
    auto dwSkip = dwIndex - cUpper;
    for (DWORD i = 0;; ++i)
    {
        std::wstring lowerName;
        DWORD dwLowerType{};
        const auto status = VReg::EnumValue(
            *m_pLower,
            lowerKey.Get(),
            i,
            lowerName,
            &dwLowerType,
            nullptr
        );
        if (status != ERROR_SUCCESS)
            return status;

        if (ContainsName(shadowed, lowerName) || dwSkip-- != 0)
            continue;

        name = std::move(lowerName);
        dwType = dwLowerType;
        if (pData)
        {
            // The scan fetched no data; enumerate the matching entry again
            // with it.
            std::wstring matchName;
            DWORD dwMatchType{};
            const auto fetched = VReg::EnumValue(
                *m_pLower,
                lowerKey.Get(),
                i,
                matchName,
                &dwMatchType,
                pData
            );
            if (fetched != ERROR_SUCCESS)
                return fetched;

            // Name, type and data all from the one call, should the entry
            // have changed since the scan.
            name = std::move(matchName);
            dwType = dwMatchType;
            cbData = static_cast<DWORD>(pData->size());
            return ERROR_SUCCESS;
        }

        std::wstring sizeName(name.size() + 1, wchar_t{});
        DWORD cchName = static_cast<DWORD>(sizeName.size());
        cbData = 0;
        return m_pLower->RegEnumValueW(
            lowerKey.Get(),
            i,
            sizeName.data(),
            &cchName,
            nullptr,
            nullptr,
            nullptr,
            &cbData
        );
    }
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::QueryInfo(
    _In_ Node* const pNode,
    KeyInfo& info
) const noexcept
try
{
    std::vector<std::wstring> shadowedKeys;
    std::vector<std::wstring> shadowedValues;
    bool isTransparent = false;
    bool isLowerBacked = false;
    LowerView view;
    KeyInfo result;
    {
        ReadLock lock(*this);
        if (pNode->isDeleted)
            return ERROR_KEY_DELETED;

        for (const auto* const pChild : pNode->children)
        {
            if (pChild->isTransparent)
                continue;

            shadowedKeys.push_back(pChild->name);
            if (pChild->isTombstone)
                continue;

            ++result.cSubKeys;
            result.cchMaxSubKey = (std::max)(
                result.cchMaxSubKey,
                static_cast<DWORD>(pChild->name.size())
            );
            result.cchMaxClass = (std::max)(
                result.cchMaxClass,
                static_cast<DWORD>(pChild->className.size())
            );
        }

        for (const auto& value : pNode->values)
        {
            shadowedValues.push_back(value.name);
            if (value.isTombstone)
                continue;

            ++result.cValues;
            result.cchMaxValueName = (std::max)(
                result.cchMaxValueName,
                static_cast<DWORD>(value.name.size())
            );
            result.cbMaxValue = (std::max)(
                result.cbMaxValue,
                static_cast<DWORD>(value.data.size())
            );
        }

        isTransparent = pNode->isTransparent;
        isLowerBacked = pNode->isLowerBacked;
        result.className = pNode->className;
        result.lastWrite = pNode->lastWrite;
        view = ViewOf(*pNode);
    }

    if (view.isVisible)
    {
        LowerKey lowerKey(*m_pLower);
        const auto status = OpenLower(
            view.path,
            KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS,
            lowerKey.Receive()
        );
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
            return status;

        if (status == ERROR_SUCCESS)
        {
            KeyInfo lower;
            std::wstring classBuffer(256, L'\0');
            for (;;)
            {
                DWORD cchClass = static_cast<DWORD>(classBuffer.size());
                const auto infoStatus = m_pLower->RegQueryInfoKeyW(
                    lowerKey.Get(),
                    classBuffer.data(),
                    &cchClass,
                    nullptr,
                    &lower.cSubKeys,
                    &lower.cchMaxSubKey,
                    &lower.cchMaxClass,
                    &lower.cValues,
                    &lower.cchMaxValueName,
                    &lower.cbMaxValue,
                    nullptr,
                    &lower.lastWrite
                );
                if (
                    infoStatus == ERROR_MORE_DATA
                    || infoStatus == ERROR_INSUFFICIENT_BUFFER
                )
                {
                    classBuffer.resize(classBuffer.size() * 2);
                    continue;
                }
                if (infoStatus != ERROR_SUCCESS)
                    return infoStatus;

                lower.className.assign(classBuffer.data(), cchClass);
                break;
            }

            // An upper entry that shadows a lower one is counted once.
            for (const auto& name : shadowedKeys)
            {
                LowerKey child(*m_pLower);
                const auto childStatus = m_pLower->RegOpenKeyExW(
                    lowerKey.Get(),
                    name.c_str(),
                    0,
                    KEY_QUERY_VALUE | m_samLowerView,
                    &child.Receive()
                );
                if (childStatus != ERROR_FILE_NOT_FOUND && !Exists(childStatus))
                    return childStatus;
                if (Exists(childStatus))
                    --lower.cSubKeys;
            }

            for (const auto& name : shadowedValues)
            {
                const auto valueStatus = m_pLower->RegQueryValueExW(
                    lowerKey.Get(),
                    name.c_str(),
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr
                );
                if (valueStatus != ERROR_FILE_NOT_FOUND && !Exists(valueStatus))
                    return valueStatus;
                if (Exists(valueStatus))
                    --lower.cValues;
            }

            result.cSubKeys += lower.cSubKeys;
            result.cValues += lower.cValues;
            result.cchMaxSubKey = (std::max)(
                result.cchMaxSubKey,
                lower.cchMaxSubKey
            );
            result.cchMaxClass = (std::max)(
                result.cchMaxClass,
                lower.cchMaxClass
            );
            result.cchMaxValueName = (std::max)(
                result.cchMaxValueName,
                lower.cchMaxValueName
            );
            result.cbMaxValue = (std::max)(result.cbMaxValue, lower.cbMaxValue);
            if (isLowerBacked)
                result.className = std::move(lower.className);
            if (isTransparent)
                result.lastWrite = lower.lastWrite;
        }
    }

    info = std::move(result);
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::SnapshotUpper(
    std::vector<UpperRecord>& records
) const noexcept
try
{
    std::vector<UpperRecord> result;
    std::wstring path;
    {
        ReadLock lock(*this);
        ListUpper(*m_pRoot, path, result);
    }

    records = std::move(result);
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::CheckKey(
    _In_ Node* const pNode
) const noexcept
{
    ReadLock lock(*this);
    return pNode->isDeleted ? ERROR_KEY_DELETED : ERROR_SUCCESS;
}

LSTATUS Hive::Watch(
    _In_ Node* const pNode,
    const BOOL bWatchSubtree,
    const DWORD dwFilter,
    const HANDLE hEvent
) const noexcept
{
    if (!(dwFilter & (REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET)))
        return ERROR_SUCCESS;

    HANDLE hOwn{};
    if (
        !DuplicateHandle(
            GetCurrentProcess(),
            hEvent,
            GetCurrentProcess(),
            &hOwn,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS
        )
    )
        return GetLastError();

    WriteLock lock(*this);
    LSTATUS status = ERROR_SUCCESS;
    if (pNode->isDeleted)
        status = ERROR_KEY_DELETED;
    else if (lock)
    {
        try
        {
            pNode->watches.push_back(
                {
                    .hEvent = hOwn,
                    .isSubtree = bWatchSubtree != FALSE,
                    .dwFilter = dwFilter,
                }
            );
            return ERROR_SUCCESS;
        }
        catch (const std::bad_alloc&)
        {
            status = ERROR_OUTOFMEMORY;
        }
    }

    CloseHandle(hOwn);
    return status;
}

LSTATUS Hive::OpenLowerValueKey(
    _In_ Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    HKEY& hLower
) const noexcept
try
{
    const std::wstring_view name(pszName);
    hLower = nullptr;
    LowerView view;
    {
        ReadLock lock(*this);
        if (pNode->isDeleted)
            return ERROR_KEY_DELETED;
        if (FindValue(*pNode, name))
            return ERROR_SUCCESS;

        view = ViewOf(*pNode);
    }

    if (!view.isVisible)
        return ERROR_SUCCESS;

    LowerKey lowerKey(*m_pLower);
    const auto opened = OpenLower(
        view.path,
        KEY_QUERY_VALUE,
        lowerKey.Receive()
    );
    if (opened != ERROR_SUCCESS)
        return opened == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : opened;

    const std::wstring valueName(name);
    const auto status = m_pLower->RegQueryValueExW(
        lowerKey.Get(),
        valueName.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
    if (status == ERROR_FILE_NOT_FOUND)
        return ERROR_SUCCESS;
    if (!Exists(status))
        return status;

    hLower = lowerKey.Release();
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

Hive::ReadLock::ReadLock(const Hive& hive) noexcept :
    m_pLock(hive.IsFrozen() ? nullptr : &hive.m_lock)
{
    if (m_pLock)
        AcquireSRWLockShared(m_pLock);
}

Hive::ReadLock::~ReadLock()
{
    if (m_pLock)
        ReleaseSRWLockShared(m_pLock);
}

Hive::WriteLock::WriteLock(const Hive& hive) noexcept :
    m_pLock(&hive.m_lock)
{
    AcquireSRWLockExclusive(m_pLock);
    if (hive.IsFrozen())
    {
        ReleaseSRWLockExclusive(m_pLock);
        m_pLock = nullptr;
    }
}

Hive::WriteLock::~WriteLock()
{
    if (m_pLock)
        ReleaseSRWLockExclusive(m_pLock);
}

Hive::WriteLock::operator bool() const noexcept
{
    return m_pLock != nullptr;
}

Node* Hive::FindChild(const Node& parent, const std::wstring_view name) noexcept
{
    const auto it = std::ranges::lower_bound(
        parent.children,
        name,
        NameLess,
        [](const Node* const pChild) { return std::wstring_view(pChild->name); }
    );
    if (it == parent.children.end() || CompareNames((*it)->name, name) != 0)
        return nullptr;

    return *it;
}

Value* Hive::FindValue(Node& node, const std::wstring_view name) noexcept
{
    const auto it = std::ranges::find_if(
        node.values,
        [&](const Value& value) { return CompareNames(value.name, name) == 0; }
    );
    return it == node.values.end() ? nullptr : &*it;
}

Node* Hive::AddChild(
    Node& parent,
    const std::wstring_view name,
    const std::wstring_view className,
    const bool isVolatile
) const
{
    auto pNode = std::make_unique<Node>();
    pNode->pHive = const_cast<Hive*>(this);
    pNode->pParent = &parent;
    pNode->name = name;
    pNode->className = className;
    pNode->isVolatile = isVolatile;
    Touch(*pNode);

    const auto position = std::ranges::lower_bound(
            parent.children,
            name,
            NameLess,
            [](const Node* const pChild)
            {
                return std::wstring_view(pChild->name);
            }
        )
        - parent.children.begin();

    // Every allocation happens before the handle is bound, so a throw leaves
    // no handle pointing at a freed node.
    m_nodes.reserve(m_nodes.size() + 1);
    parent.children.reserve(parent.children.size() + 1);

    pNode->hKey = HandleSpace::Process().Allocate(pNode.get());
    if (!pNode->hKey)
        return nullptr;

    auto* const pResult = pNode.get();
    parent.children.insert(parent.children.begin() + position, pResult);
    m_nodes.push_back(std::move(pNode));
    Touch(parent);
    return pResult;
}

void Hive::Remove(Node& node) noexcept
{
    if (node.pParent)
    {
        auto& siblings = node.pParent->children;
        siblings.erase(std::ranges::find(siblings, &node));
        Touch(*node.pParent);
    }

    // Children of a deleted key answer as deleted too.
    auto mark = [](auto& self, Node& current) -> void
    {
        current.isDeleted = true;
        Fire(current, ~DWORD{0}, false);
        for (auto* const pChild : current.children)
            self(self, *pChild);
    };
    mark(mark, node);
    if (node.pParent)
        Notify(*node.pParent, REG_NOTIFY_CHANGE_NAME);
}

void Hive::Notify(Node& node, const DWORD dwFilter) noexcept
{
    Fire(node, dwFilter, false);
    for (
        auto* pAncestor = node.pParent;
        pAncestor;
        pAncestor = pAncestor->pParent
    )
        Fire(*pAncestor, dwFilter, true);
}

void Hive::Touch(Node& node) noexcept
{
    GetSystemTimeAsFileTime(&node.lastWrite);
}
} // namespace VReg
