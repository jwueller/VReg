#pragma once

#include <VReg/hive.h>

#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <memory>
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
    /// Overlay only: hides the lower value of this name.
    bool isTombstone = false;
};

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

    /// Overlay only: the key exists because the lower key at its path does;
    /// the node is just the way to it. Writing to it makes it an upper key.
    bool isTransparent = false;
    /// Overlay only: hides the lower key of this name; never handed out.
    bool isTombstone = false;
    /// Overlay only: hides every lower subkey and value (a key deleted and
    /// created again, or emptied by `RegDeleteTree`).
    bool isOpaque = false;
    /// Overlay only: the lower key at this path exists and its class shows
    /// through (the node was made from it, not created over it).
    bool isLowerBacked = false;

    /// Sorted by @ref CompareNames, the order the native registry enumerates
    /// subkeys in.
    std::vector<Node*> children;
    /// In creation order, as the native registry enumerates values.
    std::vector<Value> values;
};

/// Ordinal comparison of uppercased names, the registry's own collation:
/// negative, zero or positive.
[[nodiscard]] int CompareNames(std::wstring_view left, std::wstring_view right) noexcept;

/**
 * @brief The node storage of a hive: the nodes, their handles and the lock.
 *
 *  Nodes are created here and freed only with the tree; a deleted node stays
 *  allocated (marked deleted) so its handle keeps answering
 *  `ERROR_KEY_DELETED`. Destroying the tree unbinds every handle it issued.
 *
 *  Readers take the lock shared and writers exclusive, until @ref Freeze;
 *  after that nothing writes and readers take no lock at all.
 */
class Tree
{
public:
    /// Creates the root. Throws `std::bad_alloc`.
    explicit Tree(Hive& owner);
    ~Tree();

    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;

    [[nodiscard]] Node* Root() const noexcept;

    void Freeze() noexcept;
    [[nodiscard]] bool IsFrozen() const noexcept;

    /// A shared lock, or nothing once frozen.
    class ReadLock
    {
    public:
        explicit ReadLock(Tree& tree) noexcept;
        ~ReadLock();
        ReadLock(const ReadLock&) = delete;
        ReadLock& operator=(const ReadLock&) = delete;

    private:
        SRWLOCK* m_pLock;
    };

    class WriteLock
    {
    public:
        explicit WriteLock(Tree& tree) noexcept;
        ~WriteLock();
        WriteLock(const WriteLock&) = delete;
        WriteLock& operator=(const WriteLock&) = delete;

    private:
        SRWLOCK& m_lock;
    };

    [[nodiscard]] static Node* FindChild(const Node& parent, std::wstring_view name) noexcept;
    [[nodiscard]] static Value* FindValue(Node& node, std::wstring_view name) noexcept;

    /// A new child of @p parent, with its handle. Throws `std::bad_alloc`, and
    /// answers null when the handle namespace is exhausted.
    [[nodiscard]] Node* AddChild(Node& parent, std::wstring_view name, std::wstring_view className, bool isVolatile);

    /// Marks @p node and everything below it deleted and unlinks it from its
    /// parent.
    static void Remove(Node& node) noexcept;

    /// Stamps @p node as written now.
    static void Touch(Node& node) noexcept;

private:
    Hive& m_owner;
    SRWLOCK m_lock = SRWLOCK_INIT;
    std::atomic<bool> m_isFrozen = false;
    std::vector<std::unique_ptr<Node>> m_nodes;
};
} // namespace VReg
