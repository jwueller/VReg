// Enumeration and key information scenarios. See scenario.h.

#include "scenario.h"

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
template <class Api>
LSTATUS EnumKeyEx(
    Api& api,
    HKEY hKey,
    DWORD dwIndex,
    LPWSTR pszName,
    LPDWORD pcchName,
    LPDWORD pReserved,
    LPWSTR pszClass,
    LPDWORD pcchClass,
    PFILETIME pft
)
{
    return api.RegEnumKeyExW(
        hKey,
        dwIndex,
        pszName,
        pcchName,
        pReserved,
        pszClass,
        pcchClass,
        pft
    );
}

template <class Api>
LSTATUS EnumKeyEx(
    Api& api,
    HKEY hKey,
    DWORD dwIndex,
    LPSTR pszName,
    LPDWORD pcchName,
    LPDWORD pReserved,
    LPSTR pszClass,
    LPDWORD pcchClass,
    PFILETIME pft
)
{
    return api.RegEnumKeyExA(
        hKey,
        dwIndex,
        pszName,
        pcchName,
        pReserved,
        pszClass,
        pcchClass,
        pft
    );
}

template <class Api>
LSTATUS EnumValueEx(
    Api& api,
    HKEY hKey,
    DWORD dwIndex,
    LPWSTR pszName,
    LPDWORD pcchName,
    LPDWORD pReserved,
    LPDWORD pdwType,
    LPBYTE pData,
    LPDWORD pcbData
)
{
    return api.RegEnumValueW(
        hKey,
        dwIndex,
        pszName,
        pcchName,
        pReserved,
        pdwType,
        pData,
        pcbData
    );
}

template <class Api>
LSTATUS EnumValueEx(
    Api& api,
    HKEY hKey,
    DWORD dwIndex,
    LPSTR pszName,
    LPDWORD pcchName,
    LPDWORD pReserved,
    LPDWORD pdwType,
    LPBYTE pData,
    LPDWORD pcbData
)
{
    return api.RegEnumValueA(
        hKey,
        dwIndex,
        pszName,
        pcchName,
        pReserved,
        pdwType,
        pData,
        pcbData
    );
}

template <class Api>
LSTATUS QueryInfo(
    Api& api,
    HKEY hKey,
    LPWSTR pszClass,
    LPDWORD pcchClass,
    LPDWORD pReserved,
    LPDWORD pcSubKeys,
    LPDWORD pcbMaxSubKeyLen,
    LPDWORD pcbMaxClassLen,
    LPDWORD pcValues,
    LPDWORD pcbMaxValueNameLen,
    LPDWORD pcbMaxValueLen,
    LPDWORD pcbSecurityDescriptor,
    PFILETIME pft
)
{
    return api.RegQueryInfoKeyW(
        hKey,
        pszClass,
        pcchClass,
        pReserved,
        pcSubKeys,
        pcbMaxSubKeyLen,
        pcbMaxClassLen,
        pcValues,
        pcbMaxValueNameLen,
        pcbMaxValueLen,
        pcbSecurityDescriptor,
        pft
    );
}

template <class Api>
LSTATUS QueryInfo(
    Api& api,
    HKEY hKey,
    LPSTR pszClass,
    LPDWORD pcchClass,
    LPDWORD pReserved,
    LPDWORD pcSubKeys,
    LPDWORD pcbMaxSubKeyLen,
    LPDWORD pcbMaxClassLen,
    LPDWORD pcValues,
    LPDWORD pcbMaxValueNameLen,
    LPDWORD pcbMaxValueLen,
    LPDWORD pcbSecurityDescriptor,
    PFILETIME pft
)
{
    return api.RegQueryInfoKeyA(
        hKey,
        pszClass,
        pcchClass,
        pReserved,
        pcSubKeys,
        pcbMaxSubKeyLen,
        pcbMaxClassLen,
        pcValues,
        pcbMaxValueNameLen,
        pcbMaxValueLen,
        pcbSecurityDescriptor,
        pft
    );
}

