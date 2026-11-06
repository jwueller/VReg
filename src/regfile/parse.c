#include "regfile/script.h"

#include "core/alloc.h"

/// Where a logical line's text came from: the physical line and column its
/// characters from @ref start on were read at.
typedef struct VReg__LineSegment
{
    SIZE_T start;
    SIZE_T line;
    SIZE_T column;
} VReg__LineSegment;

typedef enum VReg__KeyState
{
    VREG__KEY_NONE,
    VREG__KEY_OPEN,
    VREG__KEY_DELETED,
} VReg__KeyState;

typedef struct VReg__Parser
{
    void* pAllocContext;
    VRegScript* pScript;

    /// The logical line being parsed: a line of the file with its
    /// continuations joined, and the position in it.
    VReg__WString text;
    SIZE_T pos;

    /// `VReg__LineSegment`: offsets into @ref text map back to the physical
    /// lines the text came from.
    VReg__Array segments;

    /// The physical line the logical line starts on, 1-based.
    SIZE_T line;

    VReg__KeyState state;
    HKEY hRoot;
    VReg__WString path;

    /// Set by @ref VReg__ParseFail: where in @ref text, and why.
    const char* pszError;
    SIZE_T errorOffset;
} VReg__Parser;

/// Records a syntax error at the parser's position and answers its status.
static LSTATUS VReg__ParseFail(_Inout_ VReg__Parser* const pParser, _In_z_ const char* const pszMessage)
{
    pParser->pszError = pszMessage;
    pParser->errorOffset = pParser->pos;
    return ERROR_INVALID_DATA;
}

static BOOL VReg__IsBlank(const wchar_t ch)
{
    return ch == L' ' || ch == L'\t';
}

static int VReg__HexDigit(const wchar_t ch)
{
    if (ch >= L'0' && ch <= L'9')
        return ch - L'0';
    if (ch >= L'a' && ch <= L'f')
        return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F')
        return ch - L'A' + 10;
    return -1;
}

static wchar_t VReg__ParseLower(const wchar_t ch)
{
    return (ch >= L'A' && ch <= L'Z') ? (wchar_t)(ch - L'A' + L'a') : ch;
}

static BOOL VReg__ViewEquals(const VReg__WStringView left, const VReg__WStringView right)
{
    SIZE_T i;
    if (left.cch != right.cch)
        return FALSE;

    for (i = 0; i < left.cch; ++i)
    {
        if (left.pch[i] != right.pch[i])
            return FALSE;
    }

    return TRUE;
}

static BOOL VReg__ViewEqualsIgnoreCase(const VReg__WStringView left, const VReg__WStringView right)
{
    SIZE_T i;
    if (left.cch != right.cch)
        return FALSE;

    for (i = 0; i < left.cch; ++i)
    {
        if (VReg__ParseLower(left.pch[i]) != VReg__ParseLower(right.pch[i]))
            return FALSE;
    }

    return TRUE;
}

static VReg__WStringView VReg__TrimEnd(VReg__WStringView text)
{
    while (text.cch != 0 && VReg__IsBlank(text.pch[text.cch - 1]))
        --text.cch;

    return text;
}

static VReg__WStringView VReg__Substring(const VReg__WStringView text, const SIZE_T start, const SIZE_T cch)
{
    VReg__WStringView result;
    result.pch = text.pch + start;
    result.cch = cch;
    return result;
}

/// The index of @p ch in @p text from @p start on, or the text's length.
static SIZE_T VReg__FindChar(const VReg__WStringView text, const SIZE_T start, const wchar_t ch)
{
    SIZE_T i;
    for (i = start; i < text.cch; ++i)
    {
        if (text.pch[i] == ch)
            return i;
    }

    return text.cch;
}

