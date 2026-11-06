#include "regfile/script.h"

#include "core/alloc.h"
#include "core/node.h"

// Windows `reg export` breaks after the first comma past this column and
// indents continuation lines by two spaces. Wine regedit breaks at the column
// itself, which differs when a line ends exactly there.
#define VREG__HEX_WRAP_COLUMN ((SIZE_T)76)

static const wchar_t VREG__NEWLINE[] = L"\r\n";

/// Appends the terminated literal @p psz.
static BOOL VReg__OutAppend(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pOut, _In_z_ const wchar_t* const psz)
{
    const VReg__WStringView text = VReg__ViewOf(psz);
    return VReg__WStringAppend(pAllocContext, pOut, text.pch, text.cch);
}

static BOOL VReg__HasLineBreak(const VReg__WStringView text)
{
    SIZE_T i;
    for (i = 0; i < text.cch; ++i)
    {
        if (text.pch[i] == L'\r' || text.pch[i] == L'\n')
            return TRUE;
    }

    return FALSE;
}

static BOOL VReg__AppendQuoted(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pOut, const VReg__WStringView text)
{
    SIZE_T i;
    if (!VReg__WStringAppendChar(pAllocContext, pOut, L'"'))
        return FALSE;

    for (i = 0; i < text.cch; ++i)
    {
        const wchar_t ch = text.pch[i];
        if ((ch == L'\\' || ch == L'"') && !VReg__WStringAppendChar(pAllocContext, pOut, L'\\'))
            return FALSE;
        if (!VReg__WStringAppendChar(pAllocContext, pOut, ch))
            return FALSE;
    }

    return VReg__WStringAppendChar(pAllocContext, pOut, L'"');
}

/// The text a REG_SZ takes in the friendly form; FALSE when the form would not
/// reproduce the bytes.
static BOOL VReg__FriendlyString(_In_reads_bytes_opt_(cbData) const BYTE* const pData, const DWORD cbData, _Out_ VReg__WStringView* pText)
{
    const wchar_t* pch;
    SIZE_T cch;
    SIZE_T i;

    pText->pch = NULL;
    pText->cch = 0;
    if (cbData < sizeof(wchar_t) || cbData % sizeof(wchar_t) != 0)
        return FALSE;

    // Two bytes or more of data have storage.
    _Analysis_assume_(pData != NULL);
    pch = (const wchar_t*)pData;
    cch = cbData / sizeof(wchar_t);
    if (pch[cch - 1] != L'\0')
        return FALSE;

    --cch;
    for (i = 0; i < cch; ++i)
    {
        if (pch[i] == L'\0' || pch[i] == L'\r' || pch[i] == L'\n')
            return FALSE;
    }

    pText->pch = pch;
    pText->cch = cch;
    return TRUE;
}

/// Appends @p value in lowercase hex with at least @p cchMin digits.
static BOOL VReg__AppendHexNumber(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pOut, DWORD value, const SIZE_T cchMin)
{
    static const wchar_t DIGITS[] = L"0123456789abcdef";
    wchar_t wszDigits[8];
    SIZE_T cchDigits = 0;
    SIZE_T i;

    do
    {
        wszDigits[cchDigits++] = DIGITS[value & 0xF];
        value >>= 4;
    }
    while (value != 0 && cchDigits < 8);

    for (i = cchDigits; i < cchMin; ++i)
    {
        if (!VReg__WStringAppendChar(pAllocContext, pOut, L'0'))
            return FALSE;
    }
    for (i = 0; i < cchDigits; ++i)
    {
        if (!VReg__WStringAppendChar(pAllocContext, pOut, wszDigits[cchDigits - 1 - i]))
            return FALSE;
    }

    return TRUE;
}

