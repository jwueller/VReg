#include "oracle.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <format>
#include <utility>

#pragma comment(lib, "advapi32.lib")

namespace VReg::Test
{
AppHive::AppHive()
{
    static std::atomic<unsigned> s_counter;

    m_directory = std::filesystem::temp_directory_path()
                  / std::format(L"VReg-oracle-{}-{}", GetCurrentProcessId(), s_counter++);
    std::filesystem::remove_all(m_directory);
    std::filesystem::create_directories(m_directory);

    const auto file = m_directory / L"hive.dat";
    const auto error = RegLoadAppKeyW(file.c_str(), &m_hRoot, KEY_ALL_ACCESS, 0, 0);
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

void Recorder::operator()(
    std::string label,
    const LSTATUS status,
    const std::initializer_list<std::uint64_t> sizes,
    const std::initializer_list<std::span<const std::byte>> buffers
)
{
    Call call{.label = std::move(label), .status = status, .sizes = sizes, .bytes = {}};
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
    for (std::size_t i = 0; i < expected.size() && i < actual.size(); ++i)
    {
        if (expected[i] != actual[i])
        {
            INFO("call " << i);
            CHECK(expected[i] == actual[i]);
            return;
        }
    }

    CHECK(expected.size() == actual.size());
}
} // namespace VReg::Test

std::string Catch::StringMaker<VReg::Test::Call>::convert(const VReg::Test::Call& call)
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