template <class Api>
HKEY CreateWithClass(
    Api& api,
    Recorder& record,
    const HKEY hParent,
    const wchar_t* const pszName,
    const wchar_t* const pszClass
)
{
    HKEY hKey{};
    std::wstring cls = pszClass ? pszClass : L"";
    RECORD(
        "create " + Narrow(pszName),
        api.RegCreateKeyExW(
            hParent,
            pszName,
            0,
            pszClass ? cls.data() : nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hKey,
            nullptr
        )
    );
    return hKey;
}

/// Four subkeys with classes, wide and narrowable names, and one with a name
/// the ANSI code page cannot represent. Returns nothing: the keys are closed.
template <class Api> void PopulateKeys(
    Api& api,
    Recorder& record,
    const HKEY hRoot
)
{
    for (const auto& [pszName, pszClass] : std::initializer_list<std::pair<const wchar_t*, const wchar_t*>>{
             {L"Alpha", L"ClassA"},
             {L"B\u00e4ta", L"Cl\u00e4ss\u20ac"},
             {L"Gamma\u4e2d", nullptr},
             {L"D\u0101lta", L"\u4e2d\u0101"},
         })
    {
        const auto hKey = CreateWithClass(
            api,
            record,
            hRoot,
            pszName,
            pszClass
        );
        RECORD(
            "close",
            api.RegCloseKey(
                hKey
            )
        );
    }
}

template <class Api, class Char>
void EnumKeyMatrix(
    Api& api,
    Recorder& record,
    const HKEY hKey,
    const std::string& label
)
{
    for (const DWORD dwIndex : {0u, 1u, 2u, 3u, 4u, 5u})
    {
        for (const DWORD cchName : {0u, 1u, 2u, 4u, 5u, 6u, 7u, 8u, 64u})
        {
            for (const int cchClass : {-1, 0, 1, 3, 7, 8, 64})
            {
                std::array<Char, 64> name;
                std::array<Char, 64> cls;
                std::memset(name.data(), kFill, sizeof(name));
                std::memset(cls.data(), kFill, sizeof(cls));
                DWORD cchNameInOut = cchName;
                DWORD cchClassInOut = cchClass < 0 ? 0 : static_cast<DWORD>(cchClass);
                FILETIME ft = FillTime();
                const auto status = EnumKeyEx(
                    api,
                    hKey,
                    dwIndex,
                    name.data(),
                    &cchNameInOut,
                    nullptr,
                    cchClass < 0 ? nullptr : cls.data(),
                    cchClass < 0 ? nullptr : &cchClassInOut,
                    &ft
                );
                RECORD(
                    label + std::format(" index {} name {} class {}", dwIndex, cchName, cchClass),
                    status,
                    {cchNameInOut, cchClass < 0 ? 0 : cchClassInOut, Written(ft)},
                    {Bytes(name), Bytes(cls)}
                );
            }
        }
    }

    std::array<Char, 64> name;
    std::array<Char, 64> cls;
    std::memset(name.data(), kFill, sizeof(name));
    std::memset(cls.data(), kFill, sizeof(cls));
    DWORD cchName = 64;
    DWORD cchClass = 64;
    const DWORD reserved = 1;
    RECORD(
        label + " no name length",
        EnumKeyEx(
            api,
            hKey,
            0,
            name.data(),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ),
        {},
        {Bytes(name)}
    );
    // The native ANSI functions fault on a null name.
    if constexpr (std::is_same_v<Char, wchar_t>)
    {
        RECORD(
            label + " no name",
            EnumKeyEx(
                api,
                hKey,
                0,
                static_cast<Char*>(nullptr),
                &cchName,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            ),
            {cchName}
        );
    }
    cchName = 64;
    RECORD(
        label + " class without length",
        EnumKeyEx(
            api,
            hKey,
            0,
            name.data(),
            &cchName,
            nullptr,
            cls.data(),
            nullptr,
            nullptr
        ),
        {cchName},
        {Bytes(name), Bytes(cls)}
    );
    cchName = 64;
    RECORD(
        label + " class length without class",
        EnumKeyEx(
            api,
            hKey,
            0,
            name.data(),
            &cchName,
            nullptr,
            nullptr,
            &cchClass,
            nullptr
        ),
        {cchName, cchClass},
        {Bytes(name)}
    );
    cchName = 64;
    RECORD(
        label + " reserved",
        EnumKeyEx(
            api,
            hKey,
            0,
            name.data(),
            &cchName,
            const_cast<LPDWORD>(&reserved),
            nullptr,
            nullptr,
            nullptr
        ),
        {cchName},
        {Bytes(name)}
    );
    cchName = 64;
    RECORD(
        label + " invalid handle",
        EnumKeyEx(
            api,
            nullptr,
            0,
            name.data(),
            &cchName,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ),
        {cchName}
    );
}

