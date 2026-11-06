// RegNotifyChangeKeyValue on virtual keys.

#include "oracle.h"
#include "thread.h"

#include <vreg.h>

#include <catch2/catch_test_macros.hpp>

#include <utility>

using namespace VReg;
using namespace VReg::Test;

namespace
{
constexpr DWORD NAME = REG_NOTIFY_CHANGE_NAME;
constexpr DWORD LAST_SET = REG_NOTIFY_CHANGE_LAST_SET;

class Event
{
public:
    Event() noexcept :
        m_hEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    {
    }

    ~Event()
    {
        if (m_hEvent)
            CloseHandle(m_hEvent);
    }

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    [[nodiscard]] HANDLE Get() const noexcept
    {
        return m_hEvent;
    }

    /// Writes signal before they return, so nothing needs to wait.
    [[nodiscard]] bool IsSet() const noexcept
    {
        return WaitForSingleObject(m_hEvent, 0) == WAIT_OBJECT_0;
    }

    [[nodiscard]] bool WaitSet() const noexcept
    {
        return WaitForSingleObject(m_hEvent, 5000) == WAIT_OBJECT_0;
    }

    void Reset() const noexcept
    {
        ResetEvent(m_hEvent);
    }

    /// Gives up ownership of the handle, which the caller then closes.
    HANDLE Detach() noexcept
    {
        return std::exchange(m_hEvent, nullptr);
    }

private:
    HANDLE m_hEvent;
};

LSTATUS Watch(
    VRegRegistry* const registry,
    const HKEY hKey,
    const bool isSubtree,
    const DWORD dwFilter,
    const Event& event
) noexcept
{
    return VRegNotifyChangeKeyValue(
        registry,
        hKey,
        isSubtree,
        dwFilter,
        event.Get(),
        TRUE
    );
}

HKEY Create(
    VRegRegistry* const registry,
    const HKEY hParent,
    const wchar_t* const pszPath
)
{
    HKEY hKey{};
    REQUIRE(
        VRegCreateKeyExW(
            registry,
            hParent,
            pszPath,
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hKey,
            nullptr
        ) == ERROR_SUCCESS
    );
    return hKey;
}

LSTATUS SetDword(
    VRegRegistry* const registry,
    const HKEY hKey,
    const wchar_t* const pszName,
    const DWORD dwValue
) noexcept
{
    return VRegSetValueExW(
        registry,
        hKey,
        pszName,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&dwValue),
        sizeof(dwValue)
    );
}

/// A writable memory hive mounted below `HKEY_CURRENT_USER`.
class MemoryMount
{
public:
    MemoryMount()
    {
        REQUIRE(
            VRegMount(
                m_registry,
                HKEY_CURRENT_USER,
                L"VRegNotify",
                m_hive,
                0
            ) == ERROR_SUCCESS
        );
    }

    [[nodiscard]] VRegRegistry* const Virtual() noexcept
    {
        return m_registry;
    }

    [[nodiscard]] VRegHive* const Memory() noexcept
    {
        return m_hive;
    }

    [[nodiscard]] HKEY Root() noexcept
    {
        return RootHandle(m_hive);
    }

private:
    OwnedHive m_hive = MakeHive();
    OwnedRegistry m_registry = MakeVirtualRegistry();
};
} // namespace

TEST_CASE("A value write signals a last-set watch once")
{
    MemoryMount mount;
    VRegRegistry* const registry = mount.Virtual();
    const HKEY hRoot = mount.Root();

    Event event;
    REQUIRE(Watch(registry, hRoot, FALSE, LAST_SET, event) == ERROR_SUCCESS);
    CHECK_FALSE(event.IsSet());

    CHECK(SetDword(registry, hRoot, L"v", 1) == ERROR_SUCCESS);
    CHECK(event.IsSet());

    event.Reset();
    CHECK(SetDword(registry, hRoot, L"v", 2) == ERROR_SUCCESS);
    CHECK_FALSE(event.IsSet());

    REQUIRE(Watch(registry, hRoot, FALSE, LAST_SET, event) == ERROR_SUCCESS);
    CHECK(
        VRegDeleteValueW(
            registry,
            hRoot,
            L"v"
        ) == ERROR_SUCCESS
    );
    CHECK(event.IsSet());
}