/// The physical line and column of @p offset into the logical line.
static void VReg__ParserPosition(
    _In_ const VReg__Parser* const pParser,
    const SIZE_T offset,
    _Out_ SIZE_T* const pLine,
    _Out_ SIZE_T* const pColumn
)
{
    const VReg__LineSegment* pSegment = &VREG__AT(&pParser->segments, VReg__LineSegment, 0);
    SIZE_T i;
    for (i = 0; i < pParser->segments.count; ++i)
    {
        const VReg__LineSegment* const pCandidate = &VREG__AT(&pParser->segments, VReg__LineSegment, i);
        if (pCandidate->start > offset)
            break;

        pSegment = pCandidate;
    }

    *pLine = pSegment->line;
    *pColumn = pSegment->column + (offset - pSegment->start);
}

static BOOL VReg__AtEnd(_In_ const VReg__Parser* const pParser)
{
    return pParser->pos >= pParser->text.cch;
}

static wchar_t VReg__Current(_In_ const VReg__Parser* const pParser)
{
    return pParser->text.psz[pParser->pos];
}

static void VReg__SkipBlanks(_Inout_ VReg__Parser* const pParser)
{
    while (!VReg__AtEnd(pParser) && VReg__IsBlank(VReg__Current(pParser)))
        ++pParser->pos;
}

static LSTATUS VReg__ExpectEnd(_Inout_ VReg__Parser* const pParser)
{
    VReg__SkipBlanks(pParser);
    if (!VReg__AtEnd(pParser))
        return VReg__ParseFail(pParser, "unexpected text at the end of the line");

    return ERROR_SUCCESS;
}

/// Consumes @p pszToken, ignoring case, if the text continues with it.
static BOOL VReg__Consume(_Inout_ VReg__Parser* const pParser, _In_z_ const wchar_t* const pszToken)
{
    const VReg__WStringView token = VReg__ViewOf(pszToken);
    const VReg__WStringView text = VReg__WStringViewOf(&pParser->text);
    if (text.cch - pParser->pos < token.cch)
        return FALSE;
    if (!VReg__ViewEqualsIgnoreCase(VReg__Substring(text, pParser->pos, token.cch), token))
        return FALSE;

    pParser->pos += token.cch;
    return TRUE;
}

/// Reads the logical line starting at physical line @p index of @p pLines
/// (empty for blank and comment lines) and answers the index of its last
/// physical line in @p pLast.
static LSTATUS VReg__ReadLogicalLine(
    _Inout_ VReg__Parser* const pParser,
    _In_ const VReg__Array* const pLines,
    const SIZE_T index,
    _Out_ SIZE_T* const pLast
)
{
    void* const pAllocContext = pParser->pAllocContext;
    VReg__WStringView physical = VREG__AT(pLines, VReg__WStringView, index);
    VReg__LineSegment segment;
    SIZE_T current = index;

    pParser->segments.count = 0;
    if (!VReg__WStringResize(pAllocContext, &pParser->text, 0))
        return ERROR_OUTOFMEMORY;

    segment.start = 0;
    segment.line = index + 1;
    segment.column = 1;
    while (physical.cch != 0 && VReg__IsBlank(physical.pch[0]))
    {
        ++physical.pch;
        --physical.cch;
        ++segment.column;
    }
    if (!VReg__ArrayAppend(pAllocContext, &pParser->segments, sizeof(segment), &segment))
        return ERROR_OUTOFMEMORY;

    *pLast = index;
    if (physical.cch == 0 || physical.pch[0] == L';')
        return ERROR_SUCCESS;

    if (!VReg__WStringAssign(pAllocContext, &pParser->text, physical.pch, physical.cch))
        return ERROR_OUTOFMEMORY;
    if (physical.pch[0] == L'[')
        return ERROR_SUCCESS;

    // A value that continues ends its line in a backslash; a closing quote
    // ends every complete one.
    while (current + 1 < pLines->count)
    {
        VReg__WStringView joined = VReg__TrimEnd(VReg__WStringViewOf(&pParser->text));
        VReg__WStringView next;
        if (joined.cch == 0 || joined.pch[joined.cch - 1] != L'\\')
            break;

        VReg__WStringTruncate(&pParser->text, joined.cch - 1);

        ++current;
        next = VREG__AT(pLines, VReg__WStringView, current);
        segment.start = pParser->text.cch;
        segment.line = current + 1;
        segment.column = 1;
        while (next.cch != 0 && VReg__IsBlank(next.pch[0]))
        {
            ++next.pch;
            --next.cch;
            ++segment.column;
        }
        if (
            !VReg__ArrayAppend(pAllocContext, &pParser->segments, sizeof(segment), &segment)
            || !VReg__WStringAppend(pAllocContext, &pParser->text, next.pch, next.cch)
        )
            return ERROR_OUTOFMEMORY;
    }

    *pLast = current;
    return ERROR_SUCCESS;
}

