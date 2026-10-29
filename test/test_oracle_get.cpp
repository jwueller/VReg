// RegGetValue, the MUI string loaders and RegQueryMultipleValues. See
// scenario.h.

#include "scenario.h"

#include <VReg/hive.h>

#include <Shlwapi.h>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

using namespace VReg;
using namespace VReg::Test;

namespace
{
/// What the scenario's `Indirect` value resolves to natively.
std::wstring ResolvedIndirect()
{
    std::array<wchar_t, 256> text{};
    REQUIRE(
        SHLoadIndirectString(
            L"@%SystemRoot%\\System32\\shell32.dll,-21770",
            text.data(),
            static_cast<UINT>(text.size()),
            nullptr
        ) == S_OK
    );
    return text.data();
}

template <class Api>
LSTATUS GetEx(
    Api& api,
    HKEY hKey,
    LPCWSTR pszSub,
    LPCWSTR pszValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pData,
    LPDWORD pcbData
)
{
    return api.RegGetValueW(
        hKey,
        pszSub,
        pszValue,
        dwFlags,
        pdwType,
        pData,
        pcbData
    );
}

template <class Api>
LSTATUS
GetEx(
    Api& api,
    HKEY hKey,
    LPCSTR pszSub,
    LPCSTR pszValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pData,
    LPDWORD pcbData
)
{
    return api.RegGetValueA(
        hKey,
        pszSub,
        pszValue,
        dwFlags,
        pdwType,
        pData,
        pcbData
    );
}

std::vector<StoredValue> GetValues()
{
    const wchar_t NON_ASCII[] = L"a\u00e4\u20ac\u0101\u4e2d";
    std::vector<StoredValue> values;
    values.push_back({L"Sz", REG_SZ, BytesOf(L"text", 5)});
    values.push_back({L"SzNoTerminator", REG_SZ, BytesOf(L"text", 4)});
    values.push_back({L"SzEmpty", REG_SZ, {}});
    values.push_back({L"SzNonAscii", REG_SZ, BytesOf(NON_ASCII, std::size(NON_ASCII))});
    values.push_back({L"ExpandShort", REG_EXPAND_SZ, BytesOf(L"%SystemRoot%", 13)});
    values.push_back({L"ExpandLong", REG_EXPAND_SZ, BytesOf(L"%OS%\\x", 7)});
    values.push_back({L"ExpandUnset", REG_EXPAND_SZ, BytesOf(L"%VREG_UNSET_VARIABLE%", 22)});
    values.push_back({L"ExpandNoTerminator", REG_EXPAND_SZ, BytesOf(L"%OS%", 4)});
    values.push_back({L"ExpandEmpty", REG_EXPAND_SZ, {}});
    values.push_back({L"Dword", REG_DWORD, {0x78, 0x56, 0x34, 0x12}});
    values.push_back({L"DwordShort", REG_DWORD, {0x78, 0x56}});
    values.push_back({L"Qword", REG_QWORD, {1, 2, 3, 4, 5, 6, 7, 8}});
    values.push_back({L"Binary", REG_BINARY, {1, 2, 3, 4, 5}});
    values.push_back({L"BinaryEmpty", REG_BINARY, {}});
    values.push_back({L"Multi", REG_MULTI_SZ, BytesOf(L"a\0bc\0", 6)});
    values.push_back({L"MultiNoTerminator", REG_MULTI_SZ, BytesOf(L"a\0bc", 5)});
    values.push_back({L"None", REG_NONE, {1, 2}});
    values.push_back({L"Unknown", 0x1234, {4, 3}});
    return values;
}

constexpr DWORD FLAG_SETS[] = {
    0,
    RRF_RT_ANY,
    RRF_RT_REG_SZ,
    RRF_RT_REG_EXPAND_SZ,
    RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
    RRF_RT_REG_BINARY,
    RRF_RT_REG_DWORD,
    RRF_RT_REG_MULTI_SZ,
    RRF_RT_REG_NONE,
    RRF_RT_REG_QWORD,
    RRF_RT_DWORD,
    RRF_RT_QWORD,
    RRF_RT_ANY | RRF_NOEXPAND,
    RRF_RT_REG_SZ | RRF_NOEXPAND,
    RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
    RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
    RRF_RT_ANY | RRF_ZEROONFAILURE,
    RRF_RT_REG_DWORD | RRF_ZEROONFAILURE,
    RRF_RT_REG_SZ | RRF_ZEROONFAILURE | RRF_NOEXPAND,
    RRF_RT_ANY | RRF_SUBKEY_WOW6464KEY,
    RRF_RT_ANY | RRF_SUBKEY_WOW6432KEY,
    RRF_RT_ANY | RRF_SUBKEY_WOW6464KEY | RRF_SUBKEY_WOW6432KEY,
    RRF_RT_ANY | 0x10,
    RRF_RT_ANY | 0x80000000,
    0x00010000 | RRF_RT_REG_SZ,
};

template <class Api, class Char>
void GetMatrix(
    Api& api,
    Recorder& record,
    const HKEY hKey,
    const Char* const pszSub,
    const Char* const pszValue,
    const std::string& label
)
{
    for (const DWORD dwFlags : FLAG_SETS)
    {
        const auto flagLabel = label + std::format(" flags {:#x}", dwFlags);

        DWORD dwType = FILL_SIZE;
        RECORD(
            flagLabel + " type only",
            GetEx(
                api,
                hKey,
                pszSub,
                pszValue,
                dwFlags,
                &dwType,
                nullptr,
                nullptr
            ),
            {dwType}
        );

        dwType = FILL_SIZE;
        DWORD cbData = FILL_SIZE;
        RECORD(
            flagLabel + " size",
            GetEx(
                api,
                hKey,
                pszSub,
                pszValue,
                dwFlags,
                &dwType,
                nullptr,
                &cbData
            ),
            {dwType, cbData}
        );

        for (const DWORD cbBuffer : {0u, 3u, 8u, 16u, 128u})
        {
            std::array<BYTE, 128> data;
            data.fill(FILL);
            dwType = FILL_SIZE;
            cbData = cbBuffer;
            RECORD(
                flagLabel + std::format(" buffer {}", cbBuffer),
                GetEx(
                    api,
                    hKey,
                    pszSub,
                    pszValue,
                    dwFlags,
                    &dwType,
                    data.data(),
                    &cbData
                ),
                {dwType, cbData},
                {Bytes(data)}
            );
        }

        std::array<BYTE, 128> data;
        data.fill(FILL);
        cbData = 128;
        RECORD(
            flagLabel + " no type",
            GetEx(
                api,
                hKey,
                pszSub,
                pszValue,
                dwFlags,
                nullptr,
                data.data(),
                &cbData
            ),
            {cbData},
            {Bytes(data)}
        );
        data.fill(FILL);
        dwType = FILL_SIZE;
        RECORD(
            flagLabel + " data without size",
            GetEx(
                api,
                hKey,
                pszSub,
                pszValue,
                dwFlags,
                &dwType,
                data.data(),
                nullptr
            ),
            {dwType},
            {Bytes(data)}
        );
    }
}

struct GetValueEveryType
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        for (const auto& value : GetValues())
        {
            const auto label = Narrow(value.name);
            RECORD(label + " set", SetStored(api, hRoot, value));
            GetMatrix(
                api,
                record,
                hRoot,
                static_cast<LPCWSTR>(nullptr),
                value.name.c_str(),
                label + " W"
            );
            GetMatrix(
                api,
                record,
                hRoot,
                static_cast<LPCSTR>(nullptr),
                label.c_str(),
                label + " A"
            );
        }
        GetMatrix(
            api,
            record,
            hRoot,
            static_cast<LPCWSTR>(nullptr),
            L"Missing",
            "missing W"
        );
        GetMatrix(
            api,
            record,
            hRoot,
            static_cast<LPCSTR>(nullptr),
            "Missing",
            "missing A"
        );
    }
};

