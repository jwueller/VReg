#include "core/ansi.h"

#include "core/alloc.h"

LSTATUS VReg__WideFromAnsi(
    _In_opt_ void* const pAllocContext,
    _In_reads_(cch) const char* const pch,
    const SIZE_T cch,
    _Inout_ VReg__WString* const pText
)
{
    int cchWide;
    if (cch > (SIZE_T)MAXINT32)
        return ERROR_OUTOFMEMORY;
    if (cch == 0)
        return VReg__WStringResize(pAllocContext, pText, 0) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;

    cchWide = MultiByteToWideChar(CP_ACP, 0, pch, (int)cch, NULL, 0);
    if (!VReg__WStringResize(pAllocContext, pText, (SIZE_T)cchWide))
        return ERROR_OUTOFMEMORY;
    if (cchWide > 0)
        MultiByteToWideChar(CP_ACP, 0, pch, (int)cch, pText->psz, cchWide);

    return ERROR_SUCCESS;
}

LSTATUS VReg__AnsiFromWide(
    _In_opt_ void* const pAllocContext,
    _In_reads_(cch) const wchar_t* const pch,
    const SIZE_T cch,
    _Inout_ VReg__Buffer* const pText
)
{
    int cchAnsi;
    if (cch > (SIZE_T)MAXINT32)
        return ERROR_OUTOFMEMORY;
    if (cch == 0)
        return VReg__BufferResize(pAllocContext, pText, 0) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;

    cchAnsi = WideCharToMultiByte(CP_ACP, 0, pch, (int)cch, NULL, 0, NULL, NULL);
    if (!VReg__BufferResize(pAllocContext, pText, (SIZE_T)cchAnsi))
        return ERROR_OUTOFMEMORY;
    if (cchAnsi > 0)
        WideCharToMultiByte(CP_ACP, 0, pch, (int)cch, (char*)pText->pb, cchAnsi, NULL, NULL);

    return ERROR_SUCCESS;
}

LSTATUS VReg__WideArgInit(_In_opt_ void* const pAllocContext, _In_opt_z_ const LPCSTR psz, _Out_ VReg__WideArg* const pArg)
{
    SIZE_T cch = 0;
    ZeroMemory(pArg, sizeof(*pArg));
    pArg->isNull = psz == NULL;
    if (!psz)
        return ERROR_SUCCESS;

    while (psz[cch] != '\0')
        ++cch;

    return VReg__WideFromAnsi(pAllocContext, psz, cch, &pArg->text);
}

_Ret_maybenull_z_ LPCWSTR VReg__WideArgText(_In_ const VReg__WideArg* const pArg)
{
    return pArg->isNull ? NULL : VReg__WStringText(&pArg->text);
}

_Ret_maybenull_ LPWSTR VReg__WideArgMutable(_In_ VReg__WideArg* const pArg)
{
    if (pArg->isNull)
        return NULL;

    // An unallocated empty string has no storage to hand out.
    if (!pArg->text.psz && !VReg__WStringReserve(NULL, &pArg->text, 0))
        return NULL;

    return pArg->text.psz;
}

void VReg__WideArgFree(_In_opt_ void* const pAllocContext, _Inout_ VReg__WideArg* const pArg)
{
    VReg__WStringFree(pAllocContext, &pArg->text);
}
