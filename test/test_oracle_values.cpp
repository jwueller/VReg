// Value scenarios: set and query in every shape. See scenario.h.

#include "scenario.h"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <iterator>
#include <tuple>

using namespace VRegTest;

namespace
{
std::vector<StoredValue> WideValues()
{
    const wchar_t NON_ASCII[] = L"a\u00e4\u20ac\u0101\u4e2d";
    const wchar_t MULTI_NON_ASCII[] = L"\u00e4\0\u4e2d\0";
    const wchar_t EMBEDDED[] = L"ab\0cd";

    std::vector<StoredValue> values;
    // @formatter:off
    values.push_back({L"None",           REG_NONE, {1, 2, 3}});
    values.push_back({L"NoneEmpty",      REG_NONE, {}});
    values.push_back({L"Sz",             REG_SZ,   BytesOf(L"text", 5)});
    values.push_back({L"SzNoTerminator", REG_SZ,   BytesOf(L"text", 4)});
    // @formatter:on
    values.push_back(
        {
            L"SzOddBytes",
            REG_SZ,
            []
            {
                auto bytes = BytesOf(L"text", 4);
                bytes.push_back(0x41);
                return bytes;
            }()
        }
    );
    // @formatter:off
    values.push_back({L"SzOneByte",              REG_SZ,               {0x41}});
    values.push_back({L"SzEmpty",                REG_SZ,               {}});
    values.push_back({L"SzJustTerminator",       REG_SZ,               {0, 0}});
    values.push_back({L"SzNonAscii",             REG_SZ,               BytesOf(NON_ASCII, std::size(NON_ASCII))});
    values.push_back({L"SzNonAsciiNoTerminator", REG_SZ,               BytesOf(NON_ASCII, std::size(NON_ASCII) - 1)});
    values.push_back({L"SzEmbeddedNul",          REG_SZ,               BytesOf(EMBEDDED, std::size(EMBEDDED))});
    values.push_back({L"Expand",                 REG_EXPAND_SZ,        BytesOf(L"%SystemRoot%\\x", 15)});
    values.push_back({L"ExpandEmpty",            REG_EXPAND_SZ,        {}});
    values.push_back({L"Binary",                 REG_BINARY,           {1, 2, 3, 4, 5}});
    values.push_back({L"BinaryEmpty",            REG_BINARY,           {}});
    values.push_back({L"Dword",                  REG_DWORD,            {0x78, 0x56, 0x34, 0x12}});
    values.push_back({L"DwordShort",             REG_DWORD,            {0x78, 0x56}});
    values.push_back({L"DwordBigEndian",         REG_DWORD_BIG_ENDIAN, {0x12, 0x34, 0x56, 0x78}});
    values.push_back({L"Link",                   REG_LINK,             BytesOf(L"link", 4)});
    values.push_back({L"Multi",                  REG_MULTI_SZ,         BytesOf(L"a\0bc\0", 6)});
    values.push_back({L"MultiNoFinalTerminator", REG_MULTI_SZ,         BytesOf(L"a\0bc", 5)});
    values.push_back({L"MultiNoTerminator",      REG_MULTI_SZ,         BytesOf(L"abc", 3)});
    values.push_back({L"MultiEmpty",             REG_MULTI_SZ,         {}});
    values.push_back({L"MultiNonAscii",          REG_MULTI_SZ,         BytesOf(MULTI_NON_ASCII, std::size(MULTI_NON_ASCII))});
    values.push_back({L"Qword",                  REG_QWORD,            {1, 2, 3, 4, 5, 6, 7, 8}});
    values.push_back({L"ResourceList",           REG_RESOURCE_LIST,    {9, 8, 7}});
    values.push_back({L"Unknown",                0x1234,               {4, 3}});
    // @formatter:on
    return values;
}

struct SetWideAndQueryEveryType
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        for (const auto& value : WideValues())
        {
            const auto label = Narrow(value.name);
            RECORD(label + " set", SetStored(api, hRoot, value));
            QueryMatrix(api, record, hRoot, value.name.c_str(), label + " W");
            QueryMatrix(api, record, hRoot, label.c_str(), label + " A");
        }
    }
};

struct AnsiItem
{
    const char* name;
    DWORD type;
    std::string buffer;
    DWORD cbData;
};

std::vector<AnsiItem> AnsiItems()
{
    using namespace std::string_literals;
    return {
        {"Sz", REG_SZ, "text\0"s, 5},
        {"SzNoTerminatorGuardNul", REG_SZ, "text\0\0"s, 4},
        {"SzNonAscii", REG_SZ, "a\xe4\x80\0"s, 4},
        {"SzNonAsciiNoTerminator", REG_SZ, "a\xe4\x80\0"s, 3},
        {"SzEmpty", REG_SZ, "X"s, 0},
        {"SzJustTerminator", REG_SZ, "\0"s, 1},
        {"SzEmbeddedNul", REG_SZ, "ab\0cd\0"s, 6},
        {"Expand", REG_EXPAND_SZ, "%SystemRoot%\\x\0"s, 14},
        {"Multi", REG_MULTI_SZ, "a\0bc\0\0"s, 6},
        {"MultiNoFinalTerminatorGuardNul", REG_MULTI_SZ, "a\0bc\0\0"s, 5},
        {"MultiNonAscii", REG_MULTI_SZ, "\xe4\0\x80\0\0"s, 5},
        {"Binary", REG_BINARY, "\x01\x02\xe4\x80"s, 4},
        {"Dword", REG_DWORD, "\x78\x56\x34\x12"s, 4},
        {"None", REG_NONE, "\x01\x02"s, 2},
        {"Link", REG_LINK, "li\0"s, 3},
    };
}

