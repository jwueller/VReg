#include "oracle.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <format>
#include <utility>

#pragma comment(lib, "advapi32.lib")

namespace VRegTest
{
AppHive::AppHive()
{
    // Native Windows behavior is the spec; Wine may answer edge cases
    // differently, and a test following it would follow whichever Wine ran.
    if (GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version"))
        SKIP("needs the native registry, not Wine's");

    static std::atomic<unsigned> s_counter;

    m_directory = std::filesystem::temp_directory_path() / std::format(
        L"VReg-oracle-{}-{}",
        GetCurrentProcessId(),
        s_counter++
    );
    std::filesystem::remove_all(m_directory);
    std::filesystem::create_directories(m_directory);

    const auto file = m_directory / L"hive.dat";
    const auto error = RegLoadAppKeyW(
        file.c_str(),
        &m_hRoot,
        KEY_ALL_ACCESS,
        0,
        0
    );
    REQUIRE(error == ERROR_SUCCESS);
}

AppHive::~AppHive()
{
    RegCloseKey(m_hRoot);

    // The hive unloads with its last handle; a scenario that leaked one keeps
    // the file open, and the directory stays behind in temp.
    std::error_code ignored;
    std::filesystem::remove_all(m_directory, ignored);
}

HKEY AppHive::Root() const noexcept
{
    return m_hRoot;
}

VirtualHive::VirtualHive() :
    m_hive(MakeHive()),
    m_registry(MakeVirtualRegistry())
{
    REQUIRE(VReg_Mount(m_registry, HKEY_CURRENT_USER, L"VRegOracle", m_hive, 0) == ERROR_SUCCESS);
}

VReg* VirtualHive::Registry() noexcept
{
    return m_registry;
}

Api VirtualHive::AsApi() noexcept
{
    return {.pRegistry = m_registry, .isVirtualKeys = true};
}

HKEY VirtualHive::Root() noexcept
{
    return RootHandle(m_hive);
}

OverlayMount::OverlayMount() :
    m_native(MakeNativeRegistry())
{
    REQUIRE(
        VReg_CreateKeyExW(
            m_native,
            m_appHive.Root(),
            L"Lower",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &m_hLower,
            nullptr
        )
        == ERROR_SUCCESS
    );
    m_hive = MakeOverlayHive(m_native, m_appHive.Root(), L"Lower");
    m_registry = MakeVirtualRegistry(m_native);
    REQUIRE(VReg_Mount(m_registry, HKEY_CURRENT_USER, L"VRegOracle", m_hive, 0) == ERROR_SUCCESS);
}

OverlayMount::~OverlayMount()
{
    VReg_CloseKey(
        m_native,
        m_hLower
    );
}

VReg* OverlayMount::Registry() noexcept
{
    return m_registry;
}

Api OverlayMount::AsApi() noexcept
{
    return {.pRegistry = m_registry, .isVirtualKeys = true};
}

VReg* OverlayMount::Native() noexcept
{
    return m_native;
}

VRegHive* OverlayMount::Overlay() noexcept
{
    return m_hive;
}

HKEY OverlayMount::LowerRoot() const noexcept
{
    return m_hLower;
}

HKEY OverlayMount::Root() noexcept
{
    return RootHandle(m_hive);
}

void Recorder::operator()(
    std::string label,
    const LSTATUS status,
    const std::initializer_list<std::uint64_t> sizes,
    const std::initializer_list<std::span<const std::byte>> buffers
)
{
    Call call{
        .label = std::move(label),
        .status = status,
        .sizes = sizes,
        .bytes = {}
    };
    for (const auto buffer : buffers)
        call.bytes.insert(call.bytes.end(), buffer.begin(), buffer.end());

    m_calls.push_back(std::move(call));
}

Recording Recorder::Take() noexcept
{
    return std::exchange(m_calls, {});
}

void CheckSameRecording(const Recording& expected, const Recording& actual)
{
    constexpr std::size_t MAX_REPORTED = 12;
    std::size_t cReported = 0;
    for (std::size_t i = 0; i < expected.size() && i < actual.size(); ++i)
    {
        if (expected[i] != actual[i])
        {
            INFO("call " << i);
            CHECK(expected[i] == actual[i]);
            if (++cReported == MAX_REPORTED)
                return;
        }
    }

    CHECK(expected.size() == actual.size());
}
} // namespace VRegTest

std::string Catch::StringMaker<VRegTest::Call>::convert(
    const VRegTest::Call& call
)
{
    auto text = std::format("{}: status {}", call.label, call.status);

    if (call.sizes.size() > 0)
    {
        text += ", sizes";
        for (const auto size : call.sizes)
            text += std::format(" {:#x}", size);
    }

    if (call.bytes.size() > 0)
    {
        text += ", bytes";
        for (const auto byte : call.bytes)
            text += std::format(" {:02x}", static_cast<unsigned>(byte));
    }

    return text;
}
