#include "core/buffer.h"

#include "core/alloc.h"

/// The capacity to grow to for @p needed elements: at least double the old
/// one, so repeated appends stay linear.
static SIZE_T VReg__GrownCapacity(const SIZE_T capacity, const SIZE_T needed)
{
    SIZE_T grown = capacity < 8 ? 8 : capacity * 2;
    return grown < needed ? needed : grown;
}

VReg__WStringView VReg__ViewOf(_In_opt_z_ const LPCWSTR psz)
{
    VReg__WStringView view;
    view.pch = psz;
    view.cch = 0;
    if (psz)
        while (psz[view.cch] != L'\0')
            ++view.cch;

    return view;
}

BOOL VReg__BufferReserve(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Buffer* const pBuffer,
    const SIZE_T cbCapacity
)
{
    BYTE* pb;
    if (cbCapacity <= pBuffer->cbCapacity)
        return TRUE;

    pb = (BYTE*)VReg__Realloc(pAllocContext, pBuffer->pb, cbCapacity);
    if (!pb)
        return FALSE;

    pBuffer->pb = pb;
    pBuffer->cbCapacity = cbCapacity;
    return TRUE;
}

BOOL VReg__BufferResize(_In_opt_ void* const pAllocContext, _Inout_ VReg__Buffer* const pBuffer, const SIZE_T cb)
{
    if (cb > pBuffer->cbCapacity)
    {
        const SIZE_T cbGrown = VReg__GrownCapacity(pBuffer->cbCapacity, cb);
        if (!VReg__BufferReserve(pAllocContext, pBuffer, cbGrown))
            return FALSE;
    }

    pBuffer->cb = cb;
    return TRUE;
}

BOOL VReg__BufferAssign(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Buffer* const pBuffer,
    _In_reads_bytes_opt_(cb) const void* const p,
    const SIZE_T cb
)
{
    if (!VReg__BufferResize(pAllocContext, pBuffer, cb))
        return FALSE;

    if (cb != 0)
    {
        // A null source is legal only for an empty assignment.
        _Analysis_assume_(p != NULL);
        CopyMemory(pBuffer->pb, p, cb);
    }

    return TRUE;
}

BOOL VReg__BufferAppend(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Buffer* const pBuffer,
    _In_reads_bytes_(cb) const void* const p,
    const SIZE_T cb
)
{
    const SIZE_T cbOld = pBuffer->cb;
    if (!VReg__BufferResize(pAllocContext, pBuffer, cbOld + cb))
        return FALSE;

    if (cb != 0)
        CopyMemory(pBuffer->pb + cbOld, p, cb);

    return TRUE;
}

void VReg__BufferFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__Buffer* const pBuffer)
{
    VReg__Free(pAllocContext, pBuffer->pb);
    pBuffer->pb = NULL;
    pBuffer->cb = 0;
    pBuffer->cbCapacity = 0;
}

_Ret_z_ LPCWSTR VReg__WStringText(_In_ const VReg__WString* const pString)
{
    return pString->psz ? pString->psz : L"";
}

VReg__WStringView VReg__WStringViewOf(_In_ const VReg__WString* const pString)
{
    VReg__WStringView view;
    view.pch = VReg__WStringText(pString);
    view.cch = pString->cch;
    return view;
}

BOOL VReg__WStringReserve(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pString, const SIZE_T cch)
{
    wchar_t* psz;
    if (cch < pString->cchCapacity)
        return TRUE;

    psz = (wchar_t*)VReg__Realloc(pAllocContext, pString->psz, (cch + 1) * sizeof(wchar_t));
    if (!psz)
        return FALSE;

    if (!pString->psz)
        psz[0] = L'\0';

    pString->psz = psz;
    pString->cchCapacity = cch + 1;
    return TRUE;
}

