#include "writer.h"

#include "text.h"

#include <cstdio>
#include <cstring>
#include <new>

namespace VReg::RegFile
{
namespace
{
// Agent's choice: Wine regedit's wrapping (break after the comma that reaches this column, continuation lines
// indented by two spaces), not yet checked byte for byte against Windows regedit.
constexpr std::size_t kHexWrapColumn = 76;
constexpr const wchar_t* kNewline = L"\r\n";

bool HasLineBreak(const std::wstring& text)
{
    return text.find_first_of(L"\r\n") != std::wstring::npos;
}

void AppendQuoted(std::wstring& out, const std::wstring& text)
{
    out.push_back(L'"');
    for (const wchar_t ch : text)
    {
        if (ch == L'\\' || ch == L'"')
            out.push_back(L'\\');
        out.push_back(ch);
    }
    out.push_back(L'"');
}

/// The text a REG_SZ takes in the friendly form; false when the form would not reproduce the bytes.
bool FriendlyString(const std::vector<std::byte>& data, std::wstring& text)
{
    if (data.size() < sizeof(wchar_t) || data.size() % sizeof(wchar_t) != 0)
        return false;
    text.resize(data.size() / sizeof(wchar_t));
    std::memcpy(text.data(), data.data(), data.size());
    if (text.back() != L'\0')
        return false;
    text.pop_back();
    return text.find_first_of(std::wstring_view(L"\0\r\n", 3)) == std::wstring::npos;
}

void AppendHex(std::wstring& out, const Record& record, std::size_t column)
{
    wchar_t szHeader[16];
    const int cchHeader = record.dwType == REG_BINARY ? swprintf_s(szHeader, L"hex:")
                                                      : swprintf_s(szHeader, L"hex(%lx):", record.dwType);
    out.append(szHeader, static_cast<std::size_t>(cchHeader));
    column += static_cast<std::size_t>(cchHeader);

    for (std::size_t i = 0; i < record.data.size(); ++i)
    {
        wchar_t szByte[4];
        swprintf_s(szByte, L"%02x", static_cast<unsigned>(record.data[i]));
        out.append(szByte, 2);
        column += 2;
        if (i + 1 == record.data.size())
            break;
        out.push_back(L',');
        if (++column >= kHexWrapColumn)
        {
            out.append(L"\\\r\n  ");
            column = 2;
        }
    }
}

void AppendValue(std::wstring& out, const Record& record)
{
    const std::size_t lineStart = out.size();
    if (record.name.empty())
        out.push_back(L'@');
    else
        AppendQuoted(out, record.name);
    out.push_back(L'=');

    std::wstring text;
    if (record.kind == Record::Kind::DeleteValue)
        out.push_back(L'-');
    else if (record.dwType == REG_SZ && FriendlyString(record.data, text))
        AppendQuoted(out, text);
    else if (record.dwType == REG_DWORD && record.data.size() == sizeof(DWORD))
    {
        DWORD dwValue{};
        std::memcpy(&dwValue, record.data.data(), sizeof(dwValue));
        wchar_t szValue[16];
        swprintf_s(szValue, L"dword:%08lx", dwValue);
        out.append(szValue);
    }
    else
        AppendHex(out, record, out.size() - lineStart);
    out.append(kNewline);
}
} // namespace

LSTATUS WriteRecords(const std::span<const Record> records, const Format format, std::vector<std::byte>& file)
{
    try
    {
        std::wstring out = format == Format::Regedit4 ? L"REGEDIT4" : L"Windows Registry Editor Version 5.00";
        out.append(kNewline);
        out.append(kNewline);

        for (const auto& record : records)
        {
            if (HasLineBreak(record.key) || HasLineBreak(record.name))
                return ERROR_INVALID_NAME;
            switch (record.kind)
            {
            case Record::Kind::Key:
            case Record::Kind::DeleteKey:
                if (&record != records.data())
                    out.append(kNewline);
                out.append(record.kind == Record::Kind::Key ? L"[" : L"[-");
                out.append(record.key);
                out.append(L"]");
                out.append(kNewline);
                break;
            case Record::Kind::Value:
            case Record::Kind::DeleteValue:
                AppendValue(out, record);
                break;
            }
        }
        if (!records.empty())
            out.append(kNewline);
        return EncodeFile(out, format, file);
    }
    catch (const std::bad_alloc&)
    {
        return ERROR_OUTOFMEMORY;
    }
}
} // namespace VReg::RegFile
