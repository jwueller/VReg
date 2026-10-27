#include "handle_space.h"

#include <cwchar>
#include <new>
#include <string>

namespace VReg
{
HandleSpace::HandleSpace(const std::wstring_view wsName) noexcept
{
    const std::wstring wsNameZ(wsName);

    // Pagefile-backed: the pages start zero-filled, which is the unclaimed
    // state. Open-or-create is atomic, so racing copies get the same section.
    const auto hSection = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(Shared),
        wsNameZ.c_str()
    );
    if (!hSection)
        return;

    auto* const pShared = static_cast<Shared*>(MapViewOfFile(hSection, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
    if (!pShared)
    {
        CloseHandle(hSection);
        return;
    }

    // The handle stays open on purpose, and so does the view: other copies'
    // handles were reserved through it, and a section that vanished and came
    // back zeroed would hand them out again.

    // Whoever moves magic from zero has claimed it; the layout version is part
    // of the magic value, so there is nothing else to publish.
    const auto previous = InterlockedCompareExchange(&pShared->magic, kMagic, 0);
    if (previous != 0 && previous != kMagic)
        return;

    m_pShared = pShared;
}

HandleSpace& HandleSpace::Process() noexcept
{
    static HandleSpace* const pSpace = []
    {
        wchar_t wszName[64];
        swprintf_s(wszName, L"Local\\VReg-handles-%lu", GetCurrentProcessId());

        // Leaked: handles issued from it outlive static destruction.
        return new (std::nothrow) HandleSpace(wszName);
    }();

    // Allocation of a few hundred bytes failing at first use leaves nothing to
    // recover; the namespace is unusable for the whole process either way.
    if (!pSpace)
        std::terminate();

    return *pSpace;
}

bool HandleSpace::IsUsable() const noexcept
{
    return m_pShared != nullptr;
}

HKEY HandleSpace::Allocate() noexcept
{
    if (!m_pShared)
        return nullptr;

    std::scoped_lock lock(m_mutex);

    if (m_nextIndex == m_endIndex)
    {
        // InterlockedIncrement returns the new value; the block reserved is the
        // one before it.
        const auto block = static_cast<std::uintptr_t>(InterlockedIncrement(&m_pShared->nextBlock)) - 1;
        if (block >= kBlockCount)
            return nullptr;

        m_ownedBlocks[block / 64].fetch_or(std::uint64_t{1} << (block % 64), std::memory_order_release);
        m_nextIndex = block * kBlockSize;
        m_endIndex = m_nextIndex + kBlockSize;
    }

    return VirtualHKeyFromIndex(m_nextIndex++);
}

bool HandleSpace::Owns(const HKEY hKey) const noexcept
{
    if (!IsVirtualHKey(hKey))
        return false;

    const auto block = IndexFromVirtualHKey(hKey) / kBlockSize;
    const auto bits = m_ownedBlocks[block / 64].load(std::memory_order_acquire);
    return (bits >> (block % 64)) & 1;
}
} // namespace VReg
