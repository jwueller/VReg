#pragma once

#include <VReg/registry.h>

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Storage for one subtree of a virtual namespace: a tree with one
 *  anonymous root, mounted at a path by a virtual registry, held in memory
 *  on its own or layered over a key of another registry.
 *
 *  On its own, the hive starts empty and holds everything written to it.
 *  Layered over a lower key, reads fall through to that key and writes stay
 *  in memory: the hive's own layer adds and overrides keys and values.
 *  Deleting a lower value or key leaves a tombstone that hides it; a key
 *  deleted and created again, or emptied with `RegDeleteTreeW`, becomes
 *  opaque and hides everything below it in the lower layer. Enumeration
 *  lists the hive's own entries first, then the lower ones no entry shadows;
 *  counts cover both.
 *
 *  Each key has exactly one virtual handle, issued when the key is created.
 *  Readers take the hive's lock shared and writers exclusive, until
 *  @ref VRegFreezeHive; after that nothing writes and readers take no lock
 *  at all. The lower registry is never called with the lock held, so a lower
 *  call that loads a module or takes a long time cannot hold up the hive.
 *
 *  Frozen, a layered hive answers an open of a key it has nothing at or below
 *  with the lower registry's own handle. A virtual registry sends later calls
 *  on such a handle to its base, so a frozen hive's lower registry MUST be
 *  that base or answer the same handles.
 *
 * @note A hive MUST outlive every registry it is mounted in. Destroying it
 *  turns its handles into invalid ones.
 *
 * @note Once hooked, every call on a hive may run under a foreign loader
 *  lock: nothing here loads a module, creates a thread or waits on a lock a
 *  loader-lock holder could own.
 */
typedef struct VRegHive VRegHive;

/// A key in a hive. Opaque outside the hive that owns it; never freed before
/// the hive, so a pointer to one stays valid for the hive's life.
typedef struct VReg__Node VReg__Node;

/// A hive held entirely in memory; starts empty. @p pAllocContext is passed
/// to every allocation the hive makes.
LSTATUS WINAPI VRegCreateHive(_In_opt_ void* pAllocContext, _Outptr_ VRegHive** ppHive);

/// A hive over the key @p pszLowerPath below @p hLowerRoot of @p pLower,
/// which MUST outlive the hive. @p samLowerView (@ref KEY_WOW64_64KEY,
/// @ref KEY_WOW64_32KEY or 0) is added to every lower open.
LSTATUS WINAPI VRegCreateLayeredHive(
    _In_ VRegRegistry* pLower,
    _In_ HKEY hLowerRoot,
    _In_opt_z_ LPCWSTR pszLowerPath,
    _In_ REGSAM samLowerView,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegHive** ppHive
);

/// Frees the hive and unbinds every handle it issued; null is ignored.
void WINAPI VRegDestroyHive(_In_opt_ VRegHive* pHive);

/// Makes the hive immutable from now on, so its readers take no lock.
/// Safe while it is mounted and in use; there is no way back.
void WINAPI VRegFreezeHive(_In_ VRegHive* pHive);

BOOL WINAPI VRegIsHiveFrozen(_In_ VRegHive* pHive);

/**
 * @brief Creates the key @p pszPath below the hive's root if needed and sets
 *  its value @p pszName to @p pszText as a @ref REG_SZ marked as MUI text.
 *
 *  `RegLoadMUIStringW` answers an unmarked hive value @ref ERROR_INVALID_DATA,
 *  since resolving an `@` resource reference would load a module. A marked
 *  one stands for the text such a reference resolves to and answers with
 *  @p pszText itself. Any other write of the value drops the mark, and a
 *  `.reg` export carries it as a plain string.
 */
LSTATUS WINAPI VRegSetHiveMuiTextValue(
    _In_ VRegHive* pHive,
    _In_opt_z_ LPCWSTR pszPath,
    _In_opt_z_ LPCWSTR pszName,
    _In_z_ LPCWSTR pszText
);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace VReg
{
using Node = VReg__Node;

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

/// A @ref VRegHive owned by a C++ object, with the node-level interface of
/// `core/hive.h` over owned STL values. The interface is node-level with owned
/// values. Caller buffers, ANSI and wide, and handles stay in
/// @ref VirtualRegistry, once for every hive.
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

    /// The C hive, for @ref VirtualRegistry::Mount and the C functions.
    [[nodiscard]] _Ret_notnull_ VRegHive* Get() const noexcept;

    void Freeze() noexcept;
    [[nodiscard]] bool IsFrozen() const noexcept;

    [[nodiscard]] _Ret_notnull_ Node* Root() const noexcept;

    /// The virtual handle of @p pNode.
    [[nodiscard]] static HKEY HandleOf(_In_ const Node* pNode) noexcept;

    /// The node a virtual handle of this copy of VReg belongs to; null for
    /// any other handle.
    [[nodiscard]] static _Ret_maybenull_ Node* NodeOf(HKEY hKey) noexcept;

    /// The C hive @p pNode belongs to.
    [[nodiscard]] static _Ret_notnull_ VRegHive* HiveOf(
        _In_ const Node* pNode
    ) noexcept;

    LSTATUS OpenKey(
        _In_ Node* pParent,
        KeyPath path,
        REGSAM samDesired,
        OpenResult& result
    ) const noexcept;

    LSTATUS CreateKey(
        _Inout_ Node* pParent,
        KeyPath path,
        _In_z_ LPCWSTR pszClassName,
        DWORD dwOptions,
        REGSAM samDesired,
        OpenResult& result,
        DWORD& dwDisposition
    ) noexcept;

    LSTATUS DeleteKey(
        _Inout_ Node* pParent,
        KeyPath path
    ) noexcept;

    LSTATUS DeleteTree(
        _Inout_ Node* pNode
    ) noexcept;

    /// @p pData receives the data only when it is not null.
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

    LSTATUS SetMuiTextValue(
        _Inout_ Node* pNode,
        _In_z_ LPCWSTR pszName,
        _In_z_ LPCWSTR pszText
    ) noexcept;

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

    /// @p pData receives the data only when it is not null.
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

    LSTATUS CheckKey(
        _In_ Node* pNode
    ) const noexcept;

    LSTATUS Watch(
        _In_ Node* pNode,
        BOOL bWatchSubtree,
        DWORD dwFilter,
        HANDLE hEvent
    ) const noexcept;

    LSTATUS SnapshotUpper(std::vector<UpperRecord>& records) const noexcept;

    /// The registry below this hive, which answers the handles in
    /// @ref OpenResult::hLower; null for a hive with nothing below it.
    [[nodiscard]] _Ret_maybenull_ VRegRegistry* Lower() const noexcept;

    /// A key of @ref Lower holding value @p pszName of @p pNode, when that value
    /// shows through from below; null (and success) when the hive holds it
    /// or nothing does. The caller closes it through @ref Lower.
    LSTATUS OpenLowerValueKey(
        _In_ Node* pNode,
        _In_z_ LPCWSTR pszName,
        HKEY& hLower
    ) const noexcept;

private:
    VRegHive* m_pHive;
};
} // namespace VReg

#endif // __cplusplus
