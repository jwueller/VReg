#pragma once

#include <Windows.h>

namespace VReg
{
class NativeRegistry;
class Registry;
} // namespace VReg

/**
 * @brief Routes the process's `Reg*` calls into a @ref VReg::Registry
 *  (normally a @ref VReg::VirtualRegistry) through Microsoft Detours.
 *
 *  Every `Reg*` export that takes a key handle is hooked, since a virtual
 *  handle reaching an unhooked one would fail as invalid. Most are answered
 *  by the registry; the rest (save, restore, load, security, rename, copy,
 *  transacted forms, reflection, predefined-key overrides) answer
 *  @ref ERROR_NOT_SUPPORTED for a virtual handle of this copy (the
 *  transacted forms also for a path into a mount) and pass everything else
 *  on.
 *
 *  Each hook is a small thunk outside the module image that jumps through a
 *  slot. The hooks are never detached: @ref Neutralize points every slot at
 *  the trampoline, so the layer passes everything straight through while
 *  other hookers' chains stay intact in any order. It runs by itself when
 *  the module is unloaded.
 *
 *  Hook bodies may run under a foreign loader lock; nothing they reach loads
 *  a module, creates a thread or waits on a lock a loader-lock holder could
 *  own.
 *
 * @note A thread still inside a hook body when its module unmaps crashes:
 *  the module MUST NOT unload while registry calls are in flight. A module
 *  that cannot rule that out MAY pin itself (@ref GetModuleHandleExW with
 *  @ref GET_MODULE_HANDLE_EX_FLAG_PIN), which keeps it loaded until the
 *  process exits.
 */
namespace VReg::Detours
{
/// Calls @ref DetourAttach for every hook, inside a transaction the caller
/// begins and commits (for example one that also carries its own hooks).
/// Every hook calls through @p registry, which MAY wrap the registry that
/// answers (to trace each call, say). @p native is the registry below the
/// hooks, the one @p registry reaches for whatever it does not answer: the
/// commit turns its function table into the trampolines. Both MUST outlive
/// the process. Answers the first Detours error, or
/// @ref ERROR_ALREADY_INITIALIZED when this copy attached before.
LONG Attach(Registry& registry, NativeRegistry& native) noexcept;

/// @ref Attach in a transaction of its own, enrolling only the calling
/// thread.
LONG Install(Registry& registry, NativeRegistry& native) noexcept;

/// Makes every hook of this copy pass calls straight through, for good.
void Neutralize() noexcept;
} // namespace VReg::Detours
