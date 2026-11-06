#pragma once

#include <VReg/hive.h>

#include "core/buffer.h"

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VReg__Value
{
    VReg__WString name;
    DWORD dwType;
    VReg__Buffer data;

    /// Hides the lower value of this name; only in a hive with a lower
    /// registry.
    BOOL isTombstone;

    /// Stored by @ref VReg__HiveSetMuiTextValue: a terminated @ref REG_SZ.
    BOOL isMuiText;
} VReg__Value;

/// A pending change notification: fires once, then is gone.
typedef struct VReg__ChangeWatch
{
    /// Our own duplicate of the caller's event; closed when the watch fires or
    /// the hive goes.
    HANDLE hEvent;

    BOOL isSubtree;
    DWORD dwFilter;
} VReg__ChangeWatch;

/// The flags below @ref isDeleted describe a node's relation to the lower
/// registry and stay clear in a hive without one.
struct VReg__Node
{
    VRegHive* pHive;
    VReg__Node* pParent;
    HKEY hKey;
    VReg__WString name;
    VReg__WString className;
    FILETIME lastWrite;
    BOOL isVolatile;
    BOOL isDeleted;

    /// The key exists because the lower key at its path does; the node is
    /// just the way to it. Writing to it makes it an upper key.
    BOOL isTransparent;

    /// Hides the lower key of this name; never handed out.
    BOOL isTombstone;

    /// Hides every lower subkey and value (a key deleted and created again,
    /// or emptied by `RegDeleteTreeW`).
    BOOL isOpaque;

    /// The lower key at this path exists and its class shows through (the
    /// node was made from it, not created over it).
    BOOL isLowerBacked;

    /// `VReg__Node*`, sorted by @ref VReg__CompareNames, the order the native
    /// registry enumerates subkeys in.
    VReg__Array children;

    /// @ref VReg__Value, in creation order, as the native registry enumerates
    /// values.
    VReg__Array values;

    /// @ref VReg__ChangeWatch, guarded by the hive's write lock.
    VReg__Array watches;
};

/// Ordinal comparison of uppercased names, the registry's own collation:
/// negative, zero or positive.
VREGDEF int VReg__CompareNames(VReg__WStringView left, VReg__WStringView right);

#ifdef __cplusplus
} // extern "C"
#endif
