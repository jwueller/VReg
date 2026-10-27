#pragma once

#include <VReg/handle.h>

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace VReg
{
/**
 * @brief Hands out virtual handles from the process-wide namespace that every
 *  copy of VReg in the process shares.
 *
 *  Each statically linked copy has its own instance, and all instances in a
 *  process meet in one named section that holds the next free block of
 *  handle values. An instance reserves whole blocks from it and hands out
 *  values from its own blocks without further shared traffic; it owns a
 *  handle exactly when the handle falls in one of its blocks.
 *
 * @note The section is mapped once and never unmapped or closed, so an
 *  instance must outlive every handle it issued: in practice, it is never
 *  destroyed (@ref Process).
 */
class HandleSpace
{
public:
    static constexpr std::uintptr_t kBlockSize = 4096;
    static constexpr std::uintptr_t kBlockCount = kVirtualHKeyCount / kBlockSize;

    /// The section's contents. Separately built copies of VReg read this
    /// layout, so it never changes; a different layout needs a different
    /// magic value and section name.
    struct Shared
    {
        /// @ref kMagic once a copy has claimed the section; zero before.
        LONG magic;
        LONG nextBlock;
    };

    static constexpr LONG kMagic = 0x56526731; // "VRg1"

    /// Opens or creates the section @p wsName. A failure leaves the instance
    /// unusable: @ref Allocate then answers null.
    explicit HandleSpace(std::wstring_view wsName) noexcept;

    HandleSpace(const HandleSpace&) = delete;
    HandleSpace& operator=(const HandleSpace&) = delete;

    /// This copy's instance over this process's section, created on first use
    /// and never destroyed.
    [[nodiscard]] static HandleSpace& Process() noexcept;

    [[nodiscard]] bool IsUsable() const noexcept;

    /// A fresh handle owned by this instance, or null when the section is
    /// unusable or the namespace is exhausted.
    [[nodiscard]] HKEY Allocate() noexcept;

    /// Whether @p hKey is a virtual handle from one of this instance's blocks.
    /// Lock-free.
    [[nodiscard]] bool Owns(HKEY hKey) const noexcept;

private:
    Shared* m_pShared = nullptr;

    std::mutex m_mutex;
    std::uintptr_t m_nextIndex = 0;
    std::uintptr_t m_endIndex = 0;

    std::array<std::atomic<std::uint64_t>, kBlockCount / 64> m_ownedBlocks{};
};
} // namespace VReg
