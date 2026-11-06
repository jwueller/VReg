#pragma once

#include <VReg/regfile.h>

#include "core/buffer.h"

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The storage behind one @ref VRegOperation of a script.
typedef struct VReg__ScriptEntry
{
    VReg__WString path;
    VReg__WString name;
    VReg__Buffer data;
} VReg__ScriptEntry;

struct VRegScript
{
    void* pAllocContext;

    /// `VRegOperation`, pointing into @ref entries.
    VReg__Array operations;

    /// `VReg__ScriptEntry`, one per operation.
    VReg__Array entries;
};

/// A predefined root a `.reg` file can name: exports write its long name,
/// imports accept either.
typedef struct VReg__RootKey
{
    const wchar_t* pszLong;
    const wchar_t* pszShort;
    HKEY hKey;
} VReg__RootKey;

/// Every root a `.reg` file can name, the one list import and export share;
/// @ref VREG__ROOT_KEY_COUNT entries.
extern const VReg__RootKey VREG__ROOT_KEYS[];
#define VREG__ROOT_KEY_COUNT ((SIZE_T)5)

/// The long name of a root a `.reg` file can name; null for any other handle.
_Ret_maybenull_z_ const wchar_t* VReg__RootName(_In_opt_ HKEY hRoot);

/// The root named @p name, long or short form, ignoring case.
BOOL VReg__FindRoot(VReg__WStringView name, _Out_ PHKEY phRoot);

LSTATUS VReg__ScriptCreate(_In_opt_ void* pAllocContext, _Outptr_ VRegScript** ppScript);

/// Appends an operation, copying its path, name and data into the script.
LSTATUS VReg__ScriptAppend(
    _Inout_ VRegScript* pScript,
    VRegOperationKind kind,
    _In_ HKEY hRoot,
    VReg__WStringView path,
    VReg__WStringView name,
    DWORD dwType,
    _In_reads_bytes_opt_(cbData) const BYTE* pData,
    DWORD cbData,
    SIZE_T line
);

/// Decodes a whole file to text without BOM. Detects the encoding as
/// @ref VRegParseScript documents. Answers @ref ERROR_INVALID_DATA with
/// @p pError (zeroed by the caller) set on bytes the encoding cannot hold.
LSTATUS VReg__DecodeFile(
    _In_opt_ void* pAllocContext,
    _In_reads_bytes_(cbFile) const void* pFile,
    SIZE_T cbFile,
    _Inout_ VReg__WString* pText,
    _Inout_opt_ VRegParseError* pError
);

/// The bytes of @p pText in @p format, with a BOM for Unicode. Answers
/// @ref ERROR_NO_UNICODE_TRANSLATION when the format's code page cannot hold
/// it.
LSTATUS VReg__EncodeFile(
    _In_opt_ void* pAllocContext,
    _In_ const VReg__WString* pText,
    VRegFormat format,
    _Inout_ VReg__Buffer* pFile
);

/// Reads and writes whole files; Win32 errors answer as is.
LSTATUS VReg__ReadWholeFile(_In_opt_ void* pAllocContext, _In_z_ LPCWSTR pszPath, _Inout_ VReg__Buffer* pFile);

LSTATUS VReg__WriteWholeFile(_In_z_ LPCWSTR pszPath, _In_reads_bytes_(cbFile) const void* pFile, SIZE_T cbFile);

#ifdef __cplusplus
} // extern "C"
#endif
