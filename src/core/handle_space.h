#pragma once

#include <VReg/handle.h>

#include <Windows.h>

#include <array>
#include <atomic>

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
    static constexpr UINT_PTR BLOCK_SIZE = 4096;
    static constexpr UINT_PTR BLOCK_COUNT = VREG_VIRTUAL_HKEY_COUNT / BLOCK_SIZE;

    /// The section's contents. Separately built copies of VReg read this
    /// layout, so it never changes; a different layout needs a different
    /// magic value and section name.
    struct Shared
    {
        /// @ref MAGIC once a copy has claimed the section; zero before.
        LONG magic;
        LONG nextBlock;
    };

    static constexpr LONG MAGIC = 0x56526731; // "VRg1"

    /// Opens or creates the section @p pszName. A failure leaves the instance
    /// unusable: @ref Allocate then answers null.
    explicit HandleSpace(LPCWSTR pszName) noexcept;

    HandleSpace(const HandleSpace&) = delete;
    HandleSpace& operator=(const HandleSpace&) = delete;

    /// This copy's instance over this process's section, created on first use
    /// and never destroyed.
    [[nodiscard]] static HandleSpace& Process() noexcept;

    [[nodiscard]] bool IsUsable() const noexcept;

    /// A fresh handle owned by this instance and bound to @p pTarget, or null
    /// when the section is unusable, the namespace is exhausted or memory ran
    /// out.
    [[nodiscard]] HKEY Allocate(void* pTarget) noexcept;

    /// Whether @p hKey is a virtual handle from one of this instance's blocks.
    /// Lock-free.
    [[nodiscard]] bool Owns(HKEY hKey) const noexcept;

    /// Unbinds @p hKey from its target. The value is never handed out again;
    /// later lookups answer null.
    void Release(HKEY hKey) noexcept;

    /// The target @p hKey was allocated for; null when this instance does not
    /// own it. Lock-free.
    [[nodiscard]] void* Find(HKEY hKey) const noexcept;

private:
    using Targets = std::array<std::atomic<void*>, BLOCK_SIZE>;

    // Two levels of block directories, filled in as blocks are reserved and
    // never freed: a lookup is two acquire loads and an index.
    static constexpr UINT_PTR DIRECTORY_SIZE = 512;
    using Directory = std::array<std::atomic<Targets*>, DIRECTORY_SIZE>;

    [[nodiscard]] const Targets* FindBlock(UINT_PTR block) const noexcept;
    [[nodiscard]] bool AddBlock(UINT_PTR block) noexcept;

    Shared* m_pShared = nullptr;

    SRWLOCK m_lock = SRWLOCK_INIT;
    UINT_PTR m_nextIndex = 0;
    UINT_PTR m_endIndex = 0;

    std::array<std::atomic<Directory*>, BLOCK_COUNT / DIRECTORY_SIZE> m_directories{};
};
} // namespace VReg