TEST_CASE("A watch fires only for the changes its filter selects")
{
    MemoryMount mount;
    VRegRegistry* const registry = mount.Virtual();
    const HKEY hRoot = mount.Root();

    Event onName;
    Event onLastSet;
    Event onAttributes;
    Event onSecurity;
    REQUIRE(Watch(registry, hRoot, FALSE, NAME, onName) == ERROR_SUCCESS);
    REQUIRE(
        Watch(registry, hRoot, FALSE, LAST_SET, onLastSet) == ERROR_SUCCESS
    );
    REQUIRE(
        Watch(
            registry,
            hRoot,
            FALSE,
            REG_NOTIFY_CHANGE_ATTRIBUTES,
            onAttributes
        ) == ERROR_SUCCESS
    );
    REQUIRE(
        Watch(
            registry,
            hRoot,
            FALSE,
            REG_NOTIFY_CHANGE_SECURITY,
            onSecurity
        ) == ERROR_SUCCESS
    );

    CHECK(SetDword(registry, hRoot, L"v", 1) == ERROR_SUCCESS);
    CHECK(onLastSet.IsSet());
    CHECK_FALSE(onName.IsSet());

    VRegCloseKey(
        registry,
        Create(registry, hRoot, L"Sub")
    );
    CHECK(onName.IsSet());

    CHECK_FALSE(onAttributes.IsSet());
    CHECK_FALSE(onSecurity.IsSet());
}

TEST_CASE("Opening an existing key or reading signals nothing")
{
    MemoryMount mount;
    VRegRegistry* const registry = mount.Virtual();
    const HKEY hRoot = mount.Root();
    VRegCloseKey(
        registry,
        Create(registry, hRoot, L"Sub")
    );

    Event event;
    REQUIRE(
        Watch(registry, hRoot, TRUE, NAME | LAST_SET, event) == ERROR_SUCCESS
    );

    VRegCloseKey(
        registry,
        Create(registry, hRoot, L"Sub")
    );
    HKEY hSub{};
    CHECK(
        VRegOpenKeyExW(
            registry,
            hRoot,
            L"Sub",
            0,
            KEY_READ,
            &hSub
        ) == ERROR_SUCCESS
    );
    DWORD dwValue{};
    DWORD cbValue = sizeof(dwValue);
    CHECK(
        VRegQueryValueExW(
            registry,
            hSub,
            L"missing",
            nullptr,
            nullptr,
            reinterpret_cast<BYTE*>(&dwValue),
            &cbValue
        )
        == ERROR_FILE_NOT_FOUND
    );
    CHECK_FALSE(event.IsSet());
}

TEST_CASE("Only a subtree watch sees changes below the key")
{
    MemoryMount mount;
    VRegRegistry* const registry = mount.Virtual();
    const HKEY hRoot = mount.Root();
    const HKEY hDeep = Create(registry, hRoot, L"A\\B\\C");

    Event shallow;
    Event subtree;
    REQUIRE(Watch(registry, hRoot, FALSE, LAST_SET, shallow) == ERROR_SUCCESS);
    REQUIRE(Watch(registry, hRoot, TRUE, LAST_SET, subtree) == ERROR_SUCCESS);

    CHECK(SetDword(registry, hDeep, L"v", 1) == ERROR_SUCCESS);
    CHECK(subtree.IsSet());
    CHECK_FALSE(shallow.IsSet());

    subtree.Reset();
    REQUIRE(Watch(registry, hRoot, TRUE, NAME, subtree) == ERROR_SUCCESS);
    VRegCloseKey(
        registry,
        Create(registry, hDeep, L"D")
    );
    CHECK(subtree.IsSet());
}

TEST_CASE("A watch on a subkey does not see its parent change")
{
    MemoryMount mount;
    VRegRegistry* const registry = mount.Virtual();
    const HKEY hRoot = mount.Root();
    const HKEY hSub = Create(registry, hRoot, L"Sub");

    Event event;
    REQUIRE(
        Watch(registry, hSub, TRUE, NAME | LAST_SET, event) == ERROR_SUCCESS
    );
    CHECK(SetDword(registry, hRoot, L"v", 1) == ERROR_SUCCESS);
    VRegCloseKey(
        registry,
        Create(registry, hRoot, L"Sibling")
    );
    CHECK_FALSE(event.IsSet());
}

