#include <VReg/detours.h>

#include <VReg/native_registry.h>
#include <VReg/virtual_registry.h>

#include "core/export_table.h"
#include "core/handle_space.h"

#include <detours/detours.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace VReg::Detours
{
namespace
{
std::atomic<Registry*> g_pRegistry;

template <class Function, auto Member>
struct Emulated;

/// A hook the registry answers: the same call, made on @ref g_pRegistry.
template <class... Args, auto Member>
struct Emulated<LSTATUS(APIENTRY*)(Args...), Member>
{
    static LSTATUS APIENTRY Body(Args... args) noexcept
    {
        return (g_pRegistry.load(std::memory_order_acquire)->*Member)(args...);
    }
};

// The functions refused on virtual handles, by index into their resolved
// entry points (the trampolines once the hooks are committed).
#define VREG_REFUSED_HOOKS(X)                                                                                          \
    X(RegSaveKeyA)                                                                                                     \
    X(RegSaveKeyW)                                                                                                     \
    X(RegSaveKeyExA)                                                                                                   \
    X(RegSaveKeyExW)                                                                                                   \
    X(RegRestoreKeyA)                                                                                                  \
    X(RegRestoreKeyW)                                                                                                  \
    X(RegLoadKeyA)                                                                                                     \
    X(RegLoadKeyW)                                                                                                     \
    X(RegUnLoadKeyA)                                                                                                   \
    X(RegUnLoadKeyW)                                                                                                   \
    X(RegReplaceKeyA)                                                                                                  \
    X(RegReplaceKeyW)                                                                                                  \
    X(RegGetKeySecurity)                                                                                               \
    X(RegSetKeySecurity)                                                                                               \
    X(RegRenameKey)                                                                                                    \
    X(RegCopyTreeA)                                                                                                    \
    X(RegCopyTreeW)                                                                                                    \
    X(RegCreateKeyTransactedA)                                                                                         \
    X(RegCreateKeyTransactedW)                                                                                         \
    X(RegOpenKeyTransactedA)                                                                                           \
    X(RegOpenKeyTransactedW)                                                                                           \
    X(RegDeleteKeyTransactedA)                                                                                         \
    X(RegDeleteKeyTransactedW)                                                                                         \
    X(RegDisableReflectionKey)                                                                                         \
    X(RegEnableReflectionKey)                                                                                          \
    X(RegQueryReflectionKey)                                                                                           \
    X(RegOverridePredefKey)

enum RefusedIndex : std::size_t
{
#define VREG_INDEX(name) k##name,
    VREG_REFUSED_HOOKS(VREG_INDEX)
#undef VREG_INDEX
        kRefusedCount
};

std::array<void*, kRefusedCount> g_refusedTargets{};

bool IsOwned(const HKEY hKey) noexcept
{
    return HandleSpace::Process().Owns(hKey);
}

template <class T>
bool IsOwned(const T& value) noexcept
{
    (void)value;
    return false;
}

template <class Function, std::size_t Index>
struct Refused;

/// A hook nothing virtual can answer: refused for this copy's handles,
/// passed on for every other.
template <class... Args, std::size_t Index>
struct Refused<LSTATUS(APIENTRY*)(Args...), Index>
{
    static LSTATUS APIENTRY Body(Args... args) noexcept
    {
        if ((IsOwned(args) || ...))
            return ERROR_NOT_SUPPORTED;

        const auto pfnNext = reinterpret_cast<LSTATUS(APIENTRY*)(Args...)>(g_refusedTargets[Index]);
        return pfnNext(args...);
    }
};

/**
 * @brief The thunks every hook detours to: `jmp [slot]`, in memory outside
 *  any module image so they outlive the module that made them.
 *
 *  Code and slots sit on two pages of one allocation, so an x64 thunk can
 *  reach its slot RIP-relative. The memory is leaked on purpose: another
 *  hooker's trampoline may jump into a thunk after this module is gone.
 */
class Thunks
{
public:
    static constexpr std::size_t kCodeSize = 16;
    static constexpr std::size_t kPageSize = 4096;
    static constexpr std::size_t kCapacity = kPageSize / kCodeSize;

    [[nodiscard]] bool Allocate() noexcept
    {
        m_pbBase = static_cast<std::byte*>(VirtualAlloc(nullptr, 2 * kPageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        return m_pbBase != nullptr;
    }

    /// Writes thunk @p index, initially jumping to @p pTarget.
    void* Write(const std::size_t index, void* const pTarget) noexcept
    {
        auto* const pbCode = m_pbBase + index * kCodeSize;
        auto* const ppSlot = Slot(index);
        *ppSlot = pTarget;

        // jmp qword/dword ptr [slot]: RIP-relative on x64, absolute on x86.
        pbCode[0] = std::byte{0xFF};
        pbCode[1] = std::byte{0x25};
#if defined(_M_X64)
        const auto displacement = static_cast<std::int32_t>(
            reinterpret_cast<std::intptr_t>(ppSlot) - reinterpret_cast<std::intptr_t>(pbCode + 6)
        );
        std::memcpy(pbCode + 2, &displacement, sizeof(displacement));
#else
        const auto address = reinterpret_cast<std::uint32_t>(ppSlot);
        std::memcpy(pbCode + 2, &address, sizeof(address));
#endif
        return pbCode;
    }

    /// Makes the code page executable and read-only.
    [[nodiscard]] bool Seal() noexcept
    {
        DWORD dwOld{};
        if (!VirtualProtect(m_pbBase, kPageSize, PAGE_EXECUTE_READ, &dwOld))
            return false;

        FlushInstructionCache(GetCurrentProcess(), m_pbBase, kPageSize);
        return true;
    }

    [[nodiscard]] void** Slot(const std::size_t index) const noexcept
    {
        return reinterpret_cast<void**>(m_pbBase + kPageSize) + index;
    }

private:
    std::byte* m_pbBase = nullptr;
};

struct Hook
{
    /// Where Detours keeps the entry point and, once committed, the
    /// trampoline.
    void** ppTarget;
    void* pBody;
};

Thunks g_thunks;
std::array<void**, Thunks::kCapacity> g_ppTargets{};
std::atomic<std::size_t> g_cHooks;

/// Neutralizes the hooks when the module that holds the hook bodies is
/// unloaded (or the process exits).
struct NeutralizeOnUnload
{
    ~NeutralizeOnUnload()
    {
        Neutralize();
    }
} g_neutralizeOnUnload;
} // namespace

LONG Attach(VirtualRegistry& registry, NativeRegistry& native) noexcept
{
    static std::atomic<bool> s_isAttached;
    if (s_isAttached.exchange(true))
        return ERROR_ALREADY_INITIALIZED;

    g_pRegistry.store(&registry, std::memory_order_release);

    auto& functions = native.GetFunctions();
    std::array<Hook, Thunks::kCapacity> hooks{};
    std::size_t cHooks = 0;

#define VREG_EMULATED(name, member)                                                                                    \
    hooks[cHooks++] = {                                                                                                \
        reinterpret_cast<void**>(&functions.name),                                                                     \
        reinterpret_cast<void*>(&Emulated<decltype(functions.name), &Registry::member>::Body)                          \
    };
    VREG_EMULATED(RegOpenKeyExW, OpenKeyExW)
    VREG_EMULATED(RegOpenKeyExA, OpenKeyExA)
    VREG_EMULATED(RegOpenKeyW, OpenKeyW)
    VREG_EMULATED(RegOpenKeyA, OpenKeyA)
    VREG_EMULATED(RegCreateKeyExW, CreateKeyExW)
    VREG_EMULATED(RegCreateKeyExA, CreateKeyExA)
    VREG_EMULATED(RegCreateKeyW, CreateKeyW)
    VREG_EMULATED(RegCreateKeyA, CreateKeyA)
    VREG_EMULATED(RegCloseKey, CloseKey)
    VREG_EMULATED(RegQueryValueExW, QueryValueExW)
    VREG_EMULATED(RegQueryValueExA, QueryValueExA)
    VREG_EMULATED(RegQueryValueW, QueryValueW)
    VREG_EMULATED(RegQueryValueA, QueryValueA)
    VREG_EMULATED(RegSetValueExW, SetValueExW)
    VREG_EMULATED(RegSetValueExA, SetValueExA)
    VREG_EMULATED(RegSetValueW, SetValueW)
    VREG_EMULATED(RegSetValueA, SetValueA)
    VREG_EMULATED(RegDeleteValueW, DeleteValueW)
    VREG_EMULATED(RegDeleteValueA, DeleteValueA)
    VREG_EMULATED(RegDeleteKeyExW, DeleteKeyExW)
    VREG_EMULATED(RegDeleteKeyExA, DeleteKeyExA)
    VREG_EMULATED(RegDeleteKeyW, DeleteKeyW)
    VREG_EMULATED(RegDeleteKeyA, DeleteKeyA)
    VREG_EMULATED(RegDeleteTreeW, DeleteTreeW)
    VREG_EMULATED(RegDeleteTreeA, DeleteTreeA)
    VREG_EMULATED(RegEnumKeyExW, EnumKeyExW)
    VREG_EMULATED(RegEnumKeyExA, EnumKeyExA)
    VREG_EMULATED(RegEnumKeyW, EnumKeyW)
    VREG_EMULATED(RegEnumKeyA, EnumKeyA)
    VREG_EMULATED(RegEnumValueW, EnumValueW)
    VREG_EMULATED(RegEnumValueA, EnumValueA)
    VREG_EMULATED(RegQueryInfoKeyW, QueryInfoKeyW)
    VREG_EMULATED(RegQueryInfoKeyA, QueryInfoKeyA)
    VREG_EMULATED(RegLoadMUIStringW, LoadMUIStringW)
    VREG_EMULATED(RegLoadMUIStringA, LoadMUIStringA)
    VREG_EMULATED(RegGetValueW, GetValueW)
    VREG_EMULATED(RegGetValueA, GetValueA)
    VREG_EMULATED(RegNotifyChangeKeyValue, NotifyChangeKeyValue)
    VREG_EMULATED(RegFlushKey, FlushKey)
    VREG_EMULATED(RegSetKeyValueW, SetKeyValueW)
    VREG_EMULATED(RegSetKeyValueA, SetKeyValueA)
    VREG_EMULATED(RegDeleteKeyValueW, DeleteKeyValueW)
    VREG_EMULATED(RegDeleteKeyValueA, DeleteKeyValueA)
    VREG_EMULATED(RegQueryMultipleValuesW, QueryMultipleValuesW)
    VREG_EMULATED(RegQueryMultipleValuesA, QueryMultipleValuesA)
#undef VREG_EMULATED

    // A refused function this system does not export has nothing to hook.
#define VREG_REFUSED(name)                                                                                             \
    g_refusedTargets[k##name] = FindRegistryExport(#name);                                                             \
    if (g_refusedTargets[k##name])                                                                                     \
    {                                                                                                                  \
        hooks[cHooks++] = {                                                                                            \
            &g_refusedTargets[k##name],                                                                                \
            reinterpret_cast<void*>(&Refused<decltype(&::name), k##name>::Body)                                        \
        };                                                                                                             \
    }
    VREG_REFUSED_HOOKS(VREG_REFUSED)
#undef VREG_REFUSED

    if (!g_thunks.Allocate())
    {
        s_isAttached = false;
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    for (std::size_t i = 0; i < cHooks; ++i)
        g_ppTargets[i] = hooks[i].ppTarget;

    std::array<void*, Thunks::kCapacity> thunks{};
    for (std::size_t i = 0; i < cHooks; ++i)
        thunks[i] = g_thunks.Write(i, hooks[i].pBody);

    if (!g_thunks.Seal())
    {
        s_isAttached = false;
        return static_cast<LONG>(GetLastError());
    }

    g_cHooks.store(cHooks, std::memory_order_release);
    for (std::size_t i = 0; i < cHooks; ++i)
    {
        // The caller aborts the transaction on failure, so nothing is hooked
        // and a later attempt may start over (with fresh thunks).
        if (const auto error = DetourAttach(hooks[i].ppTarget, thunks[i]); error != NO_ERROR)
        {
            g_cHooks.store(0, std::memory_order_release);
            s_isAttached = false;
            return error;
        }
    }

    return NO_ERROR;
}

LONG Install(VirtualRegistry& registry, NativeRegistry& native) noexcept
{
    if (const auto error = DetourTransactionBegin(); error != NO_ERROR)
        return error;

    auto error = DetourUpdateThread(GetCurrentThread());
    if (error == NO_ERROR)
        error = Attach(registry, native);

    if (error != NO_ERROR)
    {
        DetourTransactionAbort();
        return error;
    }

    return DetourTransactionCommit();
}

void Neutralize() noexcept
{
    // Whatever the target field holds is what lies below this layer: the
    // trampoline once committed, the function itself before.
    const auto cHooks = g_cHooks.load(std::memory_order_acquire);
    for (std::size_t i = 0; i < cHooks; ++i)
        InterlockedExchangePointer(g_thunks.Slot(i), *g_ppTargets[i]);
}
} // namespace VReg::Detours
