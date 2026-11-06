#pragma once

#include <Windows.h>

/// @file
/// @brief Owned, growable storage with an allocation context passed to every
///  call that may allocate: a byte buffer, a wide string and an array of
///  fixed-size elements. A zero-filled instance is empty and needs no
///  initialization; every call that grows answers FALSE when memory ran out
///  and leaves the instance as it was.

#ifdef __cplusplus
extern "C" {
#endif

/// A view of wide text that is not necessarily terminated.
typedef struct VReg__WStringView
{
    const wchar_t* pch;
    SIZE_T cch;
} VReg__WStringView;

VReg__WStringView VReg__ViewOf(_In_opt_z_ LPCWSTR psz);

typedef struct VReg__Buffer
{
    BYTE* pb;
    SIZE_T cb;
    SIZE_T cbCapacity;
} VReg__Buffer;

BOOL VReg__BufferReserve(_In_opt_ void* pAllocContext, _Inout_ VReg__Buffer* pBuffer, SIZE_T cbCapacity);

/// Sets the size to @p cb, growing the capacity if needed; new bytes are
/// indeterminate.
BOOL VReg__BufferResize(_In_opt_ void* pAllocContext, _Inout_ VReg__Buffer* pBuffer, SIZE_T cb);

BOOL VReg__BufferAssign(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__Buffer* pBuffer,
    _In_reads_bytes_opt_(cb) const void* p,
    SIZE_T cb
);

BOOL VReg__BufferAppend(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__Buffer* pBuffer,
    _In_reads_bytes_(cb) const void* p,
    SIZE_T cb
);

void VReg__BufferFree(_In_opt_ void* pAllocContext, _Inout_ VReg__Buffer* pBuffer);

/// A wide string, terminated whenever @ref psz is not null; empty and
/// unallocated while it is.
typedef struct VReg__WString
{
    wchar_t* psz;
    SIZE_T cch;
    SIZE_T cchCapacity;
} VReg__WString;

/// The text, `L""` for an unallocated string.
_Ret_z_ LPCWSTR VReg__WStringText(_In_ const VReg__WString* pString);

VReg__WStringView VReg__WStringViewOf(_In_ const VReg__WString* pString);

/// Room for @p cch characters and the terminator.
BOOL VReg__WStringReserve(_In_opt_ void* pAllocContext, _Inout_ VReg__WString* pString, SIZE_T cch);

/// Sets the length to @p cch, growing if needed, and terminates; new
/// characters are indeterminate.
BOOL VReg__WStringResize(_In_opt_ void* pAllocContext, _Inout_ VReg__WString* pString, SIZE_T cch);

BOOL VReg__WStringAssign(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__WString* pString,
    _In_reads_opt_(cch) const wchar_t* pch,
    SIZE_T cch
);

BOOL VReg__WStringAppend(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__WString* pString,
    _In_reads_(cch) const wchar_t* pch,
    SIZE_T cch
);

BOOL VReg__WStringAppendChar(_In_opt_ void* pAllocContext, _Inout_ VReg__WString* pString, wchar_t ch);

/// Shortens the string to @p cch characters, which is at most its length.
void VReg__WStringTruncate(_Inout_ VReg__WString* pString, SIZE_T cch);

void VReg__WStringFree(_In_opt_ void* pAllocContext, _Inout_ VReg__WString* pString);

/// An array of @ref count elements of one size, given to every call.
typedef struct VReg__Array
{
    void* p;
    SIZE_T count;
    SIZE_T capacity;
} VReg__Array;

/// The element at @p index of an array of @p T, as an lvalue.
#define VREG__AT(pArray, T, index) (((T*)(pArray)->p)[index])

BOOL VReg__ArrayReserve(_In_opt_ void* pAllocContext, _Inout_ VReg__Array* pArray, SIZE_T cbElement, SIZE_T capacity);

/// Inserts a copy of @p pElement before @p index, which is at most the count.
BOOL VReg__ArrayInsert(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__Array* pArray,
    SIZE_T cbElement,
    SIZE_T index,
    _In_reads_bytes_(cbElement) const void* pElement
);

BOOL VReg__ArrayAppend(
    _In_opt_ void* pAllocContext,
    _Inout_ VReg__Array* pArray,
    SIZE_T cbElement,
    _In_reads_bytes_(cbElement) const void* pElement
);

/// Removes the element at @p index, moving the later ones down.
void VReg__ArrayRemoveAt(_Inout_ VReg__Array* pArray, SIZE_T cbElement, SIZE_T index);

void VReg__ArrayFree(_In_opt_ void* pAllocContext, _Inout_ VReg__Array* pArray);

#ifdef __cplusplus
} // extern "C"
#endif