struct EnumerateKeys
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        EnumKeyMatrix<Api, wchar_t>(api, record, hRoot, "empty W");
        EnumKeyMatrix<Api, char>(api, record, hRoot, "empty A");
        PopulateKeys(api, record, hRoot);
        EnumKeyMatrix<Api, wchar_t>(api, record, hRoot, "W");
        EnumKeyMatrix<Api, char>(api, record, hRoot, "A");
    }
};

struct EnumerateKeysPreEx
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        PopulateKeys(api, record, hRoot);
        for (const DWORD dwIndex : {0u, 1u, 2u, 3u, 4u})
        {
            for (const DWORD cchName : {0u, 1u, 2u, 4u, 5u, 6u, 7u, 8u, 64u})
            {
                std::array<wchar_t, 64> wname;
                std::memset(wname.data(), kFill, sizeof(wname));
                RECORD(
                    std::format("W index {} size {}", dwIndex, cchName),
                    api.RegEnumKeyW(
                        hRoot,
                        dwIndex,
                        wname.data(),
                        cchName
                    ),
                    {},
                    {Bytes(wname)}
                );
                std::array<char, 64> name;
                std::memset(name.data(), kFill, sizeof(name));
                RECORD(
                    std::format("A index {} size {}", dwIndex, cchName),
                    api.RegEnumKeyA(
                        hRoot,
                        dwIndex,
                        name.data(),
                        cchName
                    ),
                    {},
                    {Bytes(name)}
                );
            }
        }
        RECORD(
            "W no name",
            api.RegEnumKeyW(
                hRoot,
                0,
                nullptr,
                0
            )
        );
        RECORD(
            "A no name",
            api.RegEnumKeyA(
                hRoot,
                0,
                nullptr,
                0
            )
        );
        wchar_t wname[8];
        char name[8];
        RECORD(
            "W invalid handle",
            api.RegEnumKeyW(
                nullptr,
                0,
                wname,
                8
            )
        );
        RECORD(
            "A invalid handle",
            api.RegEnumKeyA(
                nullptr,
                0,
                name,
                8
            )
        );
    }
};

template <class Api> void PopulateValues(
    Api& api,
    Recorder& record,
    const HKEY hKey
)
{
    for (const auto& value : std::vector<StoredValue>{
             {L"v1", REG_SZ, BytesOf(L"text", 5)},
             {L"v\u00e4", REG_DWORD, {1, 2, 3, 4}},
             {L"v3\u4e2d", REG_BINARY, {}},
             {L"", REG_SZ, BytesOf(L"default\u00e4", 9)},
             {L"multi", REG_MULTI_SZ, BytesOf(L"a\u00e4\0\u4e2d\0", 6)},
             {L"expand", REG_EXPAND_SZ, BytesOf(L"%SystemRoot%", 13)},
             {L"sz\u20ac", REG_SZ, BytesOf(L"\u20ac\u0101", 3)},
             {L"noterm", REG_SZ, BytesOf(L"abc", 3)},
             {L"odd", REG_SZ, {0x61, 0, 0x62}},
             {L"qword", REG_QWORD, {1, 2, 3, 4, 5, 6, 7, 8}},
             {L"none", REG_NONE, {}},
         })
    {
        RECORD("set " + Narrow(value.name), SetStored(api, hKey, value));
    }
}