static BOOL VReg__AppendHex(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pOut,
    _In_ const VRegOperation* const pOperation,
    SIZE_T column
)
{
    const SIZE_T cchBefore = pOut->cch;
    SIZE_T i;

    if (pOperation->dwType == REG_BINARY)
    {
        if (!VReg__OutAppend(pAllocContext, pOut, L"hex:"))
            return FALSE;
    }
    else
    {
        if (
            !VReg__OutAppend(pAllocContext, pOut, L"hex(")
            || !VReg__AppendHexNumber(pAllocContext, pOut, pOperation->dwType, 1)
            || !VReg__OutAppend(pAllocContext, pOut, L"):")
        )
            return FALSE;
    }

    column += pOut->cch - cchBefore;
    for (i = 0; i < pOperation->cbData; ++i)
    {
        if (!VReg__AppendHexNumber(pAllocContext, pOut, pOperation->pData[i], 2))
            return FALSE;

        column += 2;
        if (i + 1 == pOperation->cbData)
            break;
        if (!VReg__WStringAppendChar(pAllocContext, pOut, L','))
            return FALSE;
        if (++column > VREG__HEX_WRAP_COLUMN)
        {
            if (!VReg__OutAppend(pAllocContext, pOut, L"\\\r\n  "))
                return FALSE;

            column = 2;
        }
    }

    return TRUE;
}

static BOOL VReg__AppendValue(_In_opt_ void* const pAllocContext, _Inout_ VReg__WString* const pOut, _In_ const VRegOperation* const pOperation)
{
    const SIZE_T lineStart = pOut->cch;
    const VReg__WStringView name = VReg__ViewOf(pOperation->pszName);
    VReg__WStringView text;
    BOOL isAppended;

    if (name.cch == 0)
        isAppended = VReg__WStringAppendChar(pAllocContext, pOut, L'@');
    else
        isAppended = VReg__AppendQuoted(pAllocContext, pOut, name);
    if (!isAppended || !VReg__WStringAppendChar(pAllocContext, pOut, L'='))
        return FALSE;

    if (pOperation->kind == VREG_OPERATION_DELETE_VALUE)
    {
        isAppended = VReg__WStringAppendChar(pAllocContext, pOut, L'-');
    }
    else if (pOperation->dwType == REG_SZ && VReg__FriendlyString(pOperation->pData, pOperation->cbData, &text))
    {
        isAppended = VReg__AppendQuoted(pAllocContext, pOut, text);
    }
    else if (pOperation->dwType == REG_DWORD && pOperation->cbData == sizeof(DWORD))
    {
        DWORD dwValue;
        CopyMemory(&dwValue, pOperation->pData, sizeof(dwValue));
        isAppended =
            VReg__OutAppend(pAllocContext, pOut, L"dword:")
            && VReg__AppendHexNumber(pAllocContext, pOut, dwValue, 8);
    }
    else
    {
        isAppended = VReg__AppendHex(pAllocContext, pOut, pOperation, pOut->cch - lineStart);
    }

    return isAppended && VReg__OutAppend(pAllocContext, pOut, VREG__NEWLINE);
}