TEST_CASE("The caller may close its event after registering")
{
    MemoryMount mount;
    VRegRegistry* const registry = mount.Virtual();
    const HKEY hRoot = mount.Root();

    Event event;
    HANDLE hKeep{};
    REQUIRE(
        DuplicateHandle(
            GetCurrentProcess(),
            event.Get(),
            GetCurrentProcess(),
            &hKeep,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS
        )
    );
    REQUIRE(Watch(registry, hRoot, FALSE, LAST_SET, event) == ERROR_SUCCESS);
    CloseHandle(event.Detach());

    CHECK(SetDword(registry, hRoot, L"v", 1) == ERROR_SUCCESS);
    CHECK(WaitForSingleObject(hKeep, 0) == WAIT_OBJECT_0);
    CloseHandle(hKeep);
}

TEST_CASE("Deleting a watched key signals it, and name watches on its parent")
{
    MemoryMount mount;
    VRegRegistry* const registry = mount.Virtual();
    const HKEY hRoot = mount.Root();
    const HKEY hSub = Create(registry, hRoot, L"Sub");

    Event onSub;
    Event onParent;
    REQUIRE(Watch(registry, hSub, FALSE, LAST_SET, onSub) == ERROR_SUCCESS);
    REQUIRE(Watch(registry, hRoot, FALSE, NAME, onParent) == ERROR_SUCCESS);

    CHECK(
        VRegDeleteKeyExW(
            registry,
            hRoot,
            L"Sub",
            0,
            0
        ) == ERROR_SUCCESS
    );
    CHECK(onSub.IsSet());
    CHECK(onParent.IsSet());

    Event late;
    CHECK(Watch(registry, hSub, FALSE, LAST_SET, late) == ERROR_KEY_DELETED);
}

TEST_CASE("A frozen hive accepts a watch and never signals")
{
    MemoryMount mount;
    VRegRegistry* const registry = mount.Virtual();
    const HKEY hRoot = mount.Root();
    VRegCloseKey(
        registry,
        Create(registry, hRoot, L"Sub")
    );
    VRegFreezeHive(mount.Memory());

    Event event;
    CHECK(
        Watch(registry, hRoot, TRUE, NAME | LAST_SET, event) == ERROR_SUCCESS
    );
    CHECK(SetDword(registry, hRoot, L"v", 1) == ERROR_ACCESS_DENIED);
    CHECK(
        VRegDeleteKeyExW(
            registry,
            hRoot,
            L"Sub",
            0,
            0
        ) == ERROR_ACCESS_DENIED
    );
    CHECK_FALSE(event.IsSet());
}

TEST_CASE("Freezing a hive drops its pending watches without signaling")
{
    MemoryMount mount;
    VRegRegistry* const registry = mount.Virtual();
    const HKEY hRoot = mount.Root();

    Event event;
    REQUIRE(
        Watch(registry, hRoot, TRUE, NAME | LAST_SET, event) == ERROR_SUCCESS
    );
    VRegFreezeHive(mount.Memory());
    CHECK_FALSE(event.IsSet());
}

TEST_CASE("Notification arguments are validated as before")
{
    MemoryMount mount;
    VRegRegistry* const registry = mount.Virtual();
    const HKEY hRoot = mount.Root();

    Event event;
    CHECK(
        VRegNotifyChangeKeyValue(
            registry,
            hRoot,
            FALSE,
            0x80,
            event.Get(),
            TRUE
        ) == ERROR_INVALID_PARAMETER
    );
    CHECK(
        VRegNotifyChangeKeyValue(
            registry,
            hRoot,
            FALSE,
            NAME,
            nullptr,
            TRUE
        ) == ERROR_INVALID_PARAMETER
    );
}

TEST_CASE("A synchronous call waits for a change")
{
    MemoryMount mount;
    VRegRegistry* const registry = mount.Virtual();
    const HKEY hRoot = mount.Root();

    LSTATUS status = ERROR_IO_PENDING;
    Thread waiter(
        [&]
        {
            status = VRegNotifyChangeKeyValue(
                registry,
                hRoot,
                FALSE,
                LAST_SET,
                nullptr,
                FALSE
            );
        }
    );
    CHECK_FALSE(waiter.Wait(200));

    // The registration may not be in place yet; write until the waiter returns.
    for (int i = 0; i < 100 && !waiter.Wait(50); ++i)
        SetDword(registry, hRoot, L"v", static_cast<DWORD>(i));

    REQUIRE(waiter.Wait(5000));
    CHECK(status == ERROR_SUCCESS);
}

