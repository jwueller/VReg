#include <VReg/detours.h>

#include <VReg/native_registry.h>
#include <VReg/registry.h>

#include "core/ansi.h"
#include "core/export_table.h"
#include "core/handle_space.h"
#include "core/status.h"

#include <detours/detours.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

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

    REFUSED_COUNT
};

std::array<void*, REFUSED_COUNT> g_refusedTargets{};

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

/// Whether a @ref RegOpenKeyTransactedW call names no transaction, which it
/// answers with @ref ERROR_INVALID_TRANSACTION. The hook answers that
/// itself: on ARM64 the function reaches that answer through a branch back
/// into its first 12 bytes, which the hook's jump overwrites.
///
/// Answered on every architecture rather than on ARM64 alone, since the native
/// answer is the same everywhere and one path is simpler.
bool IsUntransacted(
    const HKEY hKey,
    const LPCWSTR pszSubKey,
    const DWORD ulOptions,
    const REGSAM samDesired,
    const PHKEY phkResult,
    const HANDLE hTransaction,
    const PVOID pExtendedParemeter
) noexcept
{
    (void)hKey;
    (void)pszSubKey;
    (void)ulOptions;
    (void)samDesired;
    (void)phkResult;
    return !pExtendedParemeter && (!hTransaction || hTransaction == INVALID_HANDLE_VALUE);
}

/**
 * @brief @ref ERROR_NOT_SUPPORTED when @p hKey plus @p pszSubKey lies in a
 *  mount of @ref g_pRegistry, as a transacted call on a virtual handle
 *  answers; @ref ERROR_SUCCESS to pass the call on.
 *
 *  The registry is asked only through its `Reg*` surface, so a wrapper in
 *  front of it needs nothing more: each prefix of the path, the key itself
 *  included, is opened, and the path is in a mount when one of them opens as
 *  a virtual handle of this copy. Every prefix is tried because the real keys
 *  above a mount point need not exist.
 *
 *  It probes by opening rather than through a query beside
 *  @ref Registry, which a wrapper would have to forward and which would add a
 *  member outside the Win32 family. Opening is read-only, and a deprecated
 *  API can afford one open per path component.
 */
LSTATUS RefuseMountPath(const HKEY hKey, const LPCWSTR pszSubKey, const REGSAM samDesired) noexcept
try
{
    auto* const pRegistry = g_pRegistry.load(std::memory_order_acquire);
    const std::wstring_view subKey = pszSubKey ? pszSubKey : L"";
    for (std::size_t end = 0; end <= subKey.size(); ++end)
    {
        if (end != subKey.size() && subKey[end] != L'\\')
            continue;

        const std::wstring prefix(subKey.substr(0, end));
        HKEY hProbe{};
        const auto status = pRegistry->RegOpenKeyExW(
            hKey,
            prefix.c_str(),
            0,
            KEY_QUERY_VALUE | (samDesired & KEY_WOW64_RES),
            &hProbe
        );
        if (status != ERROR_SUCCESS)
            continue;

        const bool isVirtual = IsOwned(hProbe);
        // An empty path may answer hKey itself rather than a new handle.
        if (hProbe != hKey)
        {
            pRegistry->RegCloseKey(
                hProbe
            );
        }
        if (isVirtual)
            return ERROR_NOT_SUPPORTED;
    }
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS RefuseMountPath(const HKEY hKey, const LPCSTR pszSubKey, const REGSAM samDesired) noexcept
try
{
    const WideArg subKey(pszSubKey);
    return RefuseMountPath(hKey, subKey.Get(), samDesired);
}
catch (...)
{
    return StatusFromException();
}

/// @ref RefuseMountPath for the arguments of @ref RegCreateKeyTransactedW.
template <class String, class Class>
LSTATUS RefuseMountPath(
    const HKEY hKey,
    const String lpSubKey,
    const DWORD Reserved,
    const Class lpClass,
    const DWORD dwOptions,
    const REGSAM samDesired,
    const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    const PHKEY phkResult,
    const LPDWORD lpdwDisposition,
    const HANDLE hTransaction,
    const PVOID pExtendedParemeter
) noexcept
{
    (void)Reserved;
    (void)lpClass;
    (void)dwOptions;
    (void)lpSecurityAttributes;
    (void)phkResult;
    (void)lpdwDisposition;
    (void)hTransaction;
    (void)pExtendedParemeter;
    return RefuseMountPath(hKey, lpSubKey, samDesired);
}

/// @ref RefuseMountPath for the arguments of @ref RegOpenKeyTransactedW.
template <class String>
LSTATUS RefuseMountPath(
    const HKEY hKey,
    const String lpSubKey,
    const DWORD ulOptions,
    const REGSAM samDesired,
    const PHKEY phkResult,
    const HANDLE hTransaction,
    const PVOID pExtendedParemeter
) noexcept
{
    (void)ulOptions;
    (void)phkResult;
    (void)hTransaction;
    (void)pExtendedParemeter;
    return RefuseMountPath(hKey, lpSubKey, samDesired);
}

/// @ref RefuseMountPath for the arguments of @ref RegDeleteKeyTransactedW.
template <class String>
LSTATUS RefuseMountPath(
    const HKEY hKey,
    const String lpSubKey,
    const REGSAM samDesired,
    const DWORD Reserved,
    const HANDLE hTransaction,
    const PVOID pExtendedParemeter
) noexcept
{
    (void)Reserved;
    (void)hTransaction;
    (void)pExtendedParemeter;
    return RefuseMountPath(hKey, lpSubKey, samDesired);
}

constexpr bool IsTransacted(const std::size_t index) noexcept
{
    return index >= kRegCreateKeyTransactedA && index <= kRegDeleteKeyTransactedW;
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

        if constexpr (IsTransacted(Index))
        {
            const auto status = RefuseMountPath(args...);
            if (status != ERROR_SUCCESS)
                return status;
        }

        if constexpr (Index == kRegOpenKeyTransactedW)
        {
            if (IsUntransacted(args...))
                return ERROR_INVALID_TRANSACTION;
        }

        const auto pfnNext = reinterpret_cast<LSTATUS(APIENTRY*)(Args...)>(g_refusedTargets[Index]);
        return pfnNext(args...);
    }
};