struct SetAnsiAndQueryEveryType
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        for (const auto& item : AnsiItems())
        {
            const std::string label = item.name;
            RECORD(
                label + " set",
                VRegSetValueExA(
                    api,
                    hRoot,
                    item.name,
                    0,
                    item.type,
                    Raw(item.buffer.data()),
                    item.cbData
                )
            );
            QueryMatrix(api, record, hRoot, item.name, label + " A");
            const auto wideName = std::wstring(label.begin(), label.end());
            QueryMatrix(api, record, hRoot, wideName.c_str(), label + " W");
        }
    }
};

struct SetInvalidArguments
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        const BYTE data[16] = {1, 2, 3, 4, 5, 6, 7, 8};

        RECORD(
            "W no data nonzero size",
            VRegSetValueExW(
                api,
                hRoot,
                L"A",
                0,
                REG_BINARY,
                nullptr,
                4
            )
        );
        RECORD(
            "W no data zero size",
            VRegSetValueExW(
                api,
                hRoot,
                L"B",
                0,
                REG_BINARY,
                nullptr,
                0
            )
        );
        RECORD(
            "W string no data zero size",
            VRegSetValueExW(
                api,
                hRoot,
                L"C",
                0,
                REG_SZ,
                nullptr,
                0
            )
        );
        RECORD(
            "W reserved",
            VRegSetValueExW(
                api,
                hRoot,
                L"D",
                7,
                REG_BINARY,
                data,
                8
            )
        );
        RECORD(
            "W invalid handle",
            VRegSetValueExW(
                api,
                nullptr,
                L"E",
                0,
                REG_BINARY,
                data,
                8
            )
        );
        RECORD(
            "A no data nonzero size",
            VRegSetValueExA(
                api,
                hRoot,
                "A2",
                0,
                REG_BINARY,
                nullptr,
                4
            )
        );
        RECORD(
            "A no data zero size",
            VRegSetValueExA(
                api,
                hRoot,
                "B2",
                0,
                REG_BINARY,
                nullptr,
                0
            )
        );
        RECORD(
            "A string no data zero size",
            VRegSetValueExA(
                api,
                hRoot,
                "C2",
                0,
                REG_SZ,
                nullptr,
                0
            )
        );
        RECORD(
            "A string no data nonzero size",
            VRegSetValueExA(
                api,
                hRoot,
                "D2",
                0,
                REG_SZ,
                nullptr,
                4
            )
        );
        RECORD(
            "A reserved",
            VRegSetValueExA(
                api,
                hRoot,
                "E2",
                7,
                REG_BINARY,
                data,
                8
            )
        );
        RECORD(
            "A reserved string",
            VRegSetValueExA(
                api,
                hRoot,
                "F2",
                7,
                REG_SZ,
                data,
                8
            )
        );
        RECORD(
            "A invalid handle",
            VRegSetValueExA(
                api,
                nullptr,
                "G2",
                0,
                REG_BINARY,
                data,
                8
            )
        );
        RECORD(
            "A invalid handle string",
            VRegSetValueExA(
                api,
                nullptr,
                "H2",
                0,
                REG_SZ,
                data,
                8
            )
        );
        RECORD(
            "A long name",
            VRegSetValueExA(
                api,
                hRoot,
                std::string(300, 'n').c_str(),
                0,
                REG_DWORD,
                data,
                4
            )
        );
        RECORD(
            "W long name",
            VRegSetValueExW(
                api,
                hRoot,
                std::wstring(300, L'n').c_str(),
                0,
                REG_DWORD,
                data,
                4
            )
        );

        DWORD cbData = FILL_SIZE;
        RECORD(
            "none stored",
            VRegQueryValueExW(
                api,
                hRoot,
                L"A",
                nullptr,
                nullptr,
                nullptr,
                &cbData
            ),
            {cbData}
        );
        cbData = FILL_SIZE;
        RECORD(
            "zero stored",
            VRegQueryValueExW(
                api,
                hRoot,
                L"B",
                nullptr,
                nullptr,
                nullptr,
                &cbData
            ),
            {cbData}
        );
        cbData = FILL_SIZE;
        RECORD(
            "string stored",
            VRegQueryValueExW(
                api,
                hRoot,
                L"C",
                nullptr,
                nullptr,
                nullptr,
                &cbData
            ),
            {cbData}
        );
        cbData = FILL_SIZE;
        RECORD(
            "string stored A",
            VRegQueryValueExW(
                api,
                hRoot,
                L"C2",
                nullptr,
                nullptr,
                nullptr,
                &cbData
            ),
            {cbData}
        );
    }
};