TEST_CASE("Through an overlay, writes signal and lower changes do not")
{
    OverlayMount mount;
    VRegRegistry* const registry = mount.Registry();
    const HKEY hRoot = mount.Root();

    HKEY hLowerSub{};
    REQUIRE(
        VRegCreateKeyExW(
            mount.Native(),
            mount.LowerRoot(),
            L"Sub",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hLowerSub,
            nullptr
        )
        == ERROR_SUCCESS
    );
    HKEY hSub{};
    REQUIRE(
        VRegOpenKeyExW(
            registry,
            hRoot,
            L"Sub",
            0,
            KEY_ALL_ACCESS,
            &hSub
        ) == ERROR_SUCCESS
    );

    Event event;
    REQUIRE(
        Watch(registry, hRoot, TRUE, NAME | LAST_SET, event) == ERROR_SUCCESS
    );

    REQUIRE(SetDword(mount.Native(), hLowerSub, L"lower", 1) == ERROR_SUCCESS);
    VRegCloseKey(
        mount.Native(),
        hLowerSub
    );
    CHECK_FALSE(event.IsSet());

    CHECK(SetDword(registry, hSub, L"upper", 2) == ERROR_SUCCESS);
    CHECK(event.IsSet());

    event.Reset();
    REQUIRE(Watch(registry, hRoot, TRUE, NAME, event) == ERROR_SUCCESS);
    VRegCloseKey(
        registry,
        Create(registry, hRoot, L"New")
    );
    CHECK(event.IsSet());

    event.Reset();
    REQUIRE(Watch(registry, hRoot, TRUE, LAST_SET, event) == ERROR_SUCCESS);
    CHECK(
        VRegDeleteValueW(
            registry,
            hSub,
            L"upper"
        ) == ERROR_SUCCESS
    );
    CHECK(event.IsSet());
}

TEST_CASE("Opening a lower key through an overlay signals nothing")
{
    OverlayMount mount;
    VRegRegistry* const registry = mount.Registry();
    const HKEY hRoot = mount.Root();

    HKEY hLowerSub{};
    REQUIRE(
        VRegCreateKeyExW(
            mount.Native(),
            mount.LowerRoot(),
            L"Sub",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hLowerSub,
            nullptr
        )
        == ERROR_SUCCESS
    );
    VRegCloseKey(
        mount.Native(),
        hLowerSub
    );

    Event event;
    REQUIRE(
        Watch(registry, hRoot, TRUE, NAME | LAST_SET, event) == ERROR_SUCCESS
    );
    HKEY hSub{};
    CHECK(
        VRegOpenKeyExW(
            registry,
            hRoot,
            L"Sub",
            0,
            KEY_READ,
            &hSub
        ) == ERROR_SUCCESS
    );
    CHECK_FALSE(event.IsSet());

    // Deleting the lower key hides it: a name change for the parent.
    CHECK(
        VRegDeleteKeyExW(
            registry,
            hRoot,
            L"Sub",
            0,
            0
        ) == ERROR_SUCCESS
    );
    CHECK(event.IsSet());
}

TEST_CASE("Emptying a key in an overlay signals its watchers")
{
    OverlayMount mount;
    VRegRegistry* const registry = mount.Registry();
    const HKEY hRoot = mount.Root();

    HKEY hLowerSub{};
    REQUIRE(
        VRegCreateKeyExW(
            mount.Native(),
            mount.LowerRoot(),
            L"Sub",
            0,
            nullptr,
            0,
            KEY_ALL_ACCESS,
            nullptr,
            &hLowerSub,
            nullptr
        )
        == ERROR_SUCCESS
    );
    REQUIRE(SetDword(mount.Native(), hLowerSub, L"lower", 1) == ERROR_SUCCESS);
    VRegCloseKey(
        mount.Native(),
        hLowerSub
    );

    HKEY hSub{};
    REQUIRE(
        VRegOpenKeyExW(
            registry,
            hRoot,
            L"Sub",
            0,
            KEY_ALL_ACCESS,
            &hSub
        ) == ERROR_SUCCESS
    );
    Event event;
    REQUIRE(Watch(registry, hSub, FALSE, LAST_SET, event) == ERROR_SUCCESS);
    CHECK(
        VRegDeleteTreeW(
            registry,
            hSub,
            nullptr
        ) == ERROR_SUCCESS
    );
    CHECK(event.IsSet());
}
