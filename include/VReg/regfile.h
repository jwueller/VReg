#pragma once

#include <VReg/hive.h>
#include <VReg/registry.h>

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The encodings and headers a `.reg` file is written in.
typedef enum VRegFormat
{
    /// `Windows Registry Editor Version 5.00` as UTF-16LE with a BOM: what
    /// regedit writes.
    VREG_FORMAT_UNICODE,

    /// The same header as UTF-8 without a BOM.
    VREG_FORMAT_UTF8,

    /// `REGEDIT4` in @ref CP_ACP.
    VREG_FORMAT_REGEDIT4,
} VRegFormat;

typedef enum VRegOperationKind
{
    VREG_OPERATION_CREATE_KEY,
    VREG_OPERATION_DELETE_KEY,
    VREG_OPERATION_SET_VALUE,
    VREG_OPERATION_DELETE_VALUE,
} VRegOperationKind;

/// One line of a `.reg` file that does something. Value operations act on the
/// key most recently created by the file; each carries that key so it stands
/// alone. The pointers belong to whoever built the operation: to the script
/// for one a script holds.
typedef struct VRegOperation
{
    VRegOperationKind kind;

    /// A predefined root such as @ref HKEY_LOCAL_MACHINE.
    HKEY hRoot;

    /// Below @ref hRoot, without a leading backslash; empty for the root
    /// itself.
    LPCWSTR pszPath;

    /// Empty for the default value (`@`). Values only.
    LPCWSTR pszName;

    /// Values only.
    DWORD dwType;

    /// Values only.
    const BYTE* pData;
    DWORD cbData;

    /// 1-based line of the file the operation starts on; zero for one that
    /// came from no file.
    SIZE_T line;
} VRegOperation;

/// The operations of a file, in file order, with the storage they point into.
typedef struct VRegScript VRegScript;

/// Where and why a file failed to parse.
typedef struct VRegParseError
{
    /// 1-based, in UTF-16 code units of the decoded text.
    SIZE_T line;

    SIZE_T column;

    /// A static string.
    const char* pszMessage;
} VRegParseError;

/**
 * @brief Parses a whole file into a new script.
 *
 *  The encoding is taken from a BOM (UTF-16LE, UTF-8); without one, a
 *  `REGEDIT4` header means @ref CP_ACP and anything else UTF-8. The script is
 *  only created when the whole file parses, and is freed with
 *  @ref VRegDestroyScript.
 *
 * @return @ref ERROR_SUCCESS, @ref ERROR_INVALID_DATA on a syntax or encoding
 *  error (described by @p pError, which MAY be null and is zeroed for any
 *  other outcome), or @ref ERROR_OUTOFMEMORY.
 */
LSTATUS WINAPI VRegParseScript(
    _In_reads_bytes_(cbFile) const void* pFile,
    _In_ SIZE_T cbFile,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegScript** ppScript,
    _Out_opt_ VRegParseError* pError
);

/// Reads the file @p pszPath, then @ref VRegParseScript. Failures to read
/// answer the Win32 error.
LSTATUS WINAPI VRegParseScriptFile(
    _In_z_ LPCWSTR pszPath,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegScript** ppScript,
    _Out_opt_ VRegParseError* pError
);

/// Frees a script; null is ignored.
void WINAPI VRegDestroyScript(_In_opt_ VRegScript* pScript);

/// The script's operations, in order, and how many; valid until the script is
/// destroyed.
_Ret_notnull_ const VRegOperation* WINAPI VRegGetScriptOperations(
    _In_ const VRegScript* pScript,
    _Out_ SIZE_T* pcOperations
);

/**
 * @brief Performs @p cOperations operations through @p pRegistry, in order.
 *
 *  Stops at the first operation that fails, stores its index in
 *  @p pFailedIndex (MAY be null) and returns its status; earlier operations
 *  stay applied. Deleting a key or value that does not exist is not a
 *  failure. An operation that would delete a whole root (a key deletion with
 *  an empty path) answers @ref ERROR_INVALID_PARAMETER.
 */
_Success_(return == ERROR_SUCCESS)
LSTATUS WINAPI VRegApplyOperations(
    _In_reads_(cOperations) const VRegOperation* pOperations,
    _In_ SIZE_T cOperations,
    _In_ VRegRegistry* pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* pFailedIndex
);

/// @ref VRegApplyOperations over a script's operations.
_Success_(return == ERROR_SUCCESS)
LSTATUS WINAPI VRegApplyScript(
    _In_ const VRegScript* pScript,
    _In_ VRegRegistry* pRegistry,
    _On_failure_(_Out_opt_) SIZE_T* pFailedIndex
);

/**
 * @brief A script that recreates the key @p pszPath below the predefined root
 *  @p hRoot and everything below it: each key, then its values, then its
 *  subkeys, keys and values sorted ordinal case-insensitive.
 *
 *  Written and applied, the script reproduces every value's bytes. Freed with
 *  @ref VRegDestroyScript.
 *
 * @return @ref ERROR_INVALID_HANDLE when @p hRoot is not a root a `.reg` file
 *  can name, @ref ERROR_CANTREAD for a key nested deeper than the registry
 *  allows (a cycle through some other registry), otherwise the status of the
 *  registry calls.
 */
