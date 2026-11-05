#pragma once

#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <memory>
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

/// A value of a key; opaque like @ref Node.
struct Value;

/// A path below a key, one name per component, already split and validated.
using KeyPath = std::span<const std::wstring_view>;

/// What @ref Registry::RegQueryInfoKeyW reports, as owned values. Lengths
/// are in characters without terminators, except @ref cbMaxValue.
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

/// One entry of a hive's own layer, as @ref Hive::SnapshotUpper lists it.
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
 * @brief Storage for one subtree of a virtual namespace: a tree with one
 *  anonymous root, mounted at a path by @ref VirtualRegistry, held in memory
 *  on its own or layered over a key of another registry.
 *
 *  On its own, the hive starts empty and holds everything written to it.
 *  Layered over a lower key, reads fall through to that key and writes stay
 *  in memory: the hive's own layer adds and overrides keys and values.
 *  Deleting a lower value or key leaves a tombstone that hides it; a key
 *  deleted and created again, or emptied with @ref Registry::RegDeleteTreeW,
 *  becomes opaque and hides everything below it in the lower layer.
 *  Enumeration lists the hive's own entries first, then the lower ones no
 *  entry shadows; counts cover both.
 *
 *  The interface is node-level with owned values. Caller buffers, ANSI and
 *  wide, and handles stay in @ref VirtualRegistry, once for every hive.
 *  Every member answers the @ref LSTATUS the native registry answers for the
 *  same operation; a call on a deleted node answers @ref ERROR_KEY_DELETED,
 *  and a write to a frozen hive @ref ERROR_ACCESS_DENIED.
 *
 *  Each node has exactly one virtual handle, issued when the node is created
 *  (@ref HandleOf). Readers take the hive's lock shared and writers exclusive,
 *  until @ref Freeze; after that nothing writes and readers take no lock at
 *  all. The lower registry is never called with the lock held, so a lower
 *  call that loads a module or takes a long time cannot hold up the hive.
 *
 *  Frozen, a layered hive answers an open of a key it has nothing at or below
 *  with the lower registry's own handle (@ref OpenResult::hLower). A
 *  @ref VirtualRegistry sends later calls on such a handle to its base, so a
 *  frozen hive's lower registry MUST be that base or answer the same handles.
 *
 * @note A hive MUST outlive every registry it is mounted in. Destroying it
 *  turns its handles into invalid ones.
 *
 * @note Once hooked (@ref Detours::Install), every member may run under a
 *  foreign loader lock: nothing here loads a module, creates a thread or
 *  waits on a lock a loader-lock holder could own.
 */
class Hive final
{
public:
    /// A hive held entirely in memory; starts empty. Throws
    /// @ref std::bad_alloc.
    Hive();

    /// A hive over the key @p pszLowerPath below @p hLowerRoot of @p lower,
    /// which MUST outlive the hive. @p samLowerView (@ref KEY_WOW64_64KEY,
    /// @ref KEY_WOW64_32KEY or 0) is added to every lower open. Throws
    /// @ref std::bad_alloc.
    Hive(
        Registry& lower,
        HKEY hLowerRoot,
        _In_z_ LPCWSTR pszLowerPath,
        REGSAM samLowerView = 0
    );

    ~Hive();

    Hive(const Hive&) = delete;
    Hive& operator=(const Hive&) = delete;

    /// Makes the hive immutable from now on, so its readers take no lock.
    /// Safe while it is mounted and in use; there is no way back.
    void Freeze() noexcept;
    [[nodiscard]] bool IsFrozen() const noexcept;

    [[nodiscard]] _Ret_notnull_ Node* Root() const noexcept;

    /// The virtual handle of @p pNode.
    [[nodiscard]] static HKEY HandleOf(_In_ const Node* pNode) noexcept;

    /// The node a virtual handle of this copy of VReg belongs to, and its
    /// hive; null for any other handle.
    [[nodiscard]] static _Ret_maybenull_ Node* NodeOf(HKEY hKey) noexcept;

    [[nodiscard]] static _Ret_notnull_ Hive* HiveOf(
        _In_ const Node* pNode
    ) noexcept;

    LSTATUS OpenKey(
        _In_ Node* pParent,
        KeyPath path,
        REGSAM samDesired,
        OpenResult& result
    ) const noexcept;

    /// On a frozen hive an existing key opens as with @ref OpenKey, and a
    /// missing one answers @ref ERROR_ACCESS_DENIED.
    LSTATUS CreateKey(
        _Inout_ Node* pParent,
        KeyPath path,
        _In_z_ LPCWSTR pszClassName,
        DWORD dwOptions,
        REGSAM samDesired,
        OpenResult& result,
        DWORD& dwDisposition
    ) noexcept;