/// Appends `[key]` or `[-key]` for @p pOperation's key, preceded by a blank
/// line unless it opens the file.
static LSTATUS VReg__AppendKeyLine(
    _In_opt_ void* const pAllocContext,
    _Inout_ VReg__WString* const pOut,
    _In_ const VRegOperation* const pOperation,
    const BOOL isDelete,
    const BOOL isFirst
)
{
    const wchar_t* const pszRoot = VReg__RootName(pOperation->hRoot);
    const VReg__WStringView path = VReg__ViewOf(pOperation->pszPath);
    if (!pszRoot)
        return ERROR_INVALID_HANDLE;

    if (
        (!isFirst && !VReg__OutAppend(pAllocContext, pOut, VREG__NEWLINE))
        || !VReg__OutAppend(pAllocContext, pOut, isDelete ? L"[-" : L"[")
        || !VReg__OutAppend(pAllocContext, pOut, pszRoot)
        || (path.cch != 0 && !VReg__WStringAppendChar(pAllocContext, pOut, L'\\'))
        || !VReg__WStringAppend(pAllocContext, pOut, path.pch, path.cch)
        || !VReg__OutAppend(pAllocContext, pOut, L"]")
        || !VReg__OutAppend(pAllocContext, pOut, VREG__NEWLINE)
    )
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

/// Whether @p pLeft and @p pRight name the same key.
static BOOL VReg__SameKey(_In_ const VRegOperation* const pLeft, _In_ const VRegOperation* const pRight)
{
    return pLeft->hRoot == pRight->hRoot
        && VReg__CompareNames(VReg__ViewOf(pLeft->pszPath), VReg__ViewOf(pRight->pszPath)) == 0;
}

LSTATUS WINAPI VRegWriteScript(
    _In_ const VRegScript* const pScript,
    _In_ const VRegFormat format,
    _Outptr_result_bytebuffer_(*pcbBytes) void** const ppBytes,
    _Out_ SIZE_T* const pcbBytes
)
{
    void* const pAllocContext = pScript->pAllocContext;
    VReg__WString out = {NULL, 0, 0};
    VReg__Buffer file = {NULL, 0, 0};
    const VRegOperation* pLastKey = NULL;
    SIZE_T i;
    LSTATUS status = ERROR_SUCCESS;

    *ppBytes = NULL;
    *pcbBytes = 0;
    if (
        !VReg__OutAppend(pAllocContext, &out, format == VREG_FORMAT_REGEDIT4 ? L"REGEDIT4" : L"Windows Registry Editor Version 5.00")
        || !VReg__OutAppend(pAllocContext, &out, VREG__NEWLINE)
        || !VReg__OutAppend(pAllocContext, &out, VREG__NEWLINE)
    )
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    for (i = 0; i < pScript->operations.count; ++i)
    {
        const VRegOperation* const pOperation = &VREG__AT(&pScript->operations, VRegOperation, i);
        const BOOL isKey = pOperation->kind == VREG_OPERATION_CREATE_KEY || pOperation->kind == VREG_OPERATION_DELETE_KEY;
        if (VReg__HasLineBreak(VReg__ViewOf(pOperation->pszPath)) || VReg__HasLineBreak(VReg__ViewOf(pOperation->pszName)))
        {
            status = ERROR_INVALID_NAME;
            goto done;
        }

        if (isKey || !pLastKey || !VReg__SameKey(pLastKey, pOperation))
        {
            status = VReg__AppendKeyLine(pAllocContext, &out, pOperation, pOperation->kind == VREG_OPERATION_DELETE_KEY, i == 0);
            if (status != ERROR_SUCCESS)
                goto done;

            pLastKey = pOperation;
        }

        if (!isKey && !VReg__AppendValue(pAllocContext, &out, pOperation))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }
    }

    if (pScript->operations.count != 0 && !VReg__OutAppend(pAllocContext, &out, VREG__NEWLINE))
    {
        status = ERROR_OUTOFMEMORY;
        goto done;
    }

    status = VReg__EncodeFile(pAllocContext, &out, format, &file);
    if (status != ERROR_SUCCESS)
        goto done;

    // The header alone makes the file non-empty, so the encoding allocated.
    _Analysis_assume_(file.pb != NULL);
    *ppBytes = file.pb;
    *pcbBytes = file.cb;
    file.pb = NULL;

done:
    VReg__WStringFree(pAllocContext, &out);
    VReg__BufferFree(pAllocContext, &file);
    return status;
}

LSTATUS WINAPI VRegWriteScriptFile(_In_ const VRegScript* const pScript, _In_ const VRegFormat format, _In_z_ const LPCWSTR pszPath)
{
    void* pBytes = NULL;
    SIZE_T cbBytes = 0;
    LSTATUS status = VRegWriteScript(pScript, format, &pBytes, &cbBytes);
    if (status != ERROR_SUCCESS)
        return status;

    status = VReg__WriteWholeFile(pszPath, pBytes, cbBytes);
    VRegFreeScriptBytes(pScript, pBytes);
    return status;
}
