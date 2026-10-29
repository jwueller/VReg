#include <VReg/regfile.h>

#include "core/status.h"
#include "text.h"

#include <cstring>
#include <new>
#include <string_view>

namespace VReg::RegFile
{
namespace
{
struct SyntaxError
{
    /// Into the logical line being parsed.
    std::size_t offset;
    const char* pszMessage;
};

/// A line of the file with its continuations joined. Offsets into @ref text map back to the physical
/// lines the text came from.
struct LogicalLine
{
    struct Segment
    {
        std::size_t start;
        std::size_t line;
        std::size_t column;
    };

    std::wstring text;
    std::vector<Segment> segments;

    void Position(
        const std::size_t offset,
        std::size_t& line,
        std::size_t& column
    ) const
    {
        const Segment* pSegment = &segments.front();
        for (const auto& segment : segments)
        {
            if (segment.start > offset)
                break;
            pSegment = &segment;
        }
        line = pSegment->line;
        column = pSegment->column + (offset - pSegment->start);
    }
};

bool IsBlank(const wchar_t ch)
{
    return ch == L' ' || ch == L'\t';
}

int HexDigit(const wchar_t ch)
{
    if (ch >= L'0' && ch <= L'9')
        return ch - L'0';
    if (ch >= L'a' && ch <= L'f')
        return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F')
        return ch - L'A' + 10;
    return -1;
}

wchar_t Lower(const wchar_t ch)
{
    return (ch >= L'A' && ch <= L'Z') ? static_cast<wchar_t>(ch - L'A' + L'a') : ch;
}

bool EqualsIgnoreCase(const std::wstring_view a, const std::wstring_view b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (Lower(a[i]) != Lower(b[i]))
            return false;
    }
    return true;
}

std::wstring_view TrimEnd(std::wstring_view text)
{
    while (!text.empty() && IsBlank(text.back()))
        text.remove_suffix(1);
    return text;
}

bool FindRoot(const std::wstring_view name, HKEY& hRoot)
{
    struct Root
    {
        const wchar_t* pszLong;
        const wchar_t* pszShort;
        HKEY hKey;
    };
    const Root roots[] = {
        {L"HKEY_CLASSES_ROOT", L"HKCR", HKEY_CLASSES_ROOT},
        {L"HKEY_CURRENT_USER", L"HKCU", HKEY_CURRENT_USER},
        {L"HKEY_LOCAL_MACHINE", L"HKLM", HKEY_LOCAL_MACHINE},
        {L"HKEY_USERS", L"HKU", HKEY_USERS},
        {L"HKEY_CURRENT_CONFIG", L"HKCC", HKEY_CURRENT_CONFIG},
    };
    for (const auto& root : roots)
    {
        if (EqualsIgnoreCase(name, root.pszLong) || EqualsIgnoreCase(name, root.pszShort))
        {
            hRoot = root.hKey;
            return true;
        }
    }
    return false;
}

class Parser
{
public:
    explicit Parser(Script& script) noexcept :
        m_script(script)
    {
    }

    /// Parses all of @p text, throwing nothing but std::bad_alloc; a syntax error answers false.
    bool Run(const std::wstring& text, ParseError* const pError)
    {
        std::vector<std::wstring_view> lines;
        for (std::size_t start = 0; start <= text.size();)
        {
            auto end = text.find(L'\n', start);
            if (end == std::wstring::npos)
                end = text.size();
            auto line = std::wstring_view(text).substr(start, end - start);
            if (!line.empty() && line.back() == L'\r')
                line.remove_suffix(1);
            lines.push_back(line);
            start = end + 1;
        }

        const auto header = TrimEnd(lines[0]);
        if (header != L"Windows Registry Editor Version 5.00" && header != L"REGEDIT4")
            return Fail(
                pError,
                1,
                1,
                "missing or unknown registry editor header"
            );

        for (std::size_t i = 1; i < lines.size(); ++i)
        {
            LogicalLine line;
            try
            {
                m_line = i + 1;
                i = ReadLine(lines, i, line);
                if (!line.text.empty())
                    ParseLine(line);
            }
            catch (const SyntaxError& error)
            {
                std::size_t lineNumber = 0;
                std::size_t column = 0;
                line.Position(error.offset, lineNumber, column);
                return Fail(pError, lineNumber, column, error.pszMessage);
            }
        }
        return true;
    }

private:
    enum class KeyState
    {
        None,
        Open,
        Deleted,
    };

    static bool Fail(
        ParseError* const pError,
        const std::size_t line,
        const std::size_t column,
        const char* pszMessage
    )
    {
        if (pError)
            *pError = {line, column, pszMessage};
        return false;
    }