struct GetValueSubKeyAndName
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        RECORD(
            "set sub",
            api.RegSetKeyValueW(
                hRoot,
                L"Sub\\Deep",
                L"v",
                REG_SZ,
                L"deep",
                10
            )
        );
        RECORD(
            "set sub default",
            api.RegSetKeyValueW(
                hRoot,
                L"Sub",
                nullptr,
                REG_SZ,
                L"default",
                16
            )
        );
        RECORD(
            "set root default",
            api.RegSetKeyValueW(
                hRoot,
                nullptr,
                nullptr,
                REG_SZ,
                L"root",
                10
            )
        );
        RECORD(
            "set non-ASCII",
            api.RegSetKeyValueW(
                hRoot,
                L"S\u00e4b",
                L"v\u20ac",
                REG_SZ,
                L"x",
                4
            )
        );
        RECORD(
            "set 32",
            api.RegSetKeyValueW(
                hRoot,
                L"Sub",
                L"v32",
                REG_SZ,
                L"x",
                4
            )
        );

        for (const wchar_t* const pszSub :
             {static_cast<const wchar_t*>(nullptr),
              L"",
              L"Sub",
              L"sub\\deep",
              L"Sub\\",
              L"\\Sub",
              L"Missing",
              L"Sub\\Missing",
              L"S\u00e4b"})
        {
            const auto narrowSub = pszSub ? Narrow(pszSub) : std::string();
            const char* const pszNarrow = pszSub ? narrowSub.c_str() : nullptr;
            for (const wchar_t* const pszValue :
                 {static_cast<const wchar_t*>(nullptr), L"", L"v", L"V32", L"Missing", L"v\u20ac"})
            {
                const auto narrowValue = pszValue ? Narrow(pszValue) : std::string();
                const char* const pszNarrowValue = pszValue ? narrowValue.c_str() : nullptr;
                const auto label = std::string("sub ") + (pszSub ? narrowSub : "(null)") + " value "
                                   + (pszValue ? narrowValue : "(null)");

                for (const DWORD dwFlags :
                     {RRF_RT_ANY, RRF_RT_REG_SZ, RRF_RT_DWORD | RRF_ZEROONFAILURE, RRF_RT_ANY | RRF_SUBKEY_WOW6432KEY})
                {
                    std::array<wchar_t, 16> data;
                    std::memset(data.data(), FILL, sizeof(data));
                    DWORD dwType = FILL_SIZE;
                    DWORD cbData = sizeof(data);
                    RECORD(
                        label + std::format(" W {:#x}", dwFlags),
                        api.RegGetValueW(
                            hRoot,
                            pszSub,
                            pszValue,
                            dwFlags,
                            &dwType,
                            data.data(),
                            &cbData
                        ),
                        {dwType, cbData},
                        {Bytes(data)}
                    );

                    std::array<char, 16> adata;
                    std::memset(adata.data(), FILL, sizeof(adata));
                    dwType = FILL_SIZE;
                    cbData = sizeof(adata);
                    RECORD(
                        label + std::format(" A {:#x}", dwFlags),
                        api.RegGetValueA(
                            hRoot,
                            pszNarrow,
                            pszNarrowValue,
                            dwFlags,
                            &dwType,
                            adata.data(),
                            &cbData
                        ),
                        {dwType, cbData},
                        {Bytes(adata)}
                    );
                }
            }
        }

        DWORD cbData = 8;
        std::array<BYTE, 8> data;
        RECORD(
            "W invalid handle",
            api.RegGetValueW(
                nullptr,
                nullptr,
                L"v",
                RRF_RT_ANY,
                nullptr,
                data.data(),
                &cbData
            ),
            {cbData}
        );
        RECORD(
            "A invalid handle",
            api.RegGetValueA(
                nullptr,
                nullptr,
                "v",
                RRF_RT_ANY,
                nullptr,
                data.data(),
                &cbData
            ),
            {cbData}
        );
        RECORD(
            "W invalid handle with sub",
            api.RegGetValueW(
                nullptr,
                L"Sub",
                L"v",
                RRF_RT_ANY,
                nullptr,
                data.data(),
                &cbData
            ),
            {cbData}
        );
        RECORD(
            "A invalid handle with sub",
            api.RegGetValueA(
                nullptr,
                "Sub",
                "v",
                RRF_RT_ANY,
                nullptr,
                data.data(),
                &cbData
            ),
            {cbData}
        );
    }
};