template <class Api, class Char>
void EnumValueMatrix(
    Api& api,
    Recorder& record,
    const HKEY hKey,
    const std::string& label
)
{
    constexpr int kNoData = -2;
    constexpr int kSizeOnly = -1;
    for (const DWORD dwIndex : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u})
    {
        for (const DWORD cchName : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 64u})
        {
            for (const int cbData : {kNoData, kSizeOnly, 0, 1, 2, 3, 5, 10, 64})
            {
                std::array<Char, 64> name;
                std::array<BYTE, 64> data;
                std::memset(name.data(), kFill, sizeof(name));
                data.fill(kFill);
                DWORD cchNameInOut = cchName;
                DWORD cbDataInOut = cbData < 0 ? 0 : static_cast<DWORD>(cbData);
                DWORD dwType = kFillSize;
                if (cbData == kSizeOnly)
                    cbDataInOut = kFillSize;
                const auto status = EnumValueEx(
                    api,
                    hKey,
                    dwIndex,
                    name.data(),
                    &cchNameInOut,
                    nullptr,
                    &dwType,
                    cbData == kNoData ? nullptr : (cbData == kSizeOnly ? nullptr : data.data()),
                    cbData == kNoData ? nullptr : &cbDataInOut
                );
                RECORD(
                    label + std::format(" index {} name {} data {}", dwIndex, cchName, cbData),
                    status,
                    {cchNameInOut, cbData == kNoData ? 0 : cbDataInOut, dwType},
                    {Bytes(name), Bytes(data)}
                );
            }
        }
    }

    std::array<Char, 64> name;
    std::array<BYTE, 64> data;
    std::memset(name.data(), kFill, sizeof(name));
    data.fill(kFill);
    DWORD cchName = 64;
    DWORD cbData = 64;
    DWORD dwType = kFillSize;
    const DWORD reserved = 1;
    RECORD(
        label + " no types",
        EnumValueEx(
            api,
            hKey,
            0,
            name.data(),
            &cchName,
            nullptr,
            nullptr,
            data.data(),
            &cbData
        ),
        {cchName, cbData},
        {Bytes(name), Bytes(data)}
    );
    cchName = 64;
    cbData = 64;
    RECORD(
        label + " nothing optional",
        EnumValueEx(
            api,
            hKey,
            1,
            name.data(),
            &cchName,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ),
        {cchName},
        {Bytes(name)}
    );
    RECORD(
        label + " no name length",
        EnumValueEx(
            api,
            hKey,
            1,
            name.data(),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        )
    );
    cchName = 64;
    if constexpr (std::is_same_v<Char, wchar_t>)
    {
        RECORD(
            label + " no name",
            EnumValueEx(
                api,
                hKey,
                1,
                static_cast<Char*>(nullptr),
                &cchName,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            ),
            {cchName}
        );
    }
    cchName = 64;
    RECORD(
        label + " data without size",
        EnumValueEx(
            api,
            hKey,
            1,
            name.data(),
            &cchName,
            nullptr,
            &dwType,
            data.data(),
            nullptr
        ),
        {cchName}
    );
    cchName = 64;
    RECORD(
        label + " reserved",
        EnumValueEx(
            api,
            hKey,
            1,
            name.data(),
            &cchName,
            const_cast<LPDWORD>(&reserved),
            nullptr,
            nullptr,
            nullptr
        ),
        {cchName}
    );
    cchName = 64;
    RECORD(
        label + " invalid handle",
        EnumValueEx(
            api,
            nullptr,
            1,
            name.data(),
            &cchName,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ),
        {cchName}
    );
}

struct EnumerateValues
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        EnumValueMatrix<Api, wchar_t>(api, record, hRoot, "empty W");
        EnumValueMatrix<Api, char>(api, record, hRoot, "empty A");
        PopulateValues(api, record, hRoot);
        EnumValueMatrix<Api, wchar_t>(api, record, hRoot, "W");
        EnumValueMatrix<Api, char>(api, record, hRoot, "A");
    }
};