    /// Deletes the last component of @p path; a key with subkeys answers
    /// @ref ERROR_ACCESS_DENIED, as native does.
    LSTATUS DeleteKey(
        _Inout_ Node* pParent,
        KeyPath path
    ) noexcept;

    /// Deletes every subkey and value of @p pNode, leaving the node itself.
    LSTATUS DeleteTree(
        _Inout_ Node* pNode
    ) noexcept;

    /// Reads value @p pszName of @p pNode. @p cbData receives the size of the
    /// data in bytes; @p pData receives the data itself only when it is not
    /// null, so a null @p pData copies nothing.
    LSTATUS QueryValue(
        _In_ Node* pNode,
        _In_z_ LPCWSTR pszName,
        DWORD& dwType,
        DWORD& cbData,
        _Out_opt_ std::vector<std::byte>* pData
    ) const noexcept;

    LSTATUS SetValue(
        _Inout_ Node* pNode,
        _In_z_ LPCWSTR pszName,
        DWORD dwType,
        std::span<const std::byte> data
    ) noexcept;

    LSTATUS DeleteValue(
        _Inout_ Node* pNode,
        _In_z_ LPCWSTR pszName
    ) noexcept;

    /**
     * @brief Sets value @p pszName of @p pNode to @p pszText as a @ref REG_SZ and
     *  marks it MUI text.
     *
     *  @ref Registry::RegLoadMUIStringW answers an unmarked hive value
     *  @ref ERROR_INVALID_DATA, since resolving an `@` resource reference would
     *  load a module. A marked one stands for the text such a reference
     *  resolves to and answers with @p pszText itself. Any other write of the
     *  value drops the mark, and a `.reg` export carries it as a plain
     *  string.
     */
    LSTATUS SetMuiTextValue(
        _Inout_ Node* pNode,
        _In_z_ LPCWSTR pszName,
        _In_z_ LPCWSTR pszText
    ) noexcept;

    /// The text of value @p pszName of @p pNode if @ref SetMuiTextValue stored
    /// it, or @ref ERROR_INVALID_DATA for any other value; a value from below
    /// is never marked.
    LSTATUS QueryMuiText(
        _In_ Node* pNode,
        _In_z_ LPCWSTR pszName,
        std::wstring& text
    ) const noexcept;

    LSTATUS EnumKey(
        _In_ Node* pNode,
        DWORD dwIndex,
        std::wstring& name,
        std::wstring& className,
        FILETIME& lastWrite
    ) const noexcept;

    /// Reads the value of @p pNode at @p dwIndex. @p cbData receives the size
    /// of the data in bytes; @p pData receives the data itself only when it
    /// is not null, so a null @p pData copies nothing.
    LSTATUS EnumValue(
        _In_ Node* pNode,
        DWORD dwIndex,
        std::wstring& name,
        DWORD& dwType,
        DWORD& cbData,
        _Out_opt_ std::vector<std::byte>* pData
    ) const noexcept;

    LSTATUS QueryInfo(
        _In_ Node* pNode,
        KeyInfo& info
    ) const noexcept;

    /// What @ref Registry::RegFlushKey on @p pNode needs from the hive:
    /// success, or @ref ERROR_KEY_DELETED.
    LSTATUS CheckKey(
        _In_ Node* pNode
    ) const noexcept;

    /**
     * @brief Registers a one-shot change notification on @p pNode.
     *
     *  @p hEvent is signaled once, by the first write that @p dwFilter selects
     *  (@ref REG_NOTIFY_CHANGE_NAME for a subkey added or deleted,
     *  @ref REG_NOTIFY_CHANGE_LAST_SET for a value set or deleted; the other
     *  bits never fire) on the key, or below it when @p bWatchSubtree.
     *  Deleting a watched key fires its watches too. The hive duplicates
     *  @p hEvent, so the caller MAY close its own handle. A frozen hive
     *  accepts the registration and never signals. Answers
     *  @ref ERROR_KEY_DELETED for a deleted key, or the error of duplicating
     *  @p hEvent.
     *
     * @note Only writes made through the hive are seen; a change in the
     *  registry below a layered hive is not.
     */
    LSTATUS Watch(
        _In_ Node* pNode,
        BOOL bWatchSubtree,
        DWORD dwFilter,
        HANDLE hEvent
    ) const noexcept;

