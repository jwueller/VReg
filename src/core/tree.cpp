#include "tree.h"

#include "handle_space.h"

#include <algorithm>
#include <new>

namespace VReg
{
int CompareNames(const std::wstring_view left, const std::wstring_view right) noexcept
{
    if (left.empty() || right.empty())
        return left.empty() == right.empty() ? 0 : (left.empty() ? -1 : 1);

    return CompareStringOrdinal(
               left.data(),
               static_cast<int>(left.size()),
               right.data(),
               static_cast<int>(right.size()),
               TRUE
           )
           - CSTR_EQUAL;
}

HKEY Hive::HandleOf(const Node* const pNode) noexcept
{
    return pNode->hKey;
}

Node* Hive::NodeOf(const HKEY hKey) noexcept
{
    return static_cast<Node*>(HandleSpace::Process().Find(hKey));
}

Hive* Hive::HiveOf(const Node* const pNode) noexcept
{
    return pNode->pHive;
}

Registry* Hive::Lower() noexcept
{
    return nullptr;
}

LSTATUS Hive::OpenLowerValueKey(Node* const pNode, const std::wstring_view name, HKEY& hLower) noexcept
{
    (void)pNode;
    (void)name;
    hLower = nullptr;
    return ERROR_SUCCESS;
}

Tree::Tree(Hive& owner)
    : m_owner(owner)
{
    auto pRoot = std::make_unique<Node>();
    pRoot->pHive = &owner;
    pRoot->hKey = HandleSpace::Process().Allocate(pRoot.get());
    if (!pRoot->hKey)
        throw std::bad_alloc();

    Touch(*pRoot);
    m_nodes.push_back(std::move(pRoot));
}

Tree::~Tree()
{
    for (const auto& pNode : m_nodes)
        HandleSpace::Process().Release(pNode->hKey);
}

Node* Tree::Root() const noexcept
{
    return m_nodes.front().get();
}

void Tree::Freeze() noexcept
{
    // Taking the lock once more waits out every writer still inside; readers
    // that see the flag afterwards skip the lock and see all their writes.
    AcquireSRWLockExclusive(&m_lock);
    m_isFrozen.store(true, std::memory_order_release);
    ReleaseSRWLockExclusive(&m_lock);
}

bool Tree::IsFrozen() const noexcept
{
    return m_isFrozen.load(std::memory_order_acquire);
}

Tree::ReadLock::ReadLock(Tree& tree) noexcept
    : m_pLock(tree.IsFrozen() ? nullptr : &tree.m_lock)
{
    if (m_pLock)
        AcquireSRWLockShared(m_pLock);
}

Tree::ReadLock::~ReadLock()
{
    if (m_pLock)
        ReleaseSRWLockShared(m_pLock);
}

Tree::WriteLock::WriteLock(Tree& tree) noexcept
    : m_lock(tree.m_lock)
{
    AcquireSRWLockExclusive(&m_lock);
}

Tree::WriteLock::~WriteLock()
{
    ReleaseSRWLockExclusive(&m_lock);
}

Node* Tree::FindChild(const Node& parent, const std::wstring_view name) noexcept
{
    const auto it = std::ranges::lower_bound(
        parent.children,
        name,
        [](const std::wstring_view left, const std::wstring_view right) { return CompareNames(left, right) < 0; },
        [](const Node* const pChild) { return std::wstring_view(pChild->name); }
    );
    if (it == parent.children.end() || CompareNames((*it)->name, name) != 0)
        return nullptr;

    return *it;
}

Value* Tree::FindValue(Node& node, const std::wstring_view name) noexcept
{
    const auto it = std::ranges::find_if(node.values, [&](const Value& value) { return CompareNames(value.name, name) == 0; });
    return it == node.values.end() ? nullptr : &*it;
}

Node* Tree::AddChild(Node& parent, const std::wstring_view name, const std::wstring_view className, const bool isVolatile)
{
    auto pNode = std::make_unique<Node>();
    pNode->pHive = &m_owner;
    pNode->pParent = &parent;
    pNode->name = name;
    pNode->className = className;
    pNode->isVolatile = isVolatile;
    Touch(*pNode);

    const auto position = std::ranges::lower_bound(
                              parent.children,
                              name,
                              [](const std::wstring_view left, const std::wstring_view right)
                              { return CompareNames(left, right) < 0; },
                              [](const Node* const pChild) { return std::wstring_view(pChild->name); }
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

void Tree::Remove(Node& node) noexcept
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
        for (auto* const pChild : current.children)
            self(self, *pChild);
    };
    mark(mark, node);
}

void Tree::Touch(Node& node) noexcept
{
    GetSystemTimeAsFileTime(&node.lastWrite);
}
} // namespace VReg