template <class Api, class Char>
void QueryInfoMatrix(
    Api& api,
    Recorder& record,
    const HKEY hKey,
    const std::string& label
)
{
    constexpr int kOutputs = 8;
    for (int skip = -1; skip < kOutputs; ++skip)
    {
        for (const int cchClass : {-1, 0, 1, 3, 7, 8, 64})
        {
            std::array<Char, 64> cls;
            std::memset(cls.data(), kFill, sizeof(cls));
            std::array<DWORD, kOutputs> out;
            out.fill(kFillSize);
            FILETIME ft = FillTime();
            DWORD cchClassInOut = cchClass < 0 ? 0 : static_cast<DWORD>(cchClass);

            auto outPointer = [&](const int i)
            {
                return i == skip ? nullptr : &out[i];
            };
            const auto status = QueryInfo(
                api,
                hKey,
                cchClass < 0 ? nullptr : cls.data(),
                cchClass < 0 ? nullptr : &cchClassInOut,
                nullptr,
                outPointer(0),
                outPointer(1),
                outPointer(2),
                outPointer(3),
                outPointer(4),
                outPointer(5),
                // Virtual keys carry no security descriptor and report size 0.
                kVirtualKeys<Api> ? nullptr : outPointer(6),
                skip == 7 ? nullptr : &ft
            );
            RECORD(
                label + std::format(" skip {} class {}", skip, cchClass),
                status,
                {cchClass < 0 ? 0 : cchClassInOut, out[0], out[1], out[2], out[3], out[4], out[5], out[6], Written(ft)},
                {Bytes(cls)}
            );
        }
    }

    std::array<Char, 64> cls;
    std::memset(cls.data(), kFill, sizeof(cls));
    DWORD cchClass = 64;
    DWORD cSubKeys = kFillSize;
    const DWORD reserved = 1;
    RECORD(
        label + " class without length",
        QueryInfo(
            api,
            hKey,
            cls.data(),
            nullptr,
            nullptr,
            &cSubKeys,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ),
        {cSubKeys},
        {Bytes(cls)}
    );
    cSubKeys = kFillSize;
    RECORD(
        label + " class length without class",
        QueryInfo(
            api,
            hKey,
            static_cast<Char*>(nullptr),
            &cchClass,
            nullptr,
            &cSubKeys,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ),
        {cchClass, cSubKeys}
    );
    cSubKeys = kFillSize;
    RECORD(
        label + " reserved",
        QueryInfo(
            api,
            hKey,
            static_cast<Char*>(nullptr),
            nullptr,
            const_cast<LPDWORD>(&reserved),
            &cSubKeys,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ),
        {cSubKeys}
    );
    RECORD(
        label + " nothing",
        QueryInfo(
            api,
            hKey,
            static_cast<Char*>(nullptr),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        )
    );
    cSubKeys = kFillSize;
    cchClass = 64;
    RECORD(
        label + " invalid handle",
        QueryInfo(
            api,
            nullptr,
            cls.data(),
            &cchClass,
            nullptr,
            &cSubKeys,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        ),
        {cchClass, cSubKeys}
    );
}