    /// Reads the logical line starting at @p index into @p line (empty for blank and comment lines) and
    /// returns the index of its last physical line.
    static std::size_t ReadLine(
        const std::vector<std::wstring_view>& lines,
        std::size_t index,
        LogicalLine& line
    )
    {
        auto physical = lines[index];
        std::size_t column = 1;
        while (!physical.empty() && IsBlank(physical.front()))
        {
            physical.remove_prefix(1);
            ++column;
        }
        line.segments.push_back({0, index + 1, column});
        if (physical.empty() || physical.front() == L';')
            return index;

        line.text = physical;
        if (physical.front() == L'[')
            return index;

        // A value that continues ends its line in a backslash; a closing quote ends every complete one.
        while (index + 1 < lines.size())
        {
            auto joined = TrimEnd(line.text);
            if (joined.empty() || joined.back() != L'\\')
                break;
            joined.remove_suffix(1);
            line.text.resize(joined.size());

            auto next = lines[++index];
            column = 1;
            while (!next.empty() && IsBlank(next.front()))
            {
                next.remove_prefix(1);
                ++column;
            }
            line.segments.push_back({line.text.size(), index + 1, column});
            line.text.append(next);
        }
        return index;
    }

    void ParseLine(const LogicalLine& line)
    {
        m_text = line.text;

        // Key paths and value names reach the registry as C strings, where a NUL would silently cut them short
        // ("[-HKCU\<NUL>x]" would delete all of HKCU); no valid .reg text holds one anywhere.
        m_pos = m_text.find(L'\0');
        if (m_pos != std::wstring::npos)
            Throw("a NUL character");

        m_pos = 0;
        if (m_text.front() == L'[')
            ParseKey();
        else
            ParseValue();
    }

    [[noreturn]] void Throw(const char* const pszMessage) const
    {
        throw SyntaxError{m_pos, pszMessage};
    }

    void SkipBlanks()
    {
        while (m_pos < m_text.size() && IsBlank(m_text[m_pos]))
            ++m_pos;
    }

    bool AtEnd() const
    {
        return m_pos >= m_text.size();
    }

    void ExpectEnd()
    {
        SkipBlanks();
        if (!AtEnd())
            Throw("unexpected text at the end of the line");
    }

    bool Consume(const std::wstring_view token)
    {
        if (m_text.size() - m_pos < token.size()
            || !EqualsIgnoreCase(m_text.substr(m_pos, token.size()), token))
            return false;
        m_pos += token.size();
        return true;
    }

    void ParseKey()
    {
        const auto close = m_text.rfind(L']');
        if (close == std::wstring::npos)
        {
            m_pos = m_text.size();
            Throw("missing ] after the key name");
        }
        m_pos = close + 1;
        ExpectEnd();

        m_pos = 1;
        const bool isDelete = m_text[m_pos] == L'-';
        if (isDelete)
            ++m_pos;
        const std::wstring_view full = std::wstring_view(m_text).substr(
            m_pos,
            close - m_pos
        );
        const auto slash = full.find(L'\\');
        const auto rootName = full.substr(0, slash);
        HKEY hRoot{};
        if (!FindRoot(rootName, hRoot))
            Throw("unknown registry root");
        std::wstring path;
        if (slash != std::wstring_view::npos)
            path = full.substr(slash + 1);
        if (isDelete && path.empty())
            Throw("a root key cannot be deleted");

        Operation operation;
        operation.kind = isDelete ? OperationKind::DeleteKey : OperationKind::CreateKey;
        operation.hRoot = hRoot;
        operation.path = path;
        operation.line = m_line;
        m_script.operations.push_back(std::move(operation));

        m_state = isDelete ? KeyState::Deleted : KeyState::Open;
        m_hRoot = hRoot;
        m_path = std::move(path);
    }

    std::wstring ParseQuoted()
    {
        ++m_pos;
        std::wstring text;
        for (;;)
        {
            if (AtEnd())
                Throw("unterminated string");
            const wchar_t ch = m_text[m_pos];
            if (ch == L'"')
            {
                ++m_pos;
                return text;
            }
            // Agent's choice: any escape but \\ and \" is a syntax error, so a typo cannot silently change data;
            // regedit's leniency here was passed over.
            if (ch == L'\\')
            {
                ++m_pos;
                if (AtEnd() || (m_text[m_pos] != L'\\' && m_text[m_pos] != L'"'))
                {
                    --m_pos;
                    Throw("invalid escape, only \\\\ and \\\" are allowed");
                }
                text.push_back(m_text[m_pos]);
            }
            else
                text.push_back(ch);
            ++m_pos;
        }
    }

