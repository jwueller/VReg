#include "text.h"

#include <climits>
#include <cstring>
#include <string_view>

namespace VReg::RegFile
{
namespace
{
constexpr std::string_view kRegedit4 = "REGEDIT4";

LSTATUS Fail(
    ParseError* const pError,
    const char* const pszMessage
)
{
    if (pError)
        *pError = {1, 1, pszMessage};
    return ERROR_INVALID_DATA;
}

bool StartsWith(
    const std::span<const std::byte> bytes,
    const std::string_view prefix
)
{
    return bytes.size() >= prefix.size()
        && std::memcmp(bytes.data(), prefix.data(), prefix.size()) == 0;
}

LSTATUS Widen(
    const UINT codePage,
    const std::span<const std::byte> bytes,
    std::wstring& text,
    ParseError* const pError
)
{
    text.clear();
    if (bytes.empty())
        return ERROR_SUCCESS;
    const DWORD dwFlags = codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
    const auto pchBytes = reinterpret_cast<const char*>(bytes.data());
    const int cchText = MultiByteToWideChar(
        codePage,
        dwFlags,
        pchBytes,
        static_cast<int>(bytes.size()),
        nullptr,
        0
    );
    if (cchText <= 0)
        return Fail(pError, "the text is not valid in its encoding");
    text.resize(static_cast<std::size_t>(cchText));
    MultiByteToWideChar(
        codePage,
        dwFlags,
        pchBytes,
        static_cast<int>(bytes.size()),
        text.data(),
        cchText
    );
    return ERROR_SUCCESS;
}
} // namespace

LSTATUS DecodeFile(
    const std::span<const std::byte> file,
    std::wstring& text,
    ParseError* const pError
)
{
    if (file.size() > static_cast<std::size_t>(INT_MAX))
        return Fail(pError, "the file is too large");

    if (file.size() >= 2 && file[0] == std::byte{0xFF} && file[1] == std::byte{0xFE})
    {
        const auto body = file.subspan(2);
        if (body.size() % sizeof(wchar_t) != 0)
            return Fail(pError, "a UTF-16 file has an odd byte count");
        text.resize(body.size() / sizeof(wchar_t));
        std::memcpy(text.data(), body.data(), body.size());
        return ERROR_SUCCESS;
    }
    if (file.size() >= 2 && file[0] == std::byte{0xFE} && file[1] == std::byte{0xFF})
        return Fail(pError, "big-endian UTF-16 is not supported");
    if (file.size() >= 3 && file[0] == std::byte{0xEF} && file[1] == std::byte{0xBB} && file[2] == std::byte{0xBF})
        return Widen(CP_UTF8, file.subspan(3), text, pError);

    return Widen(
        StartsWith(file, kRegedit4) ? CP_ACP : CP_UTF8,
        file,
        text,
        pError
    );
}

LSTATUS EncodeFile(
    const std::wstring& text,
    const Format format,
    std::vector<std::byte>& file
)
{
    file.clear();
    if (format == Format::Unicode)
    {
        file.resize(sizeof(wchar_t) + text.size() * sizeof(wchar_t));
        file[0] = std::byte{0xFF};
        file[1] = std::byte{0xFE};
        std::memcpy(
            file.data() + sizeof(wchar_t),
            text.data(),
            text.size() * sizeof(wchar_t)
        );
        return ERROR_SUCCESS;
    }
    if (text.size() > static_cast<std::size_t>(INT_MAX))
        return ERROR_NOT_ENOUGH_MEMORY;

    // An ACP of UTF-8 takes neither the best-fit flag nor a default-char report.
    const UINT codePage = format == Format::Utf8 ? CP_UTF8 : CP_ACP;
    const bool isUtf8 = codePage == CP_UTF8 || GetACP() == CP_UTF8;
    const DWORD dwFlags = isUtf8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
    const int cchText = static_cast<int>(text.size());
    BOOL fUsedDefault = FALSE;
    const auto pfUsedDefault = isUtf8 ? nullptr : &fUsedDefault;
    if (cchText == 0)
        return ERROR_SUCCESS;
    const int cbFile = WideCharToMultiByte(
        codePage,
        dwFlags,
        text.data(),
        cchText,
        nullptr,
        0,
        nullptr,
        pfUsedDefault
    );
    if (cbFile <= 0 || fUsedDefault)
        return ERROR_NO_UNICODE_TRANSLATION;
    file.resize(static_cast<std::size_t>(cbFile));
    WideCharToMultiByte(
        codePage,
        dwFlags,
        text.data(),
        cchText,
        reinterpret_cast<char*>(file.data()),
        cbFile,
        nullptr,
        pfUsedDefault
    );
    if (fUsedDefault)
    {
        file.clear();
        return ERROR_NO_UNICODE_TRANSLATION;
    }
    return ERROR_SUCCESS;
}

LSTATUS ReadWholeFile(
    const std::filesystem::path& path,
    std::vector<std::byte>& file
)
{
    const HANDLE hFile = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE)
        return static_cast<LSTATUS>(GetLastError());

    LARGE_INTEGER size{};
    LSTATUS status = ERROR_SUCCESS;
    if (!GetFileSizeEx(hFile, &size))
        status = static_cast<LSTATUS>(GetLastError());
    else if (size.QuadPart > INT_MAX)
        status = ERROR_FILE_TOO_LARGE;
    else
    {
        file.resize(static_cast<std::size_t>(size.QuadPart));
        DWORD cbRead = 0;
        if (!ReadFile(hFile, file.data(), static_cast<DWORD>(file.size()), &cbRead, nullptr))
            status = static_cast<LSTATUS>(GetLastError());
        else
            file.resize(cbRead);
    }
    CloseHandle(hFile);
    return status;
}

LSTATUS WriteWholeFile(
    const std::filesystem::path& path,
    const std::span<const std::byte> file
)
{
    const HANDLE hFile = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE)
        return static_cast<LSTATUS>(GetLastError());

    LSTATUS status = ERROR_SUCCESS;
    DWORD cbWritten = 0;
    if (!WriteFile(hFile, file.data(), static_cast<DWORD>(file.size()), &cbWritten, nullptr))
        status = static_cast<LSTATUS>(GetLastError());
    if (!CloseHandle(hFile) && status == ERROR_SUCCESS)
        status = static_cast<LSTATUS>(GetLastError());
    return status;
}
} // namespace VReg::RegFile