struct LoadMUIStringForms
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        for (const auto& value : std::vector<StoredValue>{
                 {L"Plain", REG_SZ, BytesOf(L"text", 5)},
                 {L"PlainNoTerminator", REG_SZ, BytesOf(L"text", 4)},
                 {L"PlainEmpty", REG_SZ, {}},
                 {L"NonAscii", REG_SZ, BytesOf(L"a\u00e4\u20ac\u4e2d", 5)},
                 {L"Expand", REG_EXPAND_SZ, BytesOf(L"%SystemRoot%", 13)},
                 {L"Dword", REG_DWORD, {1, 2, 3, 4}},
                 {L"Binary", REG_BINARY, {1, 2, 3}},
                 {L"Multi", REG_MULTI_SZ, BytesOf(L"a\0b\0", 5)},
                 {L"Indirect", REG_SZ, BytesOf(L"@%SystemRoot%\\System32\\shell32.dll,-21770", 44)},
                 {L"IndirectMissing", REG_SZ, BytesOf(L"@%SystemRoot%\\System32\\NoSuchVRegLibrary.dll,-1", 51)},
                 {L"IndirectBadId", REG_SZ, BytesOf(L"@%SystemRoot%\\System32\\shell32.dll,-99999999", 46)},
                 {L"IndirectMalformed", REG_SZ, BytesOf(L"@shell32.dll", 13)},
             })
        {
            RECORD("set " + Narrow(value.name), SetStored(api, hRoot, value));
        }

        // A hive never resolves an indirect string, but answers a value marked
        // as MUI text with its text: here, what the indirect string resolves to.
        if constexpr (std::is_same_v<Api, VirtualRegistry>)
        {
            auto* const pNode = Hive::NodeOf(hRoot);
            REQUIRE(
                Hive::HiveOf(pNode)->SetMuiTextValue(
                    pNode,
                    L"Indirect",
                    ResolvedIndirect()
                ) == ERROR_SUCCESS
            );
        }

        for (const wchar_t* const pszName :
             {L"Plain",
              L"PlainNoTerminator",
              L"PlainEmpty",
              L"NonAscii",
              L"Expand",
              L"Dword",
              L"Binary",
              L"Multi",
              L"Indirect",
              L"IndirectMissing",
              L"IndirectBadId",
              L"IndirectMalformed",
              L"Missing",
              L""})
        {
            // Resolving an indirect string loads a resource module, which a virtual key never does.
            if (VIRTUAL_KEYS<Api> && std::wstring_view(pszName).starts_with(L"Indirect")
                && std::wstring_view(pszName) != L"Indirect")
                continue;

            const auto label = Narrow(pszName);
            for (const DWORD dwFlags : {DWORD{0}, DWORD{REG_MUI_STRING_TRUNCATE}})
            {
                for (const DWORD cbBuffer : {0u, 2u, 3u, 6u, 10u, 128u})
                {
                    std::array<wchar_t, 64> data;
                    std::memset(data.data(), FILL, sizeof(data));
                    DWORD cbData = FILL_SIZE;
                    RECORD(
                        "muiW " + label + std::format(" flags {} buffer {}", dwFlags, cbBuffer),
                        api.RegLoadMUIStringW(
                            hRoot,
                            pszName,
                            data.data(),
                            cbBuffer,
                            &cbData,
                            dwFlags,
                            nullptr
                        ),
                        {cbData},
                        {Bytes(data)}
                    );
                }

                DWORD cbData = FILL_SIZE;
                RECORD(
                    "muiW size " + label + std::format(" flags {}", dwFlags),
                    api.RegLoadMUIStringW(
                        hRoot,
                        pszName,
                        nullptr,
                        0,
                        &cbData,
                        dwFlags,
                        nullptr
                    ),
                    {cbData}
                );
                cbData = FILL_SIZE;
                RECORD(
                    "muiW null buffer " + label + std::format(" flags {}", dwFlags),
                    api.RegLoadMUIStringW(
                        hRoot,
                        pszName,
                        nullptr,
                        128,
                        dwFlags ? nullptr : &cbData,
                        dwFlags,
                        nullptr
                    ),
                    {cbData}
                );
                std::array<wchar_t, 64> data;
                std::memset(data.data(), FILL, sizeof(data));
                RECORD(
                    "muiW no size " + label + std::format(" flags {}", dwFlags),
                    api.RegLoadMUIStringW(
                        hRoot,
                        pszName,
                        data.data(),
                        128,
                        nullptr,
                        dwFlags,
                        nullptr
                    ),
                    {},
                    {Bytes(data)}
                );
            }

            std::array<wchar_t, 64> data;
            std::memset(data.data(), FILL, sizeof(data));
            DWORD cbData = FILL_SIZE;
            // Natively the directory decides where the module is found; a
            // value marked as MUI text has no module.
            if (!VIRTUAL_KEYS<Api> || label != "Indirect")
            {
                RECORD(
                    "muiW directory " + label,
                    api.RegLoadMUIStringW(
                        hRoot,
                        pszName,
                        data.data(),
                        128,
                        &cbData,
                        0,
                        L"C:\\Windows\\System32"
                    ),
                    {cbData},
                    {Bytes(data)}
                );
            }

            const auto narrow = Narrow(pszName);
            std::array<char, 64> adata;
            std::memset(adata.data(), FILL, sizeof(adata));
            cbData = FILL_SIZE;
            RECORD(
                "muiA " + label,
                api.RegLoadMUIStringA(
                    hRoot,
                    narrow.c_str(),
                    adata.data(),
                    64,
                    &cbData,
                    0,
                    nullptr
                ),
                {cbData},
                {Bytes(adata)}
            );
            cbData = FILL_SIZE;
            RECORD(
                "muiA size " + label,
                api.RegLoadMUIStringA(
                    hRoot,
                    narrow.c_str(),
                    nullptr,
                    0,
                    &cbData,
                    0,
                    nullptr
                ),
                {cbData}
            );
            RECORD(
                "muiA no size " + label,
                api.RegLoadMUIStringA(
                    hRoot,
                    narrow.c_str(),
                    adata.data(),
                    64,
                    nullptr,
                    0,
                    "C:\\Windows"
                )
            );
        }

        std::array<char, 8> adata;
        RECORD(
            "muiA null value",
            api.RegLoadMUIStringA(
                hRoot,
                nullptr,
                adata.data(),
                8,
                nullptr,
                0,
                nullptr
            )
        );
        RECORD(
            "muiA invalid handle",
            api.RegLoadMUIStringA(
                nullptr,
                "Plain",
                adata.data(),
                8,
                nullptr,
                0,
                nullptr
            )
        );
        RECORD(
            "muiA bad flags",
            api.RegLoadMUIStringA(
                hRoot,
                "Plain",
                adata.data(),
                8,
                nullptr,
                0xFFFF,
                nullptr
            )
        );

        std::array<wchar_t, 8> wdata;
        DWORD cbData = FILL_SIZE;
        RECORD(
            "muiW null value",
            api.RegLoadMUIStringW(
                hRoot,
                nullptr,
                wdata.data(),
                16,
                &cbData,
                0,
                nullptr
            ),
            {cbData}
        );
        RECORD(
            "muiW invalid handle",
            api.RegLoadMUIStringW(
                nullptr,
                L"Plain",
                wdata.data(),
                16,
                &cbData,
                0,
                nullptr
            ),
            {cbData}
        );
        RECORD(
            "muiW bad flags",
            api.RegLoadMUIStringW(
                hRoot,
                L"Plain",
                wdata.data(),
                16,
                &cbData,
                0xFFFF,
                nullptr
            ),
            {cbData}
        );
    }
};