struct QueryMissingAndInvalid
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        QueryMatrix(api, record, hRoot, L"Missing", "missing W");
        QueryMatrix(api, record, hRoot, "Missing", "missing A");

        DWORD dwType = FILL_SIZE;
        DWORD cbData = FILL_SIZE;
        std::array<BYTE, 16> data;
        data.fill(FILL);
        RECORD(
            "W reserved",
            VRegQueryValueExW(
                api,
                hRoot,
                L"Missing",
                &cbData,
                &dwType,
                data.data(),
                &cbData
            ),
            {dwType},
            {Bytes(data)}
        );
        cbData = 16;
        const DWORD reserved = 5;
        RECORD(
            "W reserved set",
            VRegQueryValueExW(
                api,
                hRoot,
                L"Missing",
                const_cast<LPDWORD>(&reserved),
                &dwType,
                data.data(),
                &cbData
            ),
            {
                dwType,
                cbData,
            }
        );
        RECORD(
            "A reserved set",
            VRegQueryValueExA(
                api,
                hRoot,
                "Missing",
                const_cast<LPDWORD>(&reserved),
                &dwType,
                data.data(),
                &cbData
            ),
            {
                dwType,
                cbData,
            }
        );

        cbData = 16;
        dwType = FILL_SIZE;
        RECORD(
            "W invalid handle",
            VRegQueryValueExW(
                api,
                nullptr,
                L"Missing",
                nullptr,
                &dwType,
                data.data(),
                &cbData
            ),
            {
                dwType,
                cbData,
            }
        );
        RECORD(
            "A invalid handle",
            VRegQueryValueExA(
                api,
                nullptr,
                "Missing",
                nullptr,
                &dwType,
                data.data(),
                &cbData
            ),
            {
                dwType,
                cbData,
            }
        );
        RECORD(
            "W invalid handle, reserved",
            VRegQueryValueExW(
                api,
                nullptr,
                L"Missing",
                const_cast<LPDWORD>(&reserved),
                nullptr,
                nullptr,
                nullptr
            )
        );
        RECORD(
            "A invalid handle, reserved",
            VRegQueryValueExA(
                api,
                nullptr,
                "Missing",
                const_cast<LPDWORD>(&reserved),
                nullptr,
                nullptr,
                nullptr
            )
        );
        RECORD(
            "A invalid handle, data without size",
            VRegQueryValueExA(
                api,
                nullptr,
                "Missing",
                nullptr,
                nullptr,
                data.data(),
                nullptr
            )
        );
        RECORD(
            "W missing, data without size",
            VRegQueryValueExW(
                api,
                hRoot,
                L"Missing",
                nullptr,
                nullptr,
                data.data(),
                nullptr
            )
        );
        RECORD(
            "A missing, data without size",
            VRegQueryValueExA(
                api,
                hRoot,
                "Missing",
                nullptr,
                nullptr,
                data.data(),
                nullptr
            )
        );
    }
};

struct DefaultValue
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        const BYTE VALUE_TEXT[] = "text";
        QueryMatrix(
            api,
            record,
            hRoot,
            static_cast<LPCWSTR>(nullptr),
            "unset null W"
        );
        QueryMatrix(api, record, hRoot, L"", "unset empty W");
        QueryMatrix(
            api,
            record,
            hRoot,
            static_cast<LPCSTR>(nullptr),
            "unset null A"
        );
        QueryMatrix(api, record, hRoot, "", "unset empty A");

        RECORD(
            "set null W",
            VRegSetValueExW(
                api,
                hRoot,
                nullptr,
                0,
                REG_SZ,
                BytesOf(L"one", 4).data(),
                8
            )
        );
        QueryMatrix(
            api,
            record,
            hRoot,
            static_cast<LPCWSTR>(nullptr),
            "null W"
        );
        QueryMatrix(api, record, hRoot, L"", "empty W");
        QueryMatrix(api, record, hRoot, static_cast<LPCSTR>(nullptr), "null A");
        QueryMatrix(api, record, hRoot, "", "empty A");

        RECORD(
            "set empty A",
            VRegSetValueExA(
                api,
                hRoot,
                "",
                0,
                REG_SZ,
                VALUE_TEXT,
                5
            )
        );
        QueryMatrix(
            api,
            record,
            hRoot,
            static_cast<LPCWSTR>(nullptr),
            "after A null W"
        );

        RECORD(
            "set null A",
            VRegSetValueExA(
                api,
                hRoot,
                nullptr,
                0,
                REG_DWORD,
                VALUE_TEXT,
                4
            )
        );
        QueryMatrix(api, record, hRoot, L"", "after null A empty W");

        RECORD(
            "delete empty A",
            VRegDeleteValueA(
                api,
                hRoot,
                ""
            )
        );
        RECORD(
            "delete empty A again",
            VRegDeleteValueA(
                api,
                hRoot,
                ""
            )
        );
        RECORD(
            "set again",
            VRegSetValueExW(
                api,
                hRoot,
                L"",
                0,
                REG_SZ,
                BytesOf(L"two", 4).data(),
                8
            )
        );
        RECORD(
            "delete null W",
            VRegDeleteValueW(
                api,
                hRoot,
                nullptr
            )
        );
        RECORD(
            "delete null W again",
            VRegDeleteValueW(
                api,
                hRoot,
                nullptr
            )
        );
        RECORD(
            "set again",
            VRegSetValueExW(
                api,
                hRoot,
                L"",
                0,
                REG_SZ,
                BytesOf(L"two", 4).data(),
                8
            )
        );
        RECORD(
            "delete null A",
            VRegDeleteValueA(
                api,
                hRoot,
                nullptr
            )
        );
    }
};

