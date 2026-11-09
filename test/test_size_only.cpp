// A size-only query must not copy the value. The global allocation functions
// are replaced for the whole test binary (forwarding to malloc and free) so a
// test can ask for the largest block requested on its thread meanwhile.

#include "oracle.h"

#include <vreg.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <malloc.h>
#include <new>
#include <vector>

using namespace VRegTest;

namespace
{
thread_local bool t_isTracking = false;
thread_local std::size_t t_cbLargest = 0;

void* Allocate(const std::size_t cb)
{
    if (t_isTracking)
        t_cbLargest = (std::max)(t_cbLargest, cb);

    if (void* const p = std::malloc(cb ? cb : 1))
        return p;

    throw std::bad_alloc();
}

void* AllocateAligned(const std::size_t cb, const std::align_val_t align)
{
    if (t_isTracking)
        t_cbLargest = (std::max)(t_cbLargest, cb);

    if (
        void* const p = _aligned_malloc(
            cb ? cb : 1,
            static_cast<std::size_t>(align)
        )
    )
        return p;

    throw std::bad_alloc();
}

/// Records the largest allocation made on this thread while alive.
class LargestAllocation
{
public:
    LargestAllocation() noexcept
    {
        t_cbLargest = 0;
        t_isTracking = true;
    }

    ~LargestAllocation()
    {
        t_isTracking = false;
    }

    LargestAllocation(const LargestAllocation&) = delete;
    LargestAllocation& operator=(const LargestAllocation&) = delete;

    [[nodiscard]] static std::size_t Get() noexcept
    {
        return t_cbLargest;
    }
};

constexpr DWORD VALUE_BYTES = 256 * 1024;
/// Far below the value, far above any bookkeeping a call makes.
constexpr std::size_t LIMIT_BYTES = VALUE_BYTES / 16;

/// Stores the large value the checks below ask for, through @p registry.
void SetBigValue(VReg* const registry, const HKEY hKey)
{
    const std::vector<std::byte> value(VALUE_BYTES, std::byte{0x5A});
    REQUIRE(
        VReg_SetValueExW(
            registry,
            hKey,
            L"Big",
            0,
            REG_BINARY,
            reinterpret_cast<const BYTE*>(value.data()),
            VALUE_BYTES
        ) == ERROR_SUCCESS
    );
}

/// Asks for the size of the large value, by name and by enumeration, and
/// checks that no allocation anywhere near its size happens meanwhile.
void CheckSizeOnly(VReg* const registry, const HKEY hKey)
{
    {
        DWORD dwType{};
        DWORD cbData = 0;
        LSTATUS status{};
        std::size_t cbLargest{};
        {
            const LargestAllocation tracker;
            status = VReg_QueryValueExW(
                registry,
                hKey,
                L"Big",
                nullptr,
                &dwType,
                nullptr,
                &cbData
            );
            cbLargest = LargestAllocation::Get();
        }
        CHECK(status == ERROR_SUCCESS);
        CHECK(dwType == REG_BINARY);
        CHECK(cbData == VALUE_BYTES);
        CHECK(cbLargest < LIMIT_BYTES);
    }

    {
        wchar_t szName[16]{};
        DWORD cchName = 16;
        DWORD dwType{};
        DWORD cbData = 0;
        LSTATUS status{};
        std::size_t cbLargest{};
        {
            const LargestAllocation tracker;
            status = VReg_EnumValueW(
                registry,
                hKey,
                0,
                szName,
                &cchName,
                nullptr,
                &dwType,
                nullptr,
                &cbData
            );
            cbLargest = LargestAllocation::Get();
        }
        CHECK(status == ERROR_SUCCESS);
        CHECK(dwType == REG_BINARY);
        CHECK(cbData == VALUE_BYTES);
        CHECK(cbLargest < LIMIT_BYTES);
    }
}
} // namespace

void* operator new(const std::size_t cb)
{
    return Allocate(cb);
}

void* operator new[](const std::size_t cb)
{
    return Allocate(cb);
}