template <class Api>
LSTATUS QueryMultiple(
    Api& api,
    HKEY hKey,
    PVALENTW pEntries,
    DWORD cEntries,
    LPWSTR pBuffer,
    LPDWORD pcbTotal
)
{
    return api.RegQueryMultipleValuesW(
        hKey,
        pEntries,
        cEntries,
        pBuffer,
        pcbTotal
    );
}

template <class Api>
LSTATUS QueryMultiple(
    Api& api,
    HKEY hKey,
    PVALENTA pEntries,
    DWORD cEntries,
    LPSTR pBuffer,
    LPDWORD pcbTotal
)
{
    return api.RegQueryMultipleValuesA(
        hKey,
        pEntries,
        cEntries,
        pBuffer,
        pcbTotal
    );
}

template <class Api, class Char, class Valent>
void MultipleMatrix(
    Api& api,
    Recorder& record,
    const HKEY hKey,
    std::vector<std::basic_string<Char>> names,
    const std::string& label
)
{
    for (const int cbTotal : {-1, 0, 1, 4, 8, 12, 16, 20, 24, 32, 40, 48, 128, 256})
    {
        // The native ANSI function faults on a null buffer, and answers `ERROR_INVALID_DATA` after filling some
        // of the entries when the buffer holds the first values but not all of them.
        if constexpr (std::is_same_v<Char, char>)
        {
            if (cbTotal < 0 || (cbTotal > 4 && cbTotal < 24))
                continue;
        }

        std::vector<Valent> entries(names.size());
        for (std::size_t i = 0; i < names.size(); ++i)
        {
            entries[i].ve_valuename = const_cast<Char*>(names[i].c_str());
            entries[i].ve_valuelen = FILL_SIZE;
            entries[i].ve_valueptr = static_cast<DWORD_PTR>(FILL_SIZE);
            entries[i].ve_type = FILL_SIZE;
        }

        std::array<BYTE, 256> buffer;
        buffer.fill(FILL);
        DWORD cbInOut = cbTotal < 0 ? FILL_SIZE : static_cast<DWORD>(cbTotal);
        const auto status = QueryMultiple(
            api,
            hKey,
            entries.data(),
            static_cast<DWORD>(entries.size()),
            cbTotal < 0 ? nullptr : reinterpret_cast<Char*>(buffer.data()),
            &cbInOut
        );

        std::vector<std::uint64_t> sizes{cbInOut};
        for (const auto& entry : entries)
        {
            sizes.push_back(entry.ve_valuelen);
            sizes.push_back(entry.ve_type);
            sizes.push_back(
                entry.ve_valueptr == static_cast<DWORD_PTR>(FILL_SIZE)
                    ? ~std::uint64_t{}
                    : Offset(
                        reinterpret_cast<const void*>(entry.ve_valueptr),
                        buffer.data()
                    )
            );
        }

        const auto& flat = sizes;
        switch (names.size())
        {
        case 1:
            RECORD(
                label + " detail",
                status,
                {flat[0], flat[1], flat[2], flat[3]},
                {Bytes(buffer)}
            );
            break;
        case 2:
            RECORD(
                label + " detail",
                status,
                {flat[0], flat[1], flat[2], flat[3], flat[4], flat[5], flat[6]},
                {Bytes(buffer)}
            );
            break;
        default:
            RECORD(
                label + " detail",
                status,
                {flat[0], flat[1], flat[2], flat[3], flat[4], flat[5], flat[6], flat[7], flat[8], flat[9]},
                {Bytes(buffer)}
            );
            break;
        }
    }
}