/**
 * @brief The thunks every hook detours to: `jmp [slot]`, in memory outside
 *  any module image so they outlive the module that made them.
 *
 *  Code and slots sit on two pages of one allocation, so an x64 or ARM64
 *  thunk can reach its slot PC-relative. The memory is leaked on purpose:
 *  another hooker's trampoline may jump into a thunk after this module is
 *  gone.
 */
class Thunks
{
public:
    static constexpr std::size_t CODE_SIZE = 16;
    static constexpr std::size_t PAGE_SIZE = 4096;
    static constexpr std::size_t CAPACITY = PAGE_SIZE / CODE_SIZE;

    [[nodiscard]] bool Allocate() noexcept
    {
        m_pbBase = static_cast<std::byte*>(VirtualAlloc(
            nullptr,
            2 * PAGE_SIZE,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_READWRITE
        ));
        return m_pbBase != nullptr;
    }

    /// Writes thunk @p index, initially jumping to @p pTarget.
    void* Write(const std::size_t index, void* const pTarget) noexcept
    {
        auto* const pbCode = m_pbBase + index * CODE_SIZE;
        auto* const ppSlot = Slot(index);
        *ppSlot = pTarget;

#if defined(_M_ARM64EC)
#error "ARM64EC is not supported: its x64 and native callers need hooks of their own."
#elif defined(_M_ARM64)
        // ldr x16, slot (PC-relative, in words); br x16.
        const auto offset = reinterpret_cast<std::intptr_t>(ppSlot) - reinterpret_cast<std::intptr_t>(pbCode);
        const auto ldr = 0x58000010u | (static_cast<std::uint32_t>(offset / 4) << 5);
        const auto br = 0xD61F0200u;
        std::memcpy(pbCode, &ldr, sizeof(ldr));
        std::memcpy(pbCode + 4, &br, sizeof(br));
#else
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
#endif
        return pbCode;
    }

    /// Makes the code page executable and read-only.
    [[nodiscard]] bool Seal() noexcept
    {
        DWORD dwOld{};
        if (!VirtualProtect(m_pbBase, PAGE_SIZE, PAGE_EXECUTE_READ, &dwOld))
            return false;

        FlushInstructionCache(GetCurrentProcess(), m_pbBase, PAGE_SIZE);
        return true;
    }

    [[nodiscard]] void** Slot(const std::size_t index) const noexcept
    {
        return reinterpret_cast<void**>(m_pbBase + PAGE_SIZE) + index;
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
std::array<void**, Thunks::CAPACITY> g_ppTargets{};
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

LONG Attach(Registry& registry, NativeRegistry& native) noexcept
{
    static std::atomic<bool> s_isAttached;
    if (s_isAttached.exchange(true))
        return ERROR_ALREADY_INITIALIZED;

    g_pRegistry.store(&registry, std::memory_order_release);

    auto& functions = native.GetFunctions();
    std::array<Hook, Thunks::CAPACITY> hooks{};
    std::size_t cHooks = 0;

#define VREG_EMULATED(name)                                                                                            \
    hooks[cHooks++] = {                                                                                                \
        reinterpret_cast<void**>(&functions.name),                                                                     \
        reinterpret_cast<void*>(&Emulated<decltype(functions.name), &Registry::name>::Body)                            \
    };
    VREG_INTERNAL_REGISTRY_FUNCTIONS(VREG_EMULATED)
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

    std::array<void*, Thunks::CAPACITY> thunks{};
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
        const auto error = DetourAttach(hooks[i].ppTarget, thunks[i]);
        if (error != NO_ERROR)
        {
            g_cHooks.store(0, std::memory_order_release);
            s_isAttached = false;
            return error;
        }
    }

    return NO_ERROR;
}

LONG Install(Registry& registry, NativeRegistry& native) noexcept
{
    const auto began = DetourTransactionBegin();
    if (began != NO_ERROR)
        return began;

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
