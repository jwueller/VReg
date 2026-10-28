#include "handle_space.h"

#include "lock.h"

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

    auto* const pShared = static_cast<Shared*>(MapViewOfFile(
        hSection,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(Shared)
    ));
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
    const auto previous = InterlockedCompareExchange(
        &pShared->magic,
        kMagic,
        0
    );
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
        return new(std::nothrow) HandleSpace(wszName);
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

HKEY HandleSpace::Allocate(void* const pTarget) noexcept
{
    if (!m_pShared)
        return nullptr;

    ExclusiveLock lock(m_lock);

    if (m_nextIndex == m_endIndex)
    {
        // InterlockedIncrement returns the new value; the block reserved is the
        // one before it.
        const auto block = static_cast<std::uintptr_t>(InterlockedIncrement(&m_pShared->nextBlock)) - 1;
        if (block >= kBlockCount || !AddBlock(block))
            return nullptr;

        m_nextIndex = block * kBlockSize;
        m_endIndex = m_nextIndex + kBlockSize;
    }

    const auto index = m_nextIndex++;
    auto* const pTargets = const_cast<Targets*>(FindBlock(index / kBlockSize));
    (*pTargets)[index % kBlockSize].store(pTarget, std::memory_order_release);
    return VirtualHKeyFromIndex(index);
}

bool HandleSpace::Owns(const HKEY hKey) const noexcept
{
    return IsVirtualHKey(hKey) && FindBlock(IndexFromVirtualHKey(hKey) / kBlockSize) != nullptr;
}

void HandleSpace::Release(const HKEY hKey) noexcept
{
    if (!IsVirtualHKey(hKey))
        return;

    const auto index = IndexFromVirtualHKey(hKey);
    if (auto* const pTargets = const_cast<Targets*>(FindBlock(index / kBlockSize)))
        (*pTargets)[index % kBlockSize].store(
            nullptr,
            std::memory_order_release
        );
}

void* HandleSpace::Find(const HKEY hKey) const noexcept
{
    if (!IsVirtualHKey(hKey))
        return nullptr;

    const auto index = IndexFromVirtualHKey(hKey);
    const auto* const pTargets = FindBlock(index / kBlockSize);
    if (!pTargets)
        return nullptr;

    return (*pTargets)[index % kBlockSize].load(std::memory_order_acquire);
}

const HandleSpace::Targets* HandleSpace::FindBlock(const std::uintptr_t block) const noexcept
{
    const auto* const pDirectory = m_directories[block / kDirectorySize].load(std::memory_order_acquire);
    if (!pDirectory)
        return nullptr;

    return (*pDirectory)[block % kDirectorySize].load(std::memory_order_acquire);
}

bool HandleSpace::AddBlock(const std::uintptr_t block) noexcept
{
    auto& directorySlot = m_directories[block / kDirectorySize];
    auto* pDirectory = directorySlot.load(std::memory_order_relaxed);
    if (!pDirectory)
    {
        pDirectory = new(std::nothrow) Directory{};
        if (!pDirectory)
            return false;

        directorySlot.store(pDirectory, std::memory_order_release);
    }

    auto* const pTargets = new(std::nothrow) Targets{};
    if (!pTargets)
        return false;

    (*pDirectory)[block % kDirectorySize].store(
        pTargets,
        std::memory_order_release
    );
    return true;
}
} // namespace VReg