struct NonAsciiNames
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        const BYTE data[4] = {1, 2, 3, 4};
        RECORD(
            "set W",
            VRegSetValueExW(
                api,
                hRoot,
                L"n\u00e4me\u20ac",
                0,
                REG_BINARY,
                data,
                4
            )
        );
        QueryMatrix(api, record, hRoot, "n\xe4me\x80", "find by A");
        QueryMatrix(api, record, hRoot, L"N\u00c4ME\u20ac", "find by upper W");

        RECORD(
            "set A",
            VRegSetValueExA(
                api,
                hRoot,
                "caf\xe9",
                0,
                REG_BINARY,
                data,
                4
            )
        );
        QueryMatrix(api, record, hRoot, L"caf\u00e9", "A set, W query");

        RECORD(
            "set W unrepresentable",
            VRegSetValueExW(
                api,
                hRoot,
                L"x\u4e2d",
                0,
                REG_BINARY,
                data,
                4
            )
        );
        QueryMatrix(api, record, hRoot, "x?", "unrepresentable as ?");
        QueryMatrix(api, record, hRoot, L"x\u4e2d", "unrepresentable as W");

        RECORD(
            "set W best fit",
            VRegSetValueExW(
                api,
                hRoot,
                L"b\u0101",
                0,
                REG_BINARY,
                data,
                4
            )
        );
        QueryMatrix(api, record, hRoot, "ba", "best fit as a");

        RECORD(
            "delete A",
            VRegDeleteValueA(
                api,
                hRoot,
                "n\xe4me\x80"
            )
        );
        RECORD(
            "delete A again",
            VRegDeleteValueA(
                api,
                hRoot,
                "n\xe4me\x80"
            )
        );
        RECORD(
            "delete W",
            VRegDeleteValueW(
                api,
                hRoot,
                L"caf\u00e9"
            )
        );
        RECORD(
            "delete A unrepresentable",
            VRegDeleteValueA(
                api,
                hRoot,
                "x?"
            )
        );
        RECORD(
            "delete W unrepresentable",
            VRegDeleteValueW(
                api,
                hRoot,
                L"x\u4e2d"
            )
        );

        RECORD(
            "set A empty-ish",
            VRegSetValueExA(
                api,
                hRoot,
                "\x80",
                0,
                REG_SZ,
                Raw("\xe4"),
                2
            )
        );
        QueryMatrix(api, record, hRoot, L"\u20ac", "euro name");
    }
};

struct DeleteValues
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        const BYTE data[4] = {1, 2, 3, 4};
        RECORD(
            "delete missing W",
            VRegDeleteValueW(
                api,
                hRoot,
                L"Missing"
            )
        );
        RECORD(
            "delete missing A",
            VRegDeleteValueA(
                api,
                hRoot,
                "Missing"
            )
        );
        RECORD(
            "set",
            VRegSetValueExW(
                api,
                hRoot,
                L"V",
                0,
                REG_BINARY,
                data,
                4
            )
        );
        RECORD(
            "delete W",
            VRegDeleteValueW(
                api,
                hRoot,
                L"v"
            )
        );
        RECORD(
            "delete W again",
            VRegDeleteValueW(
                api,
                hRoot,
                L"v"
            )
        );
        RECORD(
            "set",
            VRegSetValueExW(
                api,
                hRoot,
                L"V",
                0,
                REG_BINARY,
                data,
                4
            )
        );
        RECORD(
            "delete A",
            VRegDeleteValueA(
                api,
                hRoot,
                "V"
            )
        );
        RECORD(
            "delete A again",
            VRegDeleteValueA(
                api,
                hRoot,
                "V"
            )
        );
        RECORD(
            "delete W invalid handle",
            VRegDeleteValueW(
                api,
                nullptr,
                L"V"
            )
        );
        RECORD(
            "delete A invalid handle",
            VRegDeleteValueA(
                api,
                nullptr,
                "V"
            )
        );
    }
};