static LSTATUS VReg__ParseKey(_Inout_ VReg__Parser* const pParser)
{
    void* const pAllocContext = pParser->pAllocContext;
    const VReg__WStringView text = VReg__WStringViewOf(&pParser->text);
    SIZE_T close = text.cch;
    BOOL isDelete;
    VReg__WStringView full;
    SIZE_T slash;
    VReg__WStringView path;
    HKEY hRoot = NULL;
    LSTATUS status;

    while (close > 0 && text.pch[close - 1] != L']')
        --close;
    if (close == 0)
    {
        pParser->pos = text.cch;
        return VReg__ParseFail(pParser, "missing ] after the key name");
    }

    // `close` is one past the bracket: the position that ends the key.
    pParser->pos = close;
    status = VReg__ExpectEnd(pParser);
    if (status != ERROR_SUCCESS)
        return status;

    --close;
    pParser->pos = 1;
    isDelete = text.pch[pParser->pos] == L'-';
    if (isDelete)
        ++pParser->pos;

    full = VReg__Substring(text, pParser->pos, close - pParser->pos);
    slash = VReg__FindChar(full, 0, L'\\');
    if (!VReg__FindRoot(VReg__Substring(full, 0, slash), &hRoot))
        return VReg__ParseFail(pParser, "unknown registry root");

    path = slash < full.cch ? VReg__Substring(full, slash + 1, full.cch - slash - 1) : VReg__Substring(full, 0, 0);
    if (isDelete && path.cch == 0)
        return VReg__ParseFail(pParser, "a root key cannot be deleted");

    status = VReg__ScriptAppend(
        pParser->pScript,
        isDelete ? VREG_OPERATION_DELETE_KEY : VREG_OPERATION_CREATE_KEY,
        hRoot,
        path,
        VReg__ViewOf(L""),
        0,
        NULL,
        0,
        pParser->line
    );
    if (status != ERROR_SUCCESS)
        return status;

    pParser->state = isDelete ? VREG__KEY_DELETED : VREG__KEY_OPEN;
    pParser->hRoot = hRoot;
    return VReg__WStringAssign(pAllocContext, &pParser->path, path.pch, path.cch) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

/// Reads a quoted string at the position (on its opening quote) into @p pText.
static LSTATUS VReg__ParseQuoted(_Inout_ VReg__Parser* const pParser, _Inout_ VReg__WString* const pText)
{
    void* const pAllocContext = pParser->pAllocContext;
    ++pParser->pos;
    if (!VReg__WStringResize(pAllocContext, pText, 0))
        return ERROR_OUTOFMEMORY;

    for (;;)
    {
        wchar_t ch;
        if (VReg__AtEnd(pParser))
            return VReg__ParseFail(pParser, "unterminated string");

        ch = VReg__Current(pParser);
        if (ch == L'"')
        {
            ++pParser->pos;
            return ERROR_SUCCESS;
        }

        // Any escape but `\\` and `\"` is a syntax error, so a typo cannot
        // silently change data; regedit is more lenient here.
        if (ch == L'\\')
        {
            ++pParser->pos;
            if (VReg__AtEnd(pParser) || (VReg__Current(pParser) != L'\\' && VReg__Current(pParser) != L'"'))
            {
                --pParser->pos;
                return VReg__ParseFail(pParser, "invalid escape, only \\\\ and \\\" are allowed");
            }

            ch = VReg__Current(pParser);
        }

        if (!VReg__WStringAppendChar(pAllocContext, pText, ch))
            return ERROR_OUTOFMEMORY;

        ++pParser->pos;
    }
}

static LSTATUS VReg__ParseHexNumber(_Inout_ VReg__Parser* const pParser, const SIZE_T maxDigits, _Out_ DWORD* const pdwValue)
{
    DWORD dwValue = 0;
    SIZE_T digits = 0;
    *pdwValue = 0;
    while (!VReg__AtEnd(pParser) && VReg__HexDigit(VReg__Current(pParser)) >= 0)
    {
        if (++digits > maxDigits)
            return VReg__ParseFail(pParser, "too many hex digits");

        dwValue = (dwValue << 4) | (DWORD)VReg__HexDigit(VReg__Current(pParser));
        ++pParser->pos;
    }
    if (digits == 0)
        return VReg__ParseFail(pParser, "expected a hex number");

    *pdwValue = dwValue;
    return ERROR_SUCCESS;
}

static LSTATUS VReg__ParseHexBytes(_Inout_ VReg__Parser* const pParser, _Inout_ VReg__Buffer* const pData)
{
    VReg__SkipBlanks(pParser);
    if (VReg__AtEnd(pParser))
        return ERROR_SUCCESS;

    for (;;)
    {
        DWORD dwByte;
        BYTE byte;
        LSTATUS status;
        if (VReg__AtEnd(pParser) || VReg__HexDigit(VReg__Current(pParser)) < 0)
            return VReg__ParseFail(pParser, "expected a hex byte");

        status = VReg__ParseHexNumber(pParser, 2, &dwByte);
        if (status != ERROR_SUCCESS)
            return status;

        byte = (BYTE)dwByte;
        if (!VReg__BufferAppend(pParser->pAllocContext, pData, &byte, 1))
            return ERROR_OUTOFMEMORY;

        VReg__SkipBlanks(pParser);
        if (VReg__AtEnd(pParser))
            return ERROR_SUCCESS;
        if (VReg__Current(pParser) != L',')
            return VReg__ParseFail(pParser, "expected , between hex bytes");

        ++pParser->pos;
        VReg__SkipBlanks(pParser);
    }
}

static LSTATUS VReg__ParseValue(_Inout_ VReg__Parser* const pParser)
{
    void* const pAllocContext = pParser->pAllocContext;
    VReg__WString name = {NULL, 0, 0};
    VReg__WString text = {NULL, 0, 0};
    VReg__Buffer data = {NULL, 0, 0};
    VRegOperationKind kind = VREG_OPERATION_SET_VALUE;
    DWORD dwType = 0;
    LSTATUS status = ERROR_SUCCESS;

    if (VReg__Current(pParser) == L'@')
        ++pParser->pos;
    else if (VReg__Current(pParser) == L'"')
        status = VReg__ParseQuoted(pParser, &name);
    else
        status = VReg__ParseFail(pParser, "expected a value name or a [key]");
    if (status != ERROR_SUCCESS)
        goto done;

    if (pParser->state == VREG__KEY_NONE)
    {
        pParser->pos = 0;
        status = VReg__ParseFail(pParser, "a value outside any key");
        goto done;
    }

    // A value under a [-key] line is an error rather than silently dropped,
    // since it can only be a mistake.
    if (pParser->state == VREG__KEY_DELETED)
    {
        pParser->pos = 0;
        status = VReg__ParseFail(pParser, "a value after a deleted key");
        goto done;
    }

    VReg__SkipBlanks(pParser);
    if (VReg__AtEnd(pParser) || VReg__Current(pParser) != L'=')
    {
        status = VReg__ParseFail(pParser, "expected =");
        goto done;
    }

    ++pParser->pos;
    VReg__SkipBlanks(pParser);

    if (!VReg__AtEnd(pParser) && VReg__Current(pParser) == L'-')
    {
        ++pParser->pos;
        status = VReg__ExpectEnd(pParser);
        kind = VREG_OPERATION_DELETE_VALUE;
    }
    else if (!VReg__AtEnd(pParser) && VReg__Current(pParser) == L'"')
    {
        status = VReg__ParseQuoted(pParser, &text);
        if (status == ERROR_SUCCESS)
            status = VReg__ExpectEnd(pParser);
        if (status == ERROR_SUCCESS)
        {
            dwType = REG_SZ;
            if (!VReg__BufferAssign(pAllocContext, &data, VReg__WStringText(&text), (text.cch + 1) * sizeof(wchar_t)))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else if (VReg__Consume(pParser, L"dword:"))
    {
        DWORD dwValue = 0;
        status = VReg__ParseHexNumber(pParser, 8, &dwValue);
        if (status == ERROR_SUCCESS)
            status = VReg__ExpectEnd(pParser);
        if (status == ERROR_SUCCESS)
        {
            dwType = REG_DWORD;
            if (!VReg__BufferAssign(pAllocContext, &data, &dwValue, sizeof(dwValue)))
                status = ERROR_OUTOFMEMORY;
        }
    }
    else if (VReg__Consume(pParser, L"hex"))
    {
        dwType = REG_BINARY;
        if (VReg__Consume(pParser, L"("))
        {
            status = VReg__ParseHexNumber(pParser, 8, &dwType);
            if (status == ERROR_SUCCESS && !VReg__Consume(pParser, L")"))
                status = VReg__ParseFail(pParser, "expected ) after the value type");
        }
        if (status == ERROR_SUCCESS && !VReg__Consume(pParser, L":"))
            status = VReg__ParseFail(pParser, "expected : after hex");
        if (status == ERROR_SUCCESS)
            status = VReg__ParseHexBytes(pParser, &data);
    }
    else
    {
        status = VReg__ParseFail(pParser, "expected a string, dword:, hex: or -");
    }
    if (status != ERROR_SUCCESS)
        goto done;

    status = VReg__ScriptAppend(
        pParser->pScript,
        kind,
        pParser->hRoot,
        VReg__WStringViewOf(&pParser->path),
        VReg__WStringViewOf(&name),
        dwType,
        data.pb,
        (DWORD)data.cb,
        pParser->line
    );

done:
    VReg__WStringFree(pAllocContext, &name);
    VReg__WStringFree(pAllocContext, &text);
    VReg__BufferFree(pAllocContext, &data);
    return status;
}

static LSTATUS VReg__ParseLogicalLine(_Inout_ VReg__Parser* const pParser)
{
    // Key paths and value names reach the registry as C strings, where a NUL
    // would silently cut them short ("[-HKCU\<NUL>x]" would delete all of
    // HKCU); no valid .reg text holds one anywhere.
    pParser->pos = VReg__FindChar(VReg__WStringViewOf(&pParser->text), 0, L'\0');
    if (pParser->pos < pParser->text.cch)
        return VReg__ParseFail(pParser, "a NUL character");

    pParser->pos = 0;
    return pParser->text.psz[0] == L'[' ? VReg__ParseKey(pParser) : VReg__ParseValue(pParser);
}

/// Parses all of @p text into the parser's script. A syntax error answers
/// @ref ERROR_INVALID_DATA with @p pError filled in.
static LSTATUS VReg__ParseText(
    _Inout_ VReg__Parser* const pParser,
    const VReg__WStringView text,
    _Inout_opt_ VRegParseError* const pError
)
{
    void* const pAllocContext = pParser->pAllocContext;
    VReg__Array lines = {NULL, 0, 0};
    VReg__WStringView header;
    SIZE_T start;
    SIZE_T i;
    LSTATUS status = ERROR_SUCCESS;

    for (start = 0; start <= text.cch;)
    {
        const SIZE_T end = VReg__FindChar(text, start, L'\n');
        VReg__WStringView line = VReg__Substring(text, start, end - start);
        if (line.cch != 0 && line.pch[line.cch - 1] == L'\r')
            --line.cch;
        if (!VReg__ArrayAppend(pAllocContext, &lines, sizeof(line), &line))
        {
            status = ERROR_OUTOFMEMORY;
            goto done;
        }

        start = end + 1;
    }

    // The loop above appended the first line, however short the text.
    _Analysis_assume_(lines.p != NULL);
    header = VReg__TrimEnd(VREG__AT(&lines, VReg__WStringView, 0));
    if (
        !VReg__ViewEquals(header, VReg__ViewOf(L"Windows Registry Editor Version 5.00"))
        && !VReg__ViewEquals(header, VReg__ViewOf(L"REGEDIT4"))
    )
    {
        if (pError)
        {
            pError->line = 1;
            pError->column = 1;
            pError->pszMessage = "missing or unknown registry editor header";
        }

        status = ERROR_INVALID_DATA;
        goto done;
    }

    for (i = 1; i < lines.count; ++i)
    {
        SIZE_T last = i;
        pParser->line = i + 1;
        status = VReg__ReadLogicalLine(pParser, &lines, i, &last);
        if (status == ERROR_SUCCESS && pParser->text.cch != 0)
            status = VReg__ParseLogicalLine(pParser);
        if (status == ERROR_INVALID_DATA && pError)
            VReg__ParserPosition(pParser, pParser->errorOffset, &pError->line, &pError->column);
        if (status == ERROR_INVALID_DATA && pError)
            pError->pszMessage = pParser->pszError;
        if (status != ERROR_SUCCESS)
            goto done;

        i = last;
    }

done:
    VReg__ArrayFree(pAllocContext, &lines);
    return status;
}

LSTATUS WINAPI VRegParseScript(
    _In_reads_bytes_(cbFile) const void* const pFile,
    _In_ const SIZE_T cbFile,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegScript** const ppScript,
    _Out_opt_ VRegParseError* const pError
)
{
    VReg__WString text = {NULL, 0, 0};
    VReg__Parser parser;
    LSTATUS status;

    *ppScript = NULL;
    if (pError)
        ZeroMemory(pError, sizeof(*pError));

    status = VReg__DecodeFile(pAllocContext, pFile, cbFile, &text, pError);
    if (status != ERROR_SUCCESS)
    {
        VReg__WStringFree(pAllocContext, &text);
        return status;
    }

    ZeroMemory(&parser, sizeof(parser));
    parser.pAllocContext = pAllocContext;
    status = VReg__ScriptCreate(pAllocContext, &parser.pScript);
    if (status == ERROR_SUCCESS)
        status = VReg__ParseText(&parser, VReg__WStringViewOf(&text), pError);

    VReg__WStringFree(pAllocContext, &parser.text);
    VReg__WStringFree(pAllocContext, &parser.path);
    VReg__ArrayFree(pAllocContext, &parser.segments);
    VReg__WStringFree(pAllocContext, &text);
    if (status != ERROR_SUCCESS)
    {
        VRegDestroyScript(parser.pScript);
        return status;
    }

    *ppScript = parser.pScript;
    return ERROR_SUCCESS;
}

LSTATUS WINAPI VRegParseScriptFile(
    _In_z_ const LPCWSTR pszPath,
    _In_opt_ void* const pAllocContext,
    _Outptr_ VRegScript** const ppScript,
    _Out_opt_ VRegParseError* const pError
)
{
    VReg__Buffer file = {NULL, 0, 0};
    LSTATUS status;

    *ppScript = NULL;
    if (pError)
        ZeroMemory(pError, sizeof(*pError));
    status = VReg__ReadWholeFile(pAllocContext, pszPath, &file);
    if (status == ERROR_SUCCESS)
        status = VRegParseScript(file.pb, file.cb, pAllocContext, ppScript, pError);

    VReg__BufferFree(pAllocContext, &file);
    return status;
}