BOOL VReg__WStringResize(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pString, const SIZE_T cch)
{
    if (cch >= pString->cchCapacity)
    {
        const SIZE_T cchGrown = VReg__GrownCapacity(pString->cchCapacity, cch + 1) - 1;
        if (!VReg__WStringReserve(pAllocContext, pString, cchGrown))
            return FALSE;
    }

    if (pString->psz)
        pString->psz[cch] = L'\0';

    pString->cch = cch;
    return TRUE;
}

BOOL VReg__WStringAssign(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pString,
    _In_reads_opt_(cch) const wchar_t* const pch,
    const SIZE_T cch
)
{
    if (!VReg__WStringResize(pAllocContext, pString, cch))
        return FALSE;

    if (cch != 0)
    {
        // A null source is legal only for an empty assignment.
        _Analysis_assume_(pch != NULL);
        CopyMemory(pString->psz, pch, cch * sizeof(wchar_t));
    }

    return TRUE;
}

BOOL VReg__WStringAppend(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pString,
    _In_reads_(cch) const wchar_t* const pch,
    const SIZE_T cch
)
{
    const SIZE_T cchOld = pString->cch;
    if (!VReg__WStringResize(pAllocContext, pString, cchOld + cch))
        return FALSE;

    if (cch != 0)
        CopyMemory(pString->psz + cchOld, pch, cch * sizeof(wchar_t));

    return TRUE;
}

BOOL VReg__WStringAppendChar(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pString, const wchar_t ch)
{
    return VReg__WStringAppend(pAllocContext, pString, &ch, 1);
}

void VReg__WStringTruncate(_Inout_ VReg__WString* const pString, const SIZE_T cch)
{
    VREG_ASSERT(cch <= pString->cch);
    pString->cch = cch;
    if (pString->psz)
        pString->psz[cch] = L'\0';
}

void VReg__WStringFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pString)
{
    VReg__Free(pAllocContext, pString->psz);
    pString->psz = NULL;
    pString->cch = 0;
    pString->cchCapacity = 0;
}

BOOL VReg__ArrayReserve(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Array* const pArray,
    const SIZE_T cbElement,
    const SIZE_T capacity
)
{
    void* p;
    if (capacity <= pArray->capacity)
        return TRUE;

    p = VReg__Realloc(pAllocContext, pArray->p, capacity * cbElement);
    if (!p)
        return FALSE;

    pArray->p = p;
    pArray->capacity = capacity;
    return TRUE;
}

BOOL VReg__ArrayInsert(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Array* const pArray,
    const SIZE_T cbElement,
    const SIZE_T index,
    _In_reads_bytes_(cbElement) const void* const pElement
)
{
    BYTE* pbAt;
    VREG_ASSERT(index <= pArray->count);
    if (pArray->count == pArray->capacity)
    {
        const SIZE_T grown = VReg__GrownCapacity(pArray->capacity, pArray->count + 1);
        if (!VReg__ArrayReserve(pAllocContext, pArray, cbElement, grown))
            return FALSE;
    }

    pbAt = (BYTE*)pArray->p + index * cbElement;
    MoveMemory(pbAt + cbElement, pbAt, (pArray->count - index) * cbElement);
    CopyMemory(pbAt, pElement, cbElement);
    ++pArray->count;
    return TRUE;
}

BOOL VReg__ArrayAppend(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__Array* const pArray,
    const SIZE_T cbElement,
    _In_reads_bytes_(cbElement) const void* const pElement
)
{
    return VReg__ArrayInsert(pAllocContext, pArray, cbElement, pArray->count, pElement);
}

void VReg__ArrayRemoveAt(_Inout_ VReg__Array* const pArray, const SIZE_T cbElement, const SIZE_T index)
{
    BYTE* const pbAt = (BYTE*)pArray->p + index * cbElement;
    VREG_ASSERT(index < pArray->count);
    MoveMemory(pbAt, pbAt + cbElement, (pArray->count - index - 1) * cbElement);
    --pArray->count;
}

void VReg__ArrayFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__Array* const pArray)
{
    VReg__Free(pAllocContext, pArray->p);
    pArray->p = NULL;
    pArray->count = 0;
    pArray->capacity = 0;
}
