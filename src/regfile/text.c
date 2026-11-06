#include "regfile/script.h"

#include "core/alloc.h"

static LSTATUS VReg__DecodeFail(_Inout_opt_ VRegParseError* const pError, _In_z_ const char* const pszMessage)
{
    if (pError)
    {
        pError->line = 1;
        pError->column = 1;
        pError->pszMessage = pszMessage;
    }

    return ERROR_INVALID_DATA;
}

static BOOL VReg__StartsWithAscii(_In_reads_bytes_(cb) const BYTE* const pb, const SIZE_T cb, _In_z_ const char* const pszPrefix)
{
    SIZE_T i;
    for (i = 0; pszPrefix[i] != '\0'; ++i)
    {
        if (i >= cb || pb[i] != (BYTE)pszPrefix[i])
            return FALSE;
    }

    return TRUE;
}

static LSTATUS VReg__Widen(
    _In_opt_ void* const pAllocContext,
    const UINT codePage,
    _In_reads_bytes_(cb) const BYTE* const pb,
    const SIZE_T cb,
    _Inout_ VReg__WString* const pText,
    _Inout_opt_ VRegParseError* const pError
)
{
    const DWORD dwFlags = codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
    int cchText;
    if (!VReg__WStringResize(pAllocContext, pText, 0))
        return ERROR_OUTOFMEMORY;
    if (cb == 0)
        return ERROR_SUCCESS;

    cchText = MultiByteToWideChar(codePage, dwFlags, (const char*)pb, (int)cb, NULL, 0);
    if (cchText <= 0)
        return VReg__DecodeFail(pError, "the text is not valid in its encoding");
    if (!VReg__WStringResize(pAllocContext, pText, (SIZE_T)cchText))
        return ERROR_OUTOFMEMORY;

    MultiByteToWideChar(codePage, dwFlags, (const char*)pb, (int)cb, pText->psz, cchText);
    return ERROR_SUCCESS;
}

LSTATUS VReg__DecodeFile(
    _In_opt_ void* const pAllocContext,
    _In_reads_bytes_(cbFile) const void* const pFile,
    const SIZE_T cbFile,
    _Inout_ VReg__WString* const pText,
    _Inout_opt_ VRegParseError* const pError
)
{
    const BYTE* const pb = (const BYTE*)pFile;
    if (cbFile > (SIZE_T)MAXINT32)
        return VReg__DecodeFail(pError, "the file is too large");

    if (cbFile >= 2 && pb[0] == 0xFF && pb[1] == 0xFE)
    {
        const SIZE_T cbBody = cbFile - 2;
        if (cbBody % sizeof(wchar_t) != 0)
            return VReg__DecodeFail(pError, "a UTF-16 file has an odd byte count");
        if (!VReg__WStringAssign(pAllocContext, pText, (const wchar_t*)(pb + 2), cbBody / sizeof(wchar_t)))
            return ERROR_OUTOFMEMORY;

        return ERROR_SUCCESS;
    }
    if (cbFile >= 2 && pb[0] == 0xFE && pb[1] == 0xFF)
        return VReg__DecodeFail(pError, "big-endian UTF-16 is not supported");
    if (cbFile >= 3 && pb[0] == 0xEF && pb[1] == 0xBB && pb[2] == 0xBF)
        return VReg__Widen(pAllocContext, CP_UTF8, pb + 3, cbFile - 3, pText, pError);

    return VReg__Widen(
        pAllocContext,
        VReg__StartsWithAscii(pb, cbFile, "REGEDIT4") ? CP_ACP : CP_UTF8,
        pb,
        cbFile,
        pText,
        pError
    );
}