struct PreExValues
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        const wchar_t WIDE[] = L"he\u00e4llo";
        const char NARROW[] = "he\xe4llo";

        RECORD(
            "set W new sub",
            VRegSetValueW(
                api,
                hRoot,
                L"Sub",
                REG_SZ,
                WIDE,
                0
            )
        );
        RECORD(
            "set W new nested",
            VRegSetValueW(
                api,
                hRoot,
                L"Deep\\Sub",
                REG_SZ,
                WIDE,
                100
            )
        );
        RECORD(
            "set W cb small",
            VRegSetValueW(
                api,
                hRoot,
                L"Sub2",
                REG_SZ,
                WIDE,
                3
            )
        );
        RECORD(
            "set A new sub",
            VRegSetValueA(
                api,
                hRoot,
                "SubA",
                REG_SZ,
                NARROW,
                0
            )
        );
        RECORD(
            "set A cb small",
            VRegSetValueA(
                api,
                hRoot,
                "SubA2",
                REG_SZ,
                NARROW,
                2
            )
        );
        RECORD(
            "set W wrong type",
            VRegSetValueW(
                api,
                hRoot,
                L"Wrong",
                REG_DWORD,
                WIDE,
                0
            )
        );
        RECORD(
            "set A wrong type",
            VRegSetValueA(
                api,
                hRoot,
                "Wrong",
                REG_BINARY,
                NARROW,
                0
            )
        );
        RECORD(
            "set W null data",
            VRegSetValueW(
                api,
                hRoot,
                L"NullData",
                REG_SZ,
                nullptr,
                0
            )
        );
        RECORD(
            "set A null data",
            VRegSetValueA(
                api,
                hRoot,
                "NullDataA",
                REG_SZ,
                nullptr,
                0
            )
        );
        RECORD(
            "set W null sub",
            VRegSetValueW(
                api,
                hRoot,
                nullptr,
                REG_SZ,
                L"root default",
                0
            )
        );
        RECORD(
            "set A empty sub",
            VRegSetValueA(
                api,
                hRoot,
                "",
                REG_SZ,
                "root default A",
                0
            )
        );
        RECORD(
            "set W invalid handle",
            VRegSetValueW(
                api,
                nullptr,
                L"S",
                REG_SZ,
                WIDE,
                0
            )
        );
        RECORD(
            "set A invalid handle",
            VRegSetValueA(
                api,
                nullptr,
                "S",
                REG_SZ,
                NARROW,
                0
            )
        );
        RECORD(
            "set W unrepresentable",
            VRegSetValueW(
                api,
                hRoot,
                L"U\u4e2d",
                REG_SZ,
                WIDE,
                0
            )
        );
        RECORD(
            "set W unicode data",
            VRegSetValueW(
                api,
                hRoot,
                L"UD",
                REG_SZ,
                L"\u4e2d\u0101",
                0
            )
        );
        RECORD(
            "set A empty string",
            VRegSetValueA(
                api,
                hRoot,
                "Empty",
                REG_SZ,
                "",
                0
            )
        );

        for (
            const auto* const pszSub :
            {
                L"Sub",
                L"Deep\\Sub",
                L"Sub2",
                L"NullData",
                L"Wrong",
                L"Missing",
                L"",
                L"U\u4e2d",
                L"UD",
                L"Empty"
            }
        )
        {
            const auto label = Narrow(pszSub);
            for (const LONG cbBuffer : {-1l, 0l, 1l, 2l, 5l, 12l, 64l})
            {
                std::array<wchar_t, 32> data;
                std::memset(data.data(), FILL, sizeof(data));
                LONG cbData = cbBuffer;
                RECORD(
                    "query W " + label + std::format(" buffer {}", cbBuffer),
                    VRegQueryValueW(
                        api,
                        hRoot,
                        pszSub,
                        data.data(),
                        &cbData
                    ),
                    {static_cast<std::uint64_t>(cbData)},
                    {Bytes(data)}
                );
            }
            LONG cbData = static_cast<LONG>(FILL_SIZE);
            RECORD(
                "query W size " + label,
                VRegQueryValueW(
                    api,
                    hRoot,
                    pszSub,
                    nullptr,
                    &cbData
                ),
                {static_cast<std::uint64_t>(cbData)}
            );
            RECORD(
                "query W no size " + label,
                VRegQueryValueW(
                    api,
                    hRoot,
                    pszSub,
                    nullptr,
                    nullptr
                )
            );

            const auto narrow = Narrow(pszSub);
            for (const LONG cbBuffer : {-1l, 0l, 1l, 2l, 5l, 12l, 64l})
            {
                std::array<char, 32> data;
                std::memset(data.data(), FILL, sizeof(data));
                LONG cbData2 = cbBuffer;
                RECORD(
                    "query A " + label + std::format(" buffer {}", cbBuffer),
                    VRegQueryValueA(
                        api,
                        hRoot,
                        narrow.c_str(),
                        data.data(),
                        &cbData2
                    ),
                    {static_cast<std::uint64_t>(cbData2)},
                    {Bytes(data)}
                );
            }
            cbData = static_cast<LONG>(FILL_SIZE);
            RECORD(
                "query A size " + label,
                VRegQueryValueA(
                    api,
                    hRoot,
                    narrow.c_str(),
                    nullptr,
                    &cbData
                ),
                {static_cast<std::uint64_t>(cbData)}
            );
            RECORD(
                "query A no size " + label,
                VRegQueryValueA(
                    api,
                    hRoot,
                    narrow.c_str(),
                    nullptr,
                    nullptr
                )
            );
        }

        {
            std::array<char, 8> data;
            data.fill(FILL);
            LONG cbData = 8;
            RECORD(
                "query A null sub",
                VRegQueryValueA(
                    api,
                    hRoot,
                    nullptr,
                    data.data(),
                    &cbData
                ),
                {static_cast<std::uint64_t>(cbData)},
                {Bytes(data)}
            );
            std::array<wchar_t, 8> wdata;
            std::memset(wdata.data(), FILL, sizeof(wdata));
            cbData = 16;
            RECORD(
                "query W null sub",
                VRegQueryValueW(
                    api,
                    hRoot,
                    nullptr,
                    wdata.data(),
                    &cbData
                ),
                {static_cast<std::uint64_t>(cbData)},
                {Bytes(wdata)}
            );
            RECORD(
                "query W data without size",
                VRegQueryValueW(
                    api,
                    hRoot,
                    L"Sub",
                    wdata.data(),
                    nullptr
                )
            );
            RECORD(
                "query A data without size",
                VRegQueryValueA(
                    api,
                    hRoot,
                    "Sub",
                    data.data(),
                    nullptr
                )
            );
            cbData = 16;
            RECORD(
                "query W invalid handle",
                VRegQueryValueW(
                    api,
                    nullptr,
                    L"Sub",
                    wdata.data(),
                    &cbData
                ),
                {static_cast<std::uint64_t>(cbData)}
            );
            RECORD(
                "query A invalid handle",
                VRegQueryValueA(
                    api,
                    nullptr,
                    "Sub",
                    data.data(),
                    &cbData
                ),
                {static_cast<std::uint64_t>(cbData)}
            );
        }

        // A default value of another type is not a string.
        HKEY hSub{};
        RECORD(
            "create typed",
            VRegCreateKeyExW(
                api,
                hRoot,
                L"Typed",
                0,
                nullptr,
                0,
                KEY_ALL_ACCESS,
                nullptr,
                &hSub,
                nullptr
            )
        );
        const DWORD dwValue = 7;
        RECORD(
            "set typed",
            VRegSetValueExW(
                api,
                hSub,
                nullptr,
                0,
                REG_DWORD,
                Raw(&dwValue),
                4
            )
        );
        RECORD(
            "close typed",
            VRegCloseKey(
                api,
                hSub
            )
        );
        for (const LONG cbBuffer : {0l, 4l, 64l})
        {
            std::array<wchar_t, 32> data;
            std::memset(data.data(), FILL, sizeof(data));
            LONG cbData = cbBuffer;
            RECORD(
                "query typed W",
                VRegQueryValueW(
                    api,
                    hRoot,
                    L"Typed",
                    data.data(),
                    &cbData
                ),
                {static_cast<std::uint64_t>(cbData)},
                {Bytes(data)}
            );
            std::array<char, 32> adata;
            std::memset(adata.data(), FILL, sizeof(adata));
            cbData = cbBuffer;
            RECORD(
                "query typed A",
                VRegQueryValueA(
                    api,
                    hRoot,
                    "Typed",
                    adata.data(),
                    &cbData
                ),
                {static_cast<std::uint64_t>(cbData)},
                {Bytes(adata)}
            );
        }
        LONG cbData = static_cast<LONG>(FILL_SIZE);
        RECORD(
            "query typed W size",
            VRegQueryValueW(
                api,
                hRoot,
                L"Typed",
                nullptr,
                &cbData
            ),
            {static_cast<std::uint64_t>(cbData)}
        );
        cbData = static_cast<LONG>(FILL_SIZE);
        RECORD(
            "query typed A size",
            VRegQueryValueA(
                api,
                hRoot,
                "Typed",
                nullptr,
                &cbData
            ),
            {static_cast<std::uint64_t>(cbData)}
        );
    }
};

