#pragma once

#include <VReg/hive.h>
#include <VReg/registry.h>

#include "core/buffer.h"

#include <Windows.h>

/// @file
/// @brief The node-level interface of a hive, with owned values, as
///  `virtual_registry` uses it: caller buffers, ANSI and wide, and handles
///  stay there, once for every hive. Every function answers the @ref LSTATUS
///  the native registry answers for the same operation; a call on a deleted
///  node answers @ref ERROR_KEY_DELETED, and a write to a frozen hive
///  @ref ERROR_ACCESS_DENIED.
///
///  A string or buffer a function fills is the caller's, allocated with the
///  hive's allocation context (@ref VReg__HiveAllocContext) and freed by the
///  caller with it.

#ifdef __cplusplus
extern "C" {
#endif

/// Where an open landed: a node of the hive, or (for a key a frozen layered
/// hive leaves untouched) a handle its lower registry opened, which the
/// caller now owns.
typedef struct VReg__OpenResult
{
    VReg__Node* pNode;
    HKEY hLower;
} VReg__OpenResult;

/// What `RegQueryInfoKeyW` reports, as owned values. Lengths are in
/// characters without terminators, except @ref cbMaxValue.
typedef struct VReg__KeyInfo
{
    VReg__WString className;
    DWORD cSubKeys;
    DWORD cchMaxSubKey;
    DWORD cchMaxClass;
    DWORD cValues;
    DWORD cchMaxValueName;
    DWORD cbMaxValue;
    FILETIME lastWrite;
} VReg__KeyInfo;

typedef enum VReg__UpperKind
{
    VREG__UPPER_KEY,
    VREG__UPPER_VALUE,
} VReg__UpperKind;

/// One entry of a hive's own layer, as @ref VReg__HiveSnapshotUpper lists
/// it; the views point into the hive and are valid during the visit only.
typedef struct VReg__UpperRecord
{
    VReg__UpperKind kind;

    /// The key, relative to the hive's root and separated by backslashes; empty
    /// for the root itself.
    VReg__WStringView path;

    /// Values only; empty for the default value.
    VReg__WStringView name;

    /// Values only.
    DWORD dwType;

    /// Values only.
    const BYTE* pData;
    DWORD cbData;

    /// A key or value of the lower layer that is deleted here.
    BOOL isTombstone;

    /// Keys only: the key hides everything the lower key of this path holds (it
    /// was deleted and created again, or emptied by `RegDeleteTreeW`).
    BOOL isOpaque;
} VReg__UpperRecord;

/// Called once per record under the hive's read lock, so it MUST NOT call
/// back into the hive; a status other than @ref ERROR_SUCCESS stops the
/// snapshot and is answered.
typedef LSTATUS (*VReg__UpperVisitor)(_In_opt_ void* pContext, _In_ const VReg__UpperRecord* pRecord);

_Ret_notnull_ VReg__Node* VReg__HiveRoot(_In_ VRegHive* pHive);

/// The registry below the hive, which answers the handles in
/// @ref VReg__OpenResult::hLower; null for a hive with nothing below it.
_Ret_maybenull_ VRegRegistry* VReg__HiveLower(_In_ VRegHive* pHive);

_Ret_maybenull_ void* VReg__HiveAllocContext(_In_ VRegHive* pHive);

/// The virtual handle of @p pNode.
HKEY VReg__NodeHandle(_In_ const VReg__Node* pNode);

/// The node a virtual handle of this copy of VReg belongs to; null for any
/// other handle.
_Ret_maybenull_ VReg__Node* VReg__NodeOf(_In_opt_ HKEY hKey);

_Ret_notnull_ VRegHive* VReg__NodeHive(_In_ const VReg__Node* pNode);

/// Opens the key @p cPath components below @p pParent. On a writable
/// layered hive the nodes that stand for lower keys are added on the way,
/// which changes nothing the hive answers.
LSTATUS VReg__HiveOpenKey(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pParent,
    _In_reads_(cPath) const VReg__WStringView* pPath,
    SIZE_T cPath,
    REGSAM samDesired,
    _Out_ VReg__OpenResult* pResult
);

/// On a frozen hive an existing key opens as with @ref VReg__HiveOpenKey, and
/// a missing one answers @ref ERROR_ACCESS_DENIED.
LSTATUS VReg__HiveCreateKey(
    _In_ VRegHive* pHive,
    _Inout_ VReg__Node* pParent,
    _In_reads_(cPath) const VReg__WStringView* pPath,
    SIZE_T cPath,
    _In_z_ LPCWSTR pszClassName,
    DWORD dwOptions,
    REGSAM samDesired,
    _Out_ VReg__OpenResult* pResult,
    _Out_ LPDWORD pdwDisposition
);

/// Deletes the last component of the path; a key with subkeys answers
/// @ref ERROR_ACCESS_DENIED, as native does.
LSTATUS VReg__HiveDeleteKey(
    _In_ VRegHive* pHive,
    _Inout_ VReg__Node* pParent,
    _In_reads_(cPath) const VReg__WStringView* pPath,
    SIZE_T cPath
);

/// Deletes every subkey and value of @p pNode, leaving the node itself.
LSTATUS VReg__HiveDeleteTree(_In_ VRegHive* pHive, _Inout_ VReg__Node* pNode);

/// Reads value @p pszName of @p pNode. @p pcbData receives the size of the
/// data in bytes; @p pData receives the data itself only when it is not
/// null, so a null @p pData copies nothing.
LSTATUS VReg__HiveQueryValue(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    _In_z_ LPCWSTR pszName,
    _Out_ LPDWORD pdwType,
    _Out_ LPDWORD pcbData,
    _Inout_opt_ VReg__Buffer* pData
);

LSTATUS VReg__HiveSetValue(
    _In_ VRegHive* pHive,
    _Inout_ VReg__Node* pNode,
    _In_z_ LPCWSTR pszName,
    DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* pData,
    DWORD cbData
);

LSTATUS VReg__HiveDeleteValue(_In_ VRegHive* pHive, _Inout_ VReg__Node* pNode, _In_z_ LPCWSTR pszName);

/**
 * @brief Sets value @p pszName of @p pNode to @p pszText as a @ref REG_SZ and
 *  marks it MUI text.
 *
 *  `RegLoadMUIStringW` answers an unmarked hive value @ref ERROR_INVALID_DATA,
 *  since resolving an `@` resource reference would load a module. A marked
 *  one stands for the text such a reference resolves to and answers with
 *  @p pszText itself. Any other write of the value drops the mark, and a
 *  `.reg` export carries it as a plain string.
 */
LSTATUS VReg__HiveSetMuiTextValue(
    _In_ VRegHive* pHive,
    _Inout_ VReg__Node* pNode,
    _In_z_ LPCWSTR pszName,
    _In_z_ LPCWSTR pszText
);

/// The text of value @p pszName of @p pNode if @ref VReg__HiveSetMuiTextValue
/// stored it, or @ref ERROR_INVALID_DATA for any other value; a value from
/// below is never marked.
LSTATUS VReg__HiveQueryMuiText(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    _In_z_ LPCWSTR pszName,
    _Inout_ VReg__WString* pText
);

LSTATUS VReg__HiveEnumKey(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    DWORD dwIndex,
    _Inout_ VReg__WString* pName,
    _Inout_ VReg__WString* pClassName,
    _Out_ FILETIME* pLastWrite
);

/// Reads the value of @p pNode at @p dwIndex. @p pcbData receives the size
/// of the data in bytes; @p pData receives the data itself only when it
/// is not null, so a null @p pData copies nothing.
LSTATUS VReg__HiveEnumValue(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    DWORD dwIndex,
    _Inout_ VReg__WString* pName,
    _Out_ LPDWORD pdwType,
    _Out_ LPDWORD pcbData,
    _Inout_opt_ VReg__Buffer* pData
);

/// Fills @p pInfo, whose @ref VReg__KeyInfo::className is the caller's
/// string.
LSTATUS VReg__HiveQueryInfo(_In_ VRegHive* pHive, _In_ VReg__Node* pNode, _Inout_ VReg__KeyInfo* pInfo);

/// What `RegFlushKey` on @p pNode needs from the hive: success, or
/// @ref ERROR_KEY_DELETED.
LSTATUS VReg__HiveCheckKey(_In_ VRegHive* pHive, _In_ VReg__Node* pNode);

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
LSTATUS VReg__HiveWatch(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    BOOL bWatchSubtree,
    DWORD dwFilter,
    _In_ HANDLE hEvent
);

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
LSTATUS VReg__HiveSnapshotUpper(_In_ VRegHive* pHive, _In_ VReg__UpperVisitor pfnVisit, _In_opt_ void* pContext);

/// A key of @ref VReg__HiveLower holding value @p pszName of @p pNode, when
/// that value shows through from below; null (and success) when the hive
/// holds it or nothing does. The caller closes it through the lower
/// registry.
LSTATUS VReg__HiveOpenLowerValueKey(
    _In_ VRegHive* pHive,
    _In_ VReg__Node* pNode,
    _In_z_ LPCWSTR pszName,
    _Out_ PHKEY phLower
);

#ifdef __cplusplus
} // extern "C"
#endif