    DWORD ParseHexNumber(const std::size_t maxDigits)
    {
        DWORD dwValue = 0;
        std::size_t digits = 0;
        while (!AtEnd() && HexDigit(m_text[m_pos]) >= 0)
        {
            if (++digits > maxDigits)
                Throw("too many hex digits");
            dwValue = (dwValue << 4) | static_cast<DWORD>(HexDigit(m_text[m_pos]));
            ++m_pos;
        }
        if (digits == 0)
            Throw("expected a hex number");
        return dwValue;
    }

    void ParseHexBytes(std::vector<std::byte>& data)
    {
        SkipBlanks();
        if (AtEnd())
            return;
        for (;;)
        {
            if (AtEnd() || HexDigit(m_text[m_pos]) < 0)
                Throw("expected a hex byte");
            data.push_back(static_cast<std::byte>(ParseHexNumber(2)));
            SkipBlanks();
            if (AtEnd())
                return;
            if (m_text[m_pos] != L',')
                Throw("expected , between hex bytes");
            ++m_pos;
            SkipBlanks();
        }
    }

    void ParseValue()
    {
        Operation operation;
        operation.line = m_line;
        if (m_text[m_pos] == L'@')
            ++m_pos;
        else if (m_text[m_pos] == L'"')
            operation.name = ParseQuoted();
        else
            Throw("expected a value name or a [key]");

        if (m_state == KeyState::None)
        {
            m_pos = 0;
            Throw("a value outside any key");
        }
        // Agent's choice: a value under a [-key] line is an error rather than silently dropped, since it can only be
        // a mistake.
        if (m_state == KeyState::Deleted)
        {
            m_pos = 0;
            Throw("a value after a deleted key");
        }

        SkipBlanks();
        if (AtEnd() || m_text[m_pos] != L'=')
            Throw("expected =");
        ++m_pos;
        SkipBlanks();

        operation.hRoot = m_hRoot;
        operation.path = m_path;
        if (!AtEnd() && m_text[m_pos] == L'-')
        {
            ++m_pos;
            ExpectEnd();
            operation.kind = OperationKind::DeleteValue;
        }
        else if (!AtEnd() && m_text[m_pos] == L'"')
        {
            const auto text = ParseQuoted();
            ExpectEnd();
            operation.kind = OperationKind::SetValue;
            operation.dwType = REG_SZ;
            operation.data.resize((text.size() + 1) * sizeof(wchar_t));
            std::memcpy(
                operation.data.data(),
                text.c_str(),
                operation.data.size()
            );
        }
        else if (Consume(L"dword:"))
        {
            const DWORD dwValue = ParseHexNumber(8);
            ExpectEnd();
            operation.kind = OperationKind::SetValue;
            operation.dwType = REG_DWORD;
            operation.data.resize(sizeof(dwValue));
            std::memcpy(operation.data.data(), &dwValue, sizeof(dwValue));
        }
        else if (Consume(L"hex"))
        {
            operation.kind = OperationKind::SetValue;
            operation.dwType = REG_BINARY;
            if (Consume(L"("))
            {
                operation.dwType = ParseHexNumber(8);
                if (!Consume(L")"))
                    Throw("expected ) after the value type");
            }
            if (!Consume(L":"))
                Throw("expected : after hex");
            ParseHexBytes(operation.data);
        }
        else
            Throw("expected a string, dword:, hex: or -");

        m_script.operations.push_back(std::move(operation));
    }

    Script& m_script;
    std::wstring m_text;
    std::size_t m_pos{};
    std::size_t m_line{};
    KeyState m_state{KeyState::None};
    HKEY m_hRoot{};
    std::wstring m_path;
};
} // namespace

LSTATUS Parse(
    const std::span<const std::byte> file,
    Script& script,
    ParseError* const pError
) noexcept
{
    try
    {
        std::wstring text;
        const auto status = DecodeFile(file, text, pError);
        if (status != ERROR_SUCCESS)
            return status;
        Script parsed;
        Parser parser(parsed);
        if (!parser.Run(text, pError))
            return ERROR_INVALID_DATA;
        script = std::move(parsed);
        return ERROR_SUCCESS;
    }
    catch (...)
    {
        return StatusFromException();
    }
}

LSTATUS ParseFile(
    const std::filesystem::path& path,
    Script& script,
    ParseError* const pError
) noexcept
{
    try
    {
        std::vector<std::byte> file;
        const auto status = ReadWholeFile(path, file);
        if (status != ERROR_SUCCESS)
            return status;
        return Parse(file, script, pError);
    }
    catch (...)
    {
        return StatusFromException();
    }
}
} // namespace VReg::RegFile