struct SetKeyValueForms
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        const BYTE dword[4] = {1, 2, 3, 4};

        RECORD(
            "W creates subkey",
            VRegSetKeyValueW(
                api,
                hRoot,
                L"New\\Sub",
                L"v",
                REG_SZ,
                L"x\u00e4",
                6
            )
        );
        QueryMatrix(api, record, hRoot, L"New\\Sub", "W subkey as value");
        RECORD(
            "A creates subkey",
            VRegSetKeyValueA(
                api,
                hRoot,
                "NewA\\Sub",
                "v",
                REG_SZ,
                "x\xe4",
                3
            )
        );
        RECORD(
            "W null sub",
            VRegSetKeyValueW(
                api,
                hRoot,
                nullptr,
                L"rootv",
                REG_DWORD,
                dword,
                4
            )
        );
        RECORD(
            "W empty sub",
            VRegSetKeyValueW(
                api,
                hRoot,
                L"",
                L"rootv2",
                REG_DWORD,
                dword,
                4
            )
        );
        RECORD(
            "A null sub",
            VRegSetKeyValueA(
                api,
                hRoot,
                nullptr,
                "rootva",
                REG_DWORD,
                dword,
                4
            )
        );
        RECORD(
            "A empty sub",
            VRegSetKeyValueA(
                api,
                hRoot,
                "",
                "rootva2",
                REG_DWORD,
                dword,
                4
            )
        );
        RECORD(
            "W null name",
            VRegSetKeyValueW(
                api,
                hRoot,
                L"New",
                nullptr,
                REG_SZ,
                L"def",
                8
            )
        );
        RECORD(
            "A null name",
            VRegSetKeyValueA(
                api,
                hRoot,
                "NewA",
                nullptr,
                REG_SZ,
                "def",
                4
            )
        );
        RECORD(
            "W no terminator",
            VRegSetKeyValueW(
                api,
                hRoot,
                L"NoTerm",
                L"v",
                REG_SZ,
                L"abcd",
                6
            )
        );
        RECORD(
            "A no terminator",
            VRegSetKeyValueA(
                api,
                hRoot,
                "NoTermA",
                "v",
                REG_SZ,
                "abc",
                3
            )
        );
        RECORD(
            "W no data",
            VRegSetKeyValueW(
                api,
                hRoot,
                L"NoData",
                L"v",
                REG_SZ,
                nullptr,
                0
            )
        );
        RECORD(
            "A no data",
            VRegSetKeyValueA(
                api,
                hRoot,
                "NoDataA",
                "v",
                REG_SZ,
                nullptr,
                0
            )
        );
        RECORD(
            "W bad data",
            VRegSetKeyValueW(
                api,
                hRoot,
                L"BadData",
                L"v",
                REG_SZ,
                nullptr,
                4
            )
        );
        RECORD(
            "A bad data",
            VRegSetKeyValueA(
                api,
                hRoot,
                "BadDataA",
                "v",
                REG_SZ,
                nullptr,
                4
            )
        );
        RECORD(
            "W invalid handle",
            VRegSetKeyValueW(
                api,
                nullptr,
                L"X",
                L"v",
                REG_DWORD,
                dword,
                4
            )
        );
        RECORD(
            "A invalid handle",
            VRegSetKeyValueA(
                api,
                nullptr,
                "X",
                "v",
                REG_DWORD,
                dword,
                4
            )
        );
        RECORD(
            "W invalid handle no sub",
            VRegSetKeyValueW(
                api,
                nullptr,
                nullptr,
                L"v",
                REG_DWORD,
                dword,
                4
            )
        );
        RECORD(
            "A invalid handle no sub",
            VRegSetKeyValueA(
                api,
                nullptr,
                nullptr,
                "v",
                REG_DWORD,
                dword,
                4
            )
        );
        RECORD(
            "W unrepresentable",
            VRegSetKeyValueW(
                api,
                hRoot,
                L"U\u4e2d",
                L"v\u4e2d",
                REG_SZ,
                L"\u4e2d",
                4
            )
        );
        RECORD(
            "A multi",
            VRegSetKeyValueA(
                api,
                hRoot,
                "MultiA",
                "m",
                REG_MULTI_SZ,
                "a\0b\xe4\0",
                6
            )
        );
        RECORD(
            "W reg link",
            VRegSetKeyValueW(
                api,
                hRoot,
                L"LinkW",
                L"m",
                REG_LINK,
                L"ab",
                4
            )
        );

        for (const auto* const pszSub :
             {
                 L"New\\Sub",
                 L"NewA\\Sub",
                 L"New",
                 L"NewA",
                 L"NoTerm",
                 L"NoTermA",
                 L"NoData",
                 L"NoDataA",
                 L"BadData",
                 L"BadDataA",
                 L"MultiA",
                 L"LinkW"
             })
        {
            const auto label = Narrow(pszSub);
            HKEY hKey{};
            const auto status = VRegOpenKeyExW(
                api,
                hRoot,
                pszSub,
                0,
                KEY_READ,
                &hKey
            );
            if (status != ERROR_SUCCESS)
            {
                RECORD("open " + label, ERROR_FILE_NOT_FOUND);
                continue;
            }
            for (const auto* const pszName : {L"v", L"v\u4e2d", L""})
                QueryMatrix(
                    api,
                    record,
                    hKey,
                    pszName,
                    "check " + label + " " + Narrow(pszName)
                );
            RECORD(
                "close " + label,
                VRegCloseKey(
                    api,
                    hKey
                )
            );
        }
        QueryMatrix(api, record, hRoot, L"rootv", "rootv");
        QueryMatrix(api, record, hRoot, L"rootv2", "rootv2");
        QueryMatrix(api, record, hRoot, L"rootva", "rootva");
        QueryMatrix(api, record, hRoot, L"rootva2", "rootva2");
    }
};