LSTATUS VReg__EncodeFile(
    _In_opt_ void* const pAllocContext,
    _In_ const VReg__WString* const pText,
    const VRegFormat format,
    _Inout_ VReg__Buffer* const pFile
)
{
    UINT codePage;
    BOOL isUtf8;
    DWORD dwFlags;
    BOOL fUsedDefault = FALSE;
    BOOL* pfUsedDefault;
    int cbFile;

    if (!VReg__BufferResize(pAllocContext, pFile, 0))
        return ERROR_OUTOFMEMORY;

    if (format == VREG_FORMAT_UNICODE)
    {
        static const BYTE BOM[2] = {0xFF, 0xFE};
        if (
            !VReg__BufferAppend(pAllocContext, pFile, BOM, sizeof(BOM))
            || !VReg__BufferAppend(pAllocContext, pFile, VReg__WStringText(pText), pText->cch * sizeof(wchar_t))
        )
            return ERROR_OUTOFMEMORY;

        return ERROR_SUCCESS;
    }
    if (pText->cch > (SIZE_T)MAXINT32)
        return ERROR_NOT_ENOUGH_MEMORY;

    // An ACP of UTF-8 takes neither the best-fit flag nor a default-char
    // report.
    codePage = format == VREG_FORMAT_UTF8 ? CP_UTF8 : CP_ACP;
    isUtf8 = codePage == CP_UTF8 || GetACP() == CP_UTF8;
    dwFlags = isUtf8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
    pfUsedDefault = isUtf8 ? NULL : &fUsedDefault;
    if (pText->cch == 0)
        return ERROR_SUCCESS;

    cbFile = WideCharToMultiByte(codePage, dwFlags, pText->psz, (int)pText->cch, NULL, 0, NULL, pfUsedDefault);
    if (cbFile <= 0 || fUsedDefault)
        return ERROR_NO_UNICODE_TRANSLATION;
    if (!VReg__BufferResize(pAllocContext, pFile, (SIZE_T)cbFile))
        return ERROR_OUTOFMEMORY;

    WideCharToMultiByte(codePage, dwFlags, pText->psz, (int)pText->cch, (char*)pFile->pb, cbFile, NULL, pfUsedDefault);
    if (fUsedDefault)
    {
        pFile->cb = 0;
        return ERROR_NO_UNICODE_TRANSLATION;
    }

    return ERROR_SUCCESS;
}

LSTATUS VReg__ReadWholeFile(_In_opt_ void* const pAllocContext, _In_z_ const LPCWSTR pszPath, _Inout_ VReg__Buffer* const pFile)
{
    LARGE_INTEGER size;
    LSTATUS status = ERROR_SUCCESS;
    const HANDLE hFile = CreateFileW(pszPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return (LSTATUS)GetLastError();

    size.QuadPart = 0;
    if (!GetFileSizeEx(hFile, &size))
    {
        status = (LSTATUS)GetLastError();
    }
    else if (size.QuadPart > MAXINT32)
    {
        status = ERROR_FILE_TOO_LARGE;
    }
    else if (!VReg__BufferResize(pAllocContext, pFile, (SIZE_T)size.QuadPart))
    {
        status = ERROR_OUTOFMEMORY;
    }
    else
    {
        DWORD cbRead = 0;
        if (!ReadFile(hFile, pFile->pb, (DWORD)pFile->cb, &cbRead, NULL))
            status = (LSTATUS)GetLastError();
        else
            pFile->cb = cbRead;
    }

    CloseHandle(hFile);
    return status;
}

LSTATUS VReg__WriteWholeFile(_In_z_ const LPCWSTR pszPath, _In_reads_bytes_(cbFile) const void* const pFile, const SIZE_T cbFile)
{
    LSTATUS status = ERROR_SUCCESS;
    DWORD cbWritten = 0;
    const HANDLE hFile = CreateFileW(pszPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return (LSTATUS)GetLastError();

    if (!WriteFile(hFile, pFile, (DWORD)cbFile, &cbWritten, NULL))
        status = (LSTATUS)GetLastError();
    if (!CloseHandle(hFile) && status == ERROR_SUCCESS)
        status = (LSTATUS)GetLastError();
    return status;
}