    /**
     * @brief Lists the hive's own layer: what it holds itself, as opposed to
     *  what shows through from below.
     *
     *  This is the seam `.reg` export reads. A key comes before its values,
     *  which come before its subkeys, which are in name order; values are in
     *  creation order. A key that only stands for a lower key, with no values
     *  of its own, is not listed; the hive's own keys below it are. Reads only
     *  the hive, never the lower registry.
     */
    LSTATUS SnapshotUpper(std::vector<UpperRecord>& records) const noexcept;

    /// The registry below this hive, which answers the handles in
    /// @ref OpenResult::hLower; null for a hive with nothing below it.
    [[nodiscard]] _Ret_maybenull_ Registry* Lower() const noexcept;

    /// A key of @ref Lower holding value @p pszName of @p pNode, when that value
    /// shows through from below; null (and success) when the hive holds it
    /// or nothing does. The caller closes it through @ref Lower.
    LSTATUS OpenLowerValueKey(
        _In_ Node* pNode,
        _In_z_ LPCWSTR pszName,
        HKEY& hLower
    ) const noexcept;

private:
    Hive(
        _In_opt_ Registry* pLower,
        HKEY hLowerRoot,
        std::wstring_view lowerPath,
        REGSAM samLowerView
    );

    /// A shared lock, or nothing once frozen.
    class ReadLock
    {
    public:
        explicit ReadLock(const Hive& hive) noexcept;
        ~ReadLock();
        ReadLock(const ReadLock&) = delete;
        ReadLock& operator=(const ReadLock&) = delete;

    private:
        SRWLOCK* m_pLock;
    };

    /// The exclusive lock, or nothing once frozen; false then. A check of
    /// @ref IsFrozen made before taking it can be stale, since a writer may
    /// wait here while @ref Freeze runs, so only this one decides a write.
    class WriteLock
    {
    public:
        explicit WriteLock(const Hive& hive) noexcept;
        ~WriteLock();
        WriteLock(const WriteLock&) = delete;
        WriteLock& operator=(const WriteLock&) = delete;

        explicit operator bool() const noexcept;

    private:
        SRWLOCK* m_pLock;
    };

    /// Whether a node's lower key shows through, and its path; read under
    /// the lock, used after it is released. Never visible without a lower
    /// registry.
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

    [[nodiscard]] static Node* FindChild(
        const Node& parent,
        std::wstring_view name
    ) noexcept;

    [[nodiscard]] static Value* FindValue(
        Node& node,
        std::wstring_view name
    ) noexcept;

    /// A new child of @p parent, with its handle. Throws @ref std::bad_alloc,
    /// and answers null when the handle namespace is exhausted. Const because
    /// @ref OpenKey on a writable layered hive adds the transparent nodes
    /// that stand for lower keys, which changes nothing the hive answers.
    [[nodiscard]] Node* AddChild(
        Node& parent,
        std::wstring_view name,
        std::wstring_view className,
        bool isVolatile
    ) const;

    /// Marks @p node and everything below it deleted, unlinks it from its
    /// parent and fires the watches on it and below it, and the name watches
    /// on its parent. The write lock must be held.
    static void Remove(Node& node) noexcept;

    /// Fires and drops the watches a change of kind @p dwFilter (a
    /// `REG_NOTIFY_CHANGE_*` bit) on @p node reaches: its own, and those of its
    /// ancestors that watch their subtree. Neither @ref SetEvent nor
    /// @ref CloseHandle can wait on the loader lock or on anything a caller of
    /// the hive could hold, so they run under the write lock, which the
    /// caller must hold; deferring them would need an allocation after the
    /// change is made.
    static void Notify(Node& node, DWORD dwFilter) noexcept;

    /// Stamps @p node as written now.
    static void Touch(Node& node) noexcept;

    /// Null for a hive held entirely in memory.
    Registry* m_pLower;
    HKEY m_hLowerRoot;
    std::wstring m_lowerPath;
    REGSAM m_samLowerView;

    mutable SRWLOCK m_lock = SRWLOCK_INIT;
    std::atomic<bool> m_isFrozen = false;

    /// Fixed for the life of the hive. Not read from @ref m_nodes, whose
    /// storage moves while writers add to it.
    Node* m_pRoot = nullptr;

    /// Nodes are created here and freed only with the hive; a deleted node
    /// stays allocated (marked deleted) so its handle keeps answering
    /// @ref ERROR_KEY_DELETED. Destroying the hive unbinds every handle it
    /// issued.
    mutable std::vector<std::unique_ptr<Node>> m_nodes;
};
} // namespace VReg