struct DeleteKeyValueForms
{
    void operator()(
        const Api& api,
        const HKEY hRoot,
        Recorder& record
    ) const
    {
        const BYTE data[4] = {1, 2, 3, 4};
        RECORD(
            "set sub",
            VRegSetKeyValueW(
                api,
                hRoot,
                L"Sub",
                L"v",
                REG_BINARY,
                data,
                4
            )
        );
        RECORD(
            "set sub 2",
            VRegSetKeyValueW(
                api,
                hRoot,
                L"Sub",
                L"w",
                REG_BINARY,
                data,
                4
            )
        );
        RECORD(
            "set sub 3",
            VRegSetKeyValueW(
                api,
                hRoot,
                L"Sub",
                nullptr,
                REG_BINARY,
                data,
                4
            )
        );
        RECORD(
            "set root",
            VRegSetKeyValueW(
                api,
                hRoot,
                nullptr,
                L"v",
                REG_BINARY,
                data,
                4
            )
        );
        RECORD(
            "set root 2",
            VRegSetKeyValueW(
                api,
                hRoot,
                nullptr,
                L"w",
                REG_BINARY,
                data,
                4
            )
        );

        RECORD(
            "W missing sub",
            VRegDeleteKeyValueW(
                api,
                hRoot,
                L"Nope",
                L"v"
            )
        );
        RECORD(
            "A missing sub",
            VRegDeleteKeyValueA(
                api,
                hRoot,
                "Nope",
                "v"
            )
        );
        RECORD(
            "W missing value",
            VRegDeleteKeyValueW(
                api,
                hRoot,
                L"Sub",
                L"nope"
            )
        );
        RECORD(
            "A missing value",
            VRegDeleteKeyValueA(
                api,
                hRoot,
                "Sub",
                "nope"
            )
        );
        RECORD(
            "W delete",
            VRegDeleteKeyValueW(
                api,
                hRoot,
                L"Sub",
                L"v"
            )
        );
        RECORD(
            "W delete again",
            VRegDeleteKeyValueW(
                api,
                hRoot,
                L"Sub",
                L"v"
            )
        );
        RECORD(
            "A delete",
            VRegDeleteKeyValueA(
                api,
                hRoot,
                "Sub",
                "w"
            )
        );
        RECORD(
            "W delete default",
            VRegDeleteKeyValueW(
                api,
                hRoot,
                L"Sub",
                nullptr
            )
        );
        RECORD(
            "W delete root null sub",
            VRegDeleteKeyValueW(
                api,
                hRoot,
                nullptr,
                L"v"
            )
        );
        RECORD(
            "A delete root empty sub",
            VRegDeleteKeyValueA(
                api,
                hRoot,
                "",
                "w"
            )
        );
        RECORD(
            "W delete root again",
            VRegDeleteKeyValueW(
                api,
                hRoot,
                nullptr,
                L"v"
            )
        );
        RECORD(
            "W invalid handle",
            VRegDeleteKeyValueW(
                api,
                nullptr,
                L"Sub",
                L"v"
            )
        );
        RECORD(
            "A invalid handle",
            VRegDeleteKeyValueA(
                api,
                nullptr,
                "Sub",
                "v"
            )
        );
        RECORD(
            "W invalid handle no sub",
            VRegDeleteKeyValueW(
                api,
                nullptr,
                nullptr,
                L"v"
            )
        );
        RECORD(
            "A invalid handle no sub",
            VRegDeleteKeyValueA(
                api,
                nullptr,
                nullptr,
                "v"
            )
        );

        DWORD cbData = FILL_SIZE;
        RECORD(
            "sub values gone",
            VRegQueryValueExW(
                api,
                hRoot,
                L"v",
                nullptr,
                nullptr,
                nullptr,
                &cbData
            ),
            {cbData}
        );
    }
};

