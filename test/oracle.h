#pragma once

// The native-behavior oracle: a scenario is a sequence of registry calls run
// against several implementations, each on a fresh private app hive, and the
// recordings of status codes, out-sizes and bytes must match the real Win32
// API's exactly.

#include "objects.h"

#include <Windows.h>

#include <catch2/catch_tostring.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

namespace VRegTest
{
/// A registry hive loaded with `RegLoadAppKey` from a file in a fresh temp
/// directory: real, private to this process, and gone with the object.
class AppHive
{
public:
    AppHive();
    ~AppHive();

    AppHive(const AppHive&) = delete;
    AppHive& operator=(const AppHive&) = delete;

    /// The hive's root key, opened with `KEY_ALL_ACCESS`.
    [[nodiscard]] HKEY Root() const noexcept;

private:
    std::filesystem::path m_directory;
    HKEY m_hRoot{};
};

struct Call
{
    std::string label;
    LSTATUS status;
    std::vector<std::uint64_t> sizes;
    std::vector<std::byte> bytes;

    friend bool operator==(const Call&, const Call&) = default;
};

using Recording = std::vector<Call>;

/// Collects one scenario's calls.
class Recorder
{
public:
    /// Records a call: its status, then out-sizes and the full contents of
    /// out-buffers as they are after the call. Scenarios fill buffers and
    /// sizes with a sentinel beforehand, so what a call leaves untouched is
    /// recorded too. The out-sizes are read when this is called, so the call
    /// that fills them must have run already; `RECORD` in scenario.h sees to
    /// that.
    void operator()(
        std::string label,
        LSTATUS status,
        std::initializer_list<std::uint64_t> sizes = {},
        std::initializer_list<std::span<const std::byte>> buffers = {}
    );

    [[nodiscard]] Recording Take() noexcept;

private:
    Recording m_calls;
};

/// The bytes of a buffer, for @ref Recorder.
template <class T>
std::span<const std::byte> Bytes(const T& buffer) noexcept
{
    return std::as_bytes(std::span(buffer));
}

/// Runs @p Scenario against @p api on a fresh app hive.
template <class Scenario>
Recording Record(const Api& api)
{
    AppHive hive;
    Recorder recorder;
    Scenario{}(api, hive.Root(), recorder);
    return recorder.Take();
}

/// A virtual registry without a base, with a hive mounted below
/// `HKEY_CURRENT_USER`: the virtual counterpart of an @ref AppHive.
class VirtualHive
{
public:
    VirtualHive();

    VirtualHive(const VirtualHive&) = delete;
    VirtualHive& operator=(const VirtualHive&) = delete;

    [[nodiscard]] VReg* Registry() noexcept;

    /// The hive's keys are virtual.
    [[nodiscard]] Api AsApi() noexcept;

    /// The virtual handle of the hive's root.
    [[nodiscard]] HKEY Root() noexcept;

private:
    OwnedHive m_hive;
    OwnedRegistry m_registry;
};

/// A writable hive over an empty key of an @ref AppHive, mounted below
/// `HKEY_CURRENT_USER` in a virtual registry whose base is the native
/// registry: the overlay counterpart of an @ref AppHive.
class OverlayMount
{
public:
    OverlayMount();
    ~OverlayMount();

    OverlayMount(const OverlayMount&) = delete;
    OverlayMount& operator=(const OverlayMount&) = delete;

    [[nodiscard]] VReg* Registry() noexcept;
    [[nodiscard]] Api AsApi() noexcept;
    [[nodiscard]] VReg* Native() noexcept;
    [[nodiscard]] VRegHive* Overlay() noexcept;

    /// The key of the app hive the overlay sits on.
    [[nodiscard]] HKEY LowerRoot() const noexcept;

    /// The virtual handle of the overlay's root.
    [[nodiscard]] HKEY Root() noexcept;

private:
    AppHive m_appHive;
    OwnedRegistry m_native;
    HKEY m_hLower{};
    OwnedHive m_hive;
    OwnedRegistry m_registry;
};

/// Runs @p Scenario against a fresh @p Mount.
template <class Mount, class Scenario>
Recording RecordOn()
{
    Mount mount;
    Recorder recorder;
    Scenario{}(mount.AsApi(), mount.Root(), recorder);
    return recorder.Take();
}

/// Checks @p actual against @p expected call by call, reporting the first
/// few divergences with both calls in full.
void CheckSameRecording(const Recording& expected, const Recording& actual);
} // namespace VRegTest

template <>
struct Catch::StringMaker<VRegTest::Call>
{
    static std::string convert(const VRegTest::Call& call);
};