struct QueryMultipleValuesForms
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        const BYTE binary[3] = {1, 2, 3};
        const DWORD dword = 0x12345678;
        RECORD(
            "set a",
            api.RegSetValueExW(
                hRoot,
                L"a",
                0,
                REG_SZ,
                BytesOf(L"alpha", 6).data(),
                12
            )
        );
        RECORD(
            "set b",
            api.RegSetValueExW(
                hRoot,
                L"b",
                0,
                REG_DWORD,
                Raw(&dword),
                4
            )
        );
        RECORD(
            "set c",
            api.RegSetValueExW(
                hRoot,
                L"c",
                0,
                REG_BINARY,
                binary,
                3
            )
        );
        RECORD(
            "set d",
            api.RegSetValueExW(
                hRoot,
                L"d",
                0,
                REG_SZ,
                BytesOf(L"\u00e4\u4e2d", 3).data(),
                6
            )
        );
        RECORD(
            "set e",
            api.RegSetValueExW(
                hRoot,
                L"e",
                0,
                REG_MULTI_SZ,
                BytesOf(L"x\0y\0", 5).data(),
                10
            )
        );
        RECORD(
            "set default",
            api.RegSetValueExW(
                hRoot,
                nullptr,
                0,
                REG_SZ,
                BytesOf(L"def", 4).data(),
                8
            )
        );

        using W = std::vector<std::wstring>;
        using A = std::vector<std::string>;
        MultipleMatrix<Api, wchar_t, VALENTW>(api, record, hRoot, W{L"a"}, "W one");
        MultipleMatrix<Api, wchar_t, VALENTW>(api, record, hRoot, W{L"a", L"b"}, "W two");
        MultipleMatrix<Api, wchar_t, VALENTW>(api, record, hRoot, W{L"a", L"c", L"b"}, "W three");
        MultipleMatrix<Api, wchar_t, VALENTW>(api, record, hRoot, W{L"c", L"d", L"e"}, "W odd sizes");
        MultipleMatrix<Api, wchar_t, VALENTW>(api, record, hRoot, W{L"a", L"Missing", L"b"}, "W missing");
        MultipleMatrix<Api, wchar_t, VALENTW>(api, record, hRoot, W{L"Missing"}, "W only missing");
        MultipleMatrix<Api, wchar_t, VALENTW>(api, record, hRoot, W{L""}, "W default");
        MultipleMatrix<Api, wchar_t, VALENTW>(api, record, hRoot, W{L"b", L"b"}, "W repeated");
        MultipleMatrix<Api, char, VALENTA>(api, record, hRoot, A{"a"}, "A one");
        MultipleMatrix<Api, char, VALENTA>(api, record, hRoot, A{"a", "b"}, "A two");
        MultipleMatrix<Api, char, VALENTA>(api, record, hRoot, A{"a", "c", "b"}, "A three");
        MultipleMatrix<Api, char, VALENTA>(api, record, hRoot, A{"c", "d", "e"}, "A odd sizes");
        MultipleMatrix<Api, char, VALENTA>(api, record, hRoot, A{"a", "Missing", "b"}, "A missing");
        MultipleMatrix<Api, char, VALENTA>(api, record, hRoot, A{"Missing"}, "A only missing");
        MultipleMatrix<Api, char, VALENTA>(api, record, hRoot, A{""}, "A default");

        VALENTW wide{const_cast<LPWSTR>(L"a"), 0, 0, 0};
        VALENTA narrow{const_cast<LPSTR>("a"), 0, 0, 0};
        DWORD cbTotal = 64;
        std::array<BYTE, 64> buffer;
        buffer.fill(FILL);
        RECORD(
            "W no entries",
            api.RegQueryMultipleValuesW(
                hRoot,
                &wide,
                0,
                reinterpret_cast<LPWSTR>(buffer.data()),
                &cbTotal
            ),
            {cbTotal},
            {Bytes(buffer)}
        );
        cbTotal = 64;
        RECORD(
            "A no entries",
            api.RegQueryMultipleValuesA(
                hRoot,
                &narrow,
                0,
                reinterpret_cast<LPSTR>(buffer.data()),
                &cbTotal
            ),
            {cbTotal},
            {Bytes(buffer)}
        );
        cbTotal = 64;
        RECORD(
            "W invalid handle",
            api.RegQueryMultipleValuesW(
                nullptr,
                &wide,
                1,
                reinterpret_cast<LPWSTR>(buffer.data()),
                &cbTotal
            ),
            {cbTotal}
        );
        cbTotal = 64;
        RECORD(
            "A invalid handle",
            api.RegQueryMultipleValuesA(
                nullptr,
                &narrow,
                1,
                reinterpret_cast<LPSTR>(buffer.data()),
                &cbTotal
            ),
            {cbTotal}
        );
    }
};

using Scenarios = std::tuple<GetValueEveryType, GetValueSubKeyAndName, LoadMUIStringForms, QueryMultipleValuesForms>;
} // namespace

TEMPLATE_LIST_TEST_CASE(
    "Lookup scenarios answer as the Win32 API does",
    "[oracle][get]",
    Scenarios
)
{
    Win32Api win32;
    NativeRegistry native;
    CheckSameRecording(Record<TestType>(win32), Record<TestType>(native));
}

TEMPLATE_LIST_TEST_CASE(
    "Lookup scenarios answer as the Win32 API does on virtual keys",
    "[oracle][virtual][get]",
    Scenarios
)
{
    Reduced<Win32Api> win32;
    // The reference first, so it skips under Wine before the virtual run
    // asks the system to resolve an indirect string (see ResolvedIndirect).
    const auto expected = Record<TestType>(win32);
    CheckSameRecording(expected, RecordVirtual<TestType>());
}

TEMPLATE_LIST_TEST_CASE(
    "Lookup scenarios answer as the Win32 API does on an overlay",
    "[oracle][overlay][get]",
    Scenarios
)
{
    Reduced<Win32Api> win32;
    CheckSameRecording(Record<TestType>(win32), RecordOverlay<TestType>());
}