void* operator new(const std::size_t cb, const std::nothrow_t&) noexcept
{
    try
    {
        return Allocate(cb);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](const std::size_t cb, const std::nothrow_t&) noexcept
{
    try
    {
        return Allocate(cb);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new(const std::size_t cb, const std::align_val_t align)
{
    return AllocateAligned(cb, align);
}

void* operator new[](const std::size_t cb, const std::align_val_t align)
{
    return AllocateAligned(cb, align);
}

void* operator new(
    const std::size_t cb,
    const std::align_val_t align,
    const std::nothrow_t&
) noexcept
{
    try
    {
        return AllocateAligned(cb, align);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](
    const std::size_t cb,
    const std::align_val_t align,
    const std::nothrow_t&
) noexcept
{
    try
    {
        return AllocateAligned(cb, align);
    }
    catch (...)
    {
        return nullptr;
    }
}

void operator delete(void* const p) noexcept
{
    std::free(p);
}

void operator delete[](void* const p) noexcept
{
    std::free(p);
}

void operator delete(void* const p, const std::size_t cb) noexcept
{
    (void)cb;
    std::free(p);
}

void operator delete[](void* const p, const std::size_t cb) noexcept
{
    (void)cb;
    std::free(p);
}

void operator delete(void* const p, const std::nothrow_t&) noexcept
{
    std::free(p);
}

void operator delete[](void* const p, const std::nothrow_t&) noexcept
{
    std::free(p);
}

void operator delete(void* const p, const std::align_val_t align) noexcept
{
    (void)align;
    _aligned_free(p);
}

void operator delete[](void* const p, const std::align_val_t align) noexcept
{
    (void)align;
    _aligned_free(p);
}

void operator delete(
    void* const p,
    const std::size_t cb,
    const std::align_val_t align
) noexcept
{
    (void)cb;
    (void)align;
    _aligned_free(p);
}

void operator delete[](
    void* const p,
    const std::size_t cb,
    const std::align_val_t align
) noexcept
{
    (void)cb;
    (void)align;
    _aligned_free(p);
}

void operator delete(
    void* const p,
    const std::align_val_t align,
    const std::nothrow_t&

) noexcept
{
    (void)align;
    _aligned_free(p);
}

void operator delete[](
    void* const p,
    const std::align_val_t align,
    const std::nothrow_t&

) noexcept
{
    (void)align;
    _aligned_free(p);
}

TEST_CASE("A size-only query of a Hive value copies no data")
{
    OwnedHive hive = MakeHive();
    OwnedRegistry registry = MakeVirtualRegistry();
    REQUIRE(
        VReg_Mount(registry, HKEY_CURRENT_USER, L"SizeOnly", hive, 0) == ERROR_SUCCESS
    );

    SetBigValue(registry, RootHandle(hive));
    CheckSizeOnly(registry, RootHandle(hive));
}

TEST_CASE("A size-only query of an Hive upper value copies no data")
{
    AppHive appHive;
    OwnedRegistry native = MakeNativeRegistry();
    OwnedHive overlay = MakeOverlayHive(native, appHive.Root(), L"");
    OwnedRegistry registry = MakeVirtualRegistry(native);
    REQUIRE(
        VReg_Mount(registry, HKEY_CURRENT_USER, L"SizeOnly", overlay, 0) == ERROR_SUCCESS
    );

    SetBigValue(registry, RootHandle(overlay));
    CheckSizeOnly(registry, RootHandle(overlay));
}

TEST_CASE("A size-only query of an Hive lower value copies no data")
{
    AppHive appHive;
    OwnedRegistry native = MakeNativeRegistry();
    SetBigValue(native, appHive.Root());

    OwnedHive overlay = MakeOverlayHive(native, appHive.Root(), L"");
    OwnedRegistry registry = MakeVirtualRegistry(native);
    REQUIRE(
        VReg_Mount(registry, HKEY_CURRENT_USER, L"SizeOnly", overlay, 0) == ERROR_SUCCESS
    );

    CheckSizeOnly(registry, RootHandle(overlay));
}