LSTATUS WINAPI VRegSnapshotScript(
    _In_ VRegRegistry* pRegistry,
    _In_ HKEY hRoot,
    _In_opt_z_ LPCWSTR pszPath,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegScript** ppScript
);

/**
 * @brief A script of what @p pHive holds itself, which reproduces the hive's
 *  merged view when applied over the same lower registry (or, for a hive
 *  held entirely in memory, its contents).
 *
 *  @p hRoot and @p pszPath name where the hive is mounted: the operations'
 *  paths are that prefix plus the key's path in the hive. The hive's own
 *  keys are created with their own values, a deleted lower value is a value
 *  deletion, a deleted lower key a key deletion, and a key that hides its
 *  lower counterpart a deletion followed by its creation and contents. A key
 *  that only shows a lower key through is left out. Order is that of
 *  @ref VRegSnapshotScript. A tombstone with no lower counterpart is kept,
 *  and does nothing when applied. Freed with @ref VRegDestroyScript.
 *
 * @return As @ref VRegSnapshotScript, except that the hive is read instead of
 *  a registry.
 */
LSTATUS WINAPI VRegDeltaScript(
    _In_ VRegHive* pHive,
    _In_ HKEY hRoot,
    _In_opt_z_ LPCWSTR pszPath,
    _In_opt_ void* pAllocContext,
    _Outptr_ VRegScript** ppScript
);

/**
 * @brief Lays @p pScript out as a regedit-style file in @p format, into bytes
 *  allocated with the script's context, freed with @ref VRegFreeScriptBytes.
 *
 *  Each key operation is a `[key]` or `[-key]` line and each value operation
 *  a `"name"=...` line under it; a value whose key differs from the last key
 *  line gets a `[key]` line of its own first, so a script built by hand lays
 *  out as one parsed from a file would.
 *
 * @return @ref ERROR_NO_UNICODE_TRANSLATION when @p format cannot represent a
 *  name or string, @ref ERROR_INVALID_NAME for a name that holds a line break
 *  (no format can write one), @ref ERROR_INVALID_HANDLE for a root a `.reg`
 *  file cannot name.
 */
LSTATUS WINAPI VRegWriteScript(
    _In_ const VRegScript* pScript,
    _In_ VRegFormat format,
    _Outptr_result_bytebuffer_(*pcbBytes) void** ppBytes,
    _Out_ SIZE_T* pcbBytes
);

void WINAPI VRegFreeScriptBytes(_In_ const VRegScript* pScript, _In_opt_ void* pBytes);

/// @ref VRegWriteScript into the file @p pszPath. An existing file is replaced
/// only once the layout has succeeded.
LSTATUS WINAPI VRegWriteScriptFile(_In_ const VRegScript* pScript, _In_ VRegFormat format, _In_z_ LPCWSTR pszPath);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The C++ shape of the `.reg` support, over the C functions above.
namespace VReg::RegFile
{
enum class Format
{
    Unicode = VREG_FORMAT_UNICODE,
    Utf8 = VREG_FORMAT_UTF8,
    Regedit4 = VREG_FORMAT_REGEDIT4,
};

enum class OperationKind
{
    CreateKey = VREG_OPERATION_CREATE_KEY,
    DeleteKey = VREG_OPERATION_DELETE_KEY,
    SetValue = VREG_OPERATION_SET_VALUE,
    DeleteValue = VREG_OPERATION_DELETE_VALUE,
};

struct Operation
{
    OperationKind kind{};
    HKEY hRoot{};
    std::wstring path;
    std::wstring name;
    DWORD dwType{};
    std::vector<std::byte> data;
    std::size_t line{};
};

struct Script
{
    std::vector<Operation> operations;
};

struct ParseError
{
    std::size_t line{};
    std::size_t column{};
    const char* pszMessage{};
};

LSTATUS Parse(
    std::span<const std::byte> file,
    Script& script,
    _Out_opt_ ParseError* pError
) noexcept;

LSTATUS ParseFile(
    const std::filesystem::path& path,
    Script& script,
    _Out_opt_ ParseError* pError
) noexcept;

_Success_(return == ERROR_SUCCESS)
LSTATUS Apply(
    const Script& script,
    Registry& registry,
    _On_failure_(_Out_opt_) std::size_t* pFailed
) noexcept;

LSTATUS ExportSnapshot(
    Registry& registry,
    HKEY hRoot,
    _In_z_ LPCWSTR pszPath,
    Format format,
    std::vector<std::byte>& file
) noexcept;

LSTATUS ExportSnapshotToFile(
    Registry& registry,
    HKEY hRoot,
    _In_z_ LPCWSTR pszPath,
    Format format,
    const std::filesystem::path& fileName
) noexcept;

LSTATUS ExportDelta(
    const Hive& hive,
    HKEY hRoot,
    _In_z_ LPCWSTR pszPath,
    Format format,
    std::vector<std::byte>& file
) noexcept;

LSTATUS ExportDeltaToFile(
    const Hive& hive,
    HKEY hRoot,
    _In_z_ LPCWSTR pszPath,
    Format format,
    const std::filesystem::path& fileName
) noexcept;
} // namespace VReg::RegFile

#endif // __cplusplus
