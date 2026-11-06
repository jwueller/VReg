#pragma once

#include <VReg/api.h>
#include <VReg/registry.h>

#include <Windows.h>

/**
 * @file
 * @brief Routes the process's `Reg*` calls into a @ref VRegRegistry (normally
 *  a virtual registry) through Microsoft Detours.
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
 *  slot. The hooks are never detached: @ref VRegNeutralizeHooks points every
 *  slot at the trampoline, so the layer passes everything straight through
 *  while other hookers' chains stay intact in any order. It runs by itself
 *  when the module is unloaded.
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

#ifdef __cplusplus
extern "C" {
#endif

/// Calls @ref DetourAttach for every hook, inside a transaction the caller
/// begins and commits (for example one that also carries its own hooks).
/// Every hook calls through @p pRegistry, which MAY wrap the registry that
/// answers (to trace each call, say). @p pNative is the registry below the
/// hooks, the one @p pRegistry reaches for whatever it does not answer: it
/// MUST be one @ref VRegCreateNativeRegistry made, and the commit turns its
/// function table into the trampolines. Both MUST outlive the process.
/// Answers the first Detours error, or @ref ERROR_ALREADY_INITIALIZED when
/// this copy attached before.
VREGDEF LONG WINAPI VRegAttachHooks(_In_ VRegRegistry* pRegistry, _In_ VRegRegistry* pNative);

/// @ref VRegAttachHooks in a transaction of its own, enrolling only the
/// calling thread.
VREGDEF LONG WINAPI VRegInstallHooks(_In_ VRegRegistry* pRegistry, _In_ VRegRegistry* pNative);

/// Makes every hook of this copy pass calls straight through, for good.
VREGDEF void WINAPI VRegNeutralizeHooks(void);

#ifdef __cplusplus
} // extern "C"
#endif