struct QueryInfoKeyForms
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        QueryInfoMatrix<Api, wchar_t>(api, record, hRoot, "empty root W");
        QueryInfoMatrix<Api, char>(api, record, hRoot, "empty root A");
        PopulateKeys(api, record, hRoot);
        PopulateValues(api, record, hRoot);
        QueryInfoMatrix<Api, wchar_t>(api, record, hRoot, "root W");
        QueryInfoMatrix<Api, char>(api, record, hRoot, "root A");

        for (const auto* const pszName : {L"Alpha", L"B\u00e4ta", L"Gamma\u4e2d", L"D\u0101lta"})
        {
            HKEY hKey{};
            RECORD(
                "open",
                api.RegOpenKeyExW(
                    hRoot,
                    pszName,
                    0,
                    KEY_READ,
                    &hKey
                )
            );
            QueryInfoMatrix<Api, wchar_t>(api, record, hKey, "sub W");
            QueryInfoMatrix<Api, char>(api, record, hKey, "sub A");
            RECORD(
                "close",
                api.RegCloseKey(
                    hKey
                )
            );
        }

        // A virtual key has one handle per node, so it carries no per-open access rights.
        if constexpr (!kVirtualKeys<Api>)
        {
            HKEY hKey{};
            RECORD(
                "open for write only",
                api.RegOpenKeyExW(
                    hRoot,
                    L"Alpha",
                    0,
                    KEY_SET_VALUE,
                    &hKey
                )
            );
            QueryInfoMatrix<Api, wchar_t>(api, record, hKey, "write only W");
            QueryInfoMatrix<Api, char>(api, record, hKey, "write only A");
            RECORD(
                "close",
                api.RegCloseKey(
                    hKey
                )
            );
        }
    }
};

struct EnumerateWhileChanging
{
    template <class Api> void operator()(Api& api, const HKEY hRoot, Recorder& record) const
    {
        PopulateKeys(api, record, hRoot);
        PopulateValues(api, record, hRoot);

        std::array<wchar_t, 64> name;
        for (DWORD dwIndex = 0; dwIndex < 5; ++dwIndex)
        {
            name.fill(L'\0');
            DWORD cchName = 64;
            const auto status = api.RegEnumKeyExW(
                hRoot,
                dwIndex,
                name.data(),
                &cchName,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );
            RECORD("enum key", status, {cchName}, {Bytes(name)});
            if (status == ERROR_SUCCESS)
                RECORD(
                    "delete enumerated",
                    api.RegDeleteKeyExW(
                        hRoot,
                        name.data(),
                        0,
                        0
                    )
                );
        }

        for (DWORD dwIndex = 0; dwIndex < 5; ++dwIndex)
        {
            name.fill(L'\0');
            DWORD cchName = 64;
            const auto status = api.RegEnumValueW(
                hRoot,
                dwIndex,
                name.data(),
                &cchName,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );
            RECORD("enum value", status, {cchName}, {Bytes(name)});
            if (status == ERROR_SUCCESS)
                RECORD(
                    "delete enumerated",
                    api.RegDeleteValueW(
                        hRoot,
                        name.data()
                    )
                );
        }

        for (DWORD dwIndex = 0; dwIndex < 3; ++dwIndex)
        {
            name.fill(L'\0');
            DWORD cchName = 64;
            RECORD(
                "enum key after",
                api.RegEnumKeyExW(
                    hRoot,
                    dwIndex,
                    name.data(),
                    &cchName,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr
                ),
                {cchName},
                {Bytes(name)}
            );
        }
    }
};

using Scenarios
    = std::tuple<EnumerateKeys, EnumerateKeysPreEx, EnumerateValues, QueryInfoKeyForms, EnumerateWhileChanging>;
} // namespace

TEMPLATE_LIST_TEST_CASE(
    "Enumeration scenarios answer as the Win32 API does",
    "[oracle][enum]",
    Scenarios
)
{
    Win32Api win32;
    NativeRegistry native;
    CheckSameRecording(Record<TestType>(win32), Record<TestType>(native));
}

TEMPLATE_LIST_TEST_CASE(
    "Enumeration scenarios answer as the Win32 API does on virtual keys",
    "[oracle][virtual][enum]",
    Scenarios
)
{
    Reduced<Win32Api> win32;
    CheckSameRecording(Record<TestType>(win32), RecordVirtual<TestType>());
}

TEMPLATE_LIST_TEST_CASE(
    "Enumeration scenarios answer as the Win32 API does on an overlay",
    "[oracle][overlay][enum]",
    Scenarios
)
{
    Reduced<Win32Api> win32;
    CheckSameRecording(Record<TestType>(win32), RecordOverlay<TestType>());
}