using Scenarios = std::tuple<
    SetWideAndQueryEveryType,
    SetAnsiAndQueryEveryType,
    SetInvalidArguments,
    QueryMissingAndInvalid,
    DefaultValue,
    NonAsciiNames,
    DeleteValues,
    PreExValues,
    SetKeyValueForms,
    DeleteKeyValueForms>;
} // namespace

TEMPLATE_LIST_TEST_CASE(
    "Value scenarios answer as the Win32 API does",
    "[oracle][values]",
    Scenarios
)
{
    Win32Registry win32Registry;
    const Api win32 = NativeReference(win32Registry);
    OwnedRegistry native = MakeNativeRegistry();
    CheckSameRecording(Record<TestType>(win32), Record<TestType>(Api{native, false}));
}

TEMPLATE_LIST_TEST_CASE(
    "Value scenarios answer as the Win32 API does on virtual keys",
    "[oracle][virtual][values]",
    Scenarios
)
{
    Win32Registry win32Registry;
    const Api win32 = ReducedReference(win32Registry);
    CheckSameRecording(Record<TestType>(win32), RecordVirtual<TestType>());
}

TEMPLATE_LIST_TEST_CASE(
    "Value scenarios answer as the Win32 API does on an overlay",
    "[oracle][overlay][values]",
    Scenarios
)
{
    Win32Registry win32Registry;
    const Api win32 = ReducedReference(win32Registry);
    CheckSameRecording(Record<TestType>(win32), RecordOverlay<TestType>());
}
