# VReg design

A virtualized Windows registry library: a registry interface with the Win32
signatures, in-memory and overlay storage mounted into a virtual namespace,
`.reg` import/export, and an optional Detours adapter that routes a process's
`Reg*` calls into it. The project grew out of the virtual registry in
DirectRetrofit, which becomes its first consumer.

## Goals

- Usable standalone as a registry that defers to an underlying
  implementation (native or virtual), and as a pure in-memory registry with
  no real registry behind it.
- One virtual namespace per process that several modules contribute subtrees
  to, each module linking
  the library statically; everything outside the virtual subtrees passes
  through untouched, including real values that are not overridden
  (MUI-resolved strings).
- Optional auto-hooking of the process's `Reg*` calls via Detours, playing
  nicely with other hookers of the same functions.
- `.reg` files for persistence and test fixtures.
- Windows only; x86 and x64; runs under Wine/Proton.

## v1 scope

All four parts ship in v1: the core (interface, storage, virtual namespace,
handles), `.reg` import/export, and the Detours adapter. DirectRetrofit
migrating onto it is the acceptance test for the core and the adapter.

## Non-goals (v1)

- ARM64 and ARM64EC.
- Calls made straight to `ntdll` (`NtOpenKey` and friends); they bypass the
  hooks.
- Persistence beyond explicit import/export calls (no file-backed hives, no
  automatic flush).
- Hive *file* parsing (`.dat`, `.hiv`); the native registry is reached only
  through its public API.
- A vcpkg port; it follows v1, with a `detours` feature.

## Architecture

### Targets

- `VReg::Core`: the `Registry` interface, `NativeRegistry`,
  `VirtualRegistry`, the `Hive` interface with `MemoryHive` and
  `OverlayHive`, handle encoding and the shared handle namespace.
- `VReg::RegFile`: `.reg` parse, apply and export; depends on `VReg::Core`.
- `VReg::Detours`: the hook adapter; built only with `VREG_WITH_DETOURS`
  (off by default).

The handle layer lives in `VReg::Core` rather than in a target of its own: every
non-native implementation hands out handles, so none of them exists without
it.

### `Registry`: the Win32-shaped interface

- Virtual methods with the `Reg*ExW` signatures: open, create, close, query
  value, set value, delete value, delete key, delete tree, enumerate keys
  and values, query info, load MUI string, get value, notify, flush.
- The A forms and the pre-`Ex` forms (`RegOpenKeyA`, `RegEnumKeyW`, ...) are
  non-virtual and implemented once over the W virtuals, widening and
  narrowing through `CP_ACP` in both directions (ANSI callers use the system
  code page; UTF-8 there corrupts non-ASCII names and data).
- Modern overloads returning owned values (`std::wstring`,
  `std::vector<std::byte>`) are non-virtual helpers on top. The original
  Win32 signatures stay the primary surface.

Implementations:

- `NativeRegistry`: forwards through a table of function pointers. Defaults
  resolve from the export tables of `KernelBase.dll` first and `advapi32.dll`
  for the pre-`Ex` forms it alone exports, never via `GetProcAddress` (an
  apphelp shim stub would otherwise stand in for the entry point). The
  Detours adapter hands these fields to `DetourAttach`, so after the commit
  they hold the trampolines.
- `VirtualRegistry`: a mount table of hives plus an optional base
  `Registry`. Paths outside every mount go to the base; with no base they
  answer `ERROR_FILE_NOT_FOUND`. It owns the A/W and caller-buffer rules
  (sizes in characters vs bytes, terminators, `ERROR_MORE_DATA`), once, for
  every hive.
  - Overlay over native: base = native, overlay hives mounted where keys
    are added or overridden.
  - Pure in-memory: no base, `HKLM` and `HKCU` each mounted to an empty
    `MemoryHive`.

### `Hive`: storage

A tree with one anonymous root, mounted at a path; several can be mounted at
once, as with `RegLoadKey`. The interface is node-level with owned values;
buffer and A/W rules stay in `VirtualRegistry`.

- `MemoryHive`: starts empty.
- `OverlayHive(upper: MemoryHive, lower: Registry + key path)`: reads the
  upper layer first and falls through to the lower one. The lower layer can
  be native or another `VirtualRegistry`.
  - Value and key **tombstones** hide lower entries.
  - An **opaque** key hides all lower children and values (a key deleted
    and recreated).
  - Enumeration and `QueryInfoKey` merge both layers, stepping over shadowed
    names and shifting lower indices by the upper count; counts include
    both sides. A failed lower query never becomes a smaller successful
    view, and the class comes from the lower call of the caller's width.
  - Only direct children enumerate: a key registered as a whole path is not
    a subkey of its root.
  - An upper-only key ends its enumerations with `ERROR_NO_MORE_ITEMS`,
    never `ERROR_FILE_NOT_FOUND`.
  - `RegLoadMUIStringW` on a lower-backed value forwards to the lower key;
    an upper value is answered as the literal it is (never resolved by us).

## Handles, freeze and locking

### Encoding

Virtual HKEYs are odd values counting down from `0x7FFFFFFF` on both x86 and
x64. Real handles are even; predefined roots lie above the base
(sign-extended to `0xFFFFFFFF8000000x` on x64). `IsVirtualHKey` is "nonzero,
odd, at most the base".

Agent's choice: the range stays 31-bit on x64 so code that truncates an
HKEY to a `DWORD` survives; about 1G handles is far beyond any hive. The
full 63-bit range was passed over.

Measured on Windows 11 26200, x86 and x64 (a probe opening keys through
plain, WOW64, HKCR, HKCU, HKU, HKCC, `RegOpenCurrentUser`,
`RegOpenUserClassesRoot`, `RegConnectRegistry`, transacted and app-hive
routes, plus 2000 repeated opens): bit 0 was never set on a real handle.
Bit 1 is advapi32's tag for merged `HKEY_CLASSES_ROOT` handles; the kernel
ignores both low bits, so `NtQueryKey` resolves tagged handles. Not covered
by the probe: true remote `RegConnectRegistry` (connecting to the own
machine name may short-circuit to local) and Wine; the handle probe becomes
a permanent test, run under Wine in CI.

### One handle per node

Opening a key returns its node's handle; `RegCloseKey` on a virtual handle
is a no-op. A deleted node keeps its slot and calls on it answer
`ERROR_KEY_DELETED`.

Agent's choice: this keeps reads lock-free and handles stable, at the cost
of never reclaiming deleted nodes and of two opens returning the same value.
Per-open refcounted handles (closer to Win32 identity, a lock on every open
and close) were passed over.

### Shared handle namespace across static copies

Each statically linked copy of the library in a process allocates handles
from one namespace:

- A pagefile-backed section `Local\VReg-handles-<pid>` holds
  `{magic, layoutVersion, nextBlock}`. This layout is the only ABI between
  copies and is frozen forever.
- Each copy creates or opens and maps it at init (`CreateFileMapping` is
  atomic open-or-create, the pages are zero-filled, the first copy claims
  `magic` with `InterlockedCompareExchange`). It is never unmapped or
  closed, `DLL_PROCESS_DETACH` included.
- A copy reserves blocks of handle values with one `InterlockedAdd` and
  hands out values from its own blocks; owning a handle means it falls in
  one of the copy's blocks.
- Agent's choice: a section over a named mutex, since the counter needs
  shared storage either way and the section needs no lock; blocks of 4096
  over per-handle increments, for one cross-copy operation per block.
- A same-session process could pre-create the section; it can already open
  this process and do worse, so no defense is added.

### Mount modes

Chosen per mount:

- **Frozen**: the hive is built, then frozen (`Freeze()` on the hive) before
  it is mounted, and is immutable from then on; every read is lock-free. Writes answer `ERROR_ACCESS_DENIED`. Opening a lower-only key
  (nothing in the upper layer at or below it) returns the lower handle
  itself, at no cost on keys the mount does not touch.
- **Writable**: each hive has an SRWLOCK (shared for readers, exclusive for
  writers). Every open inside the mount returns a virtual handle, creating a
  transparent node lazily, or a real handle handed out earlier would miss a
  later copy-up. The lock covers only in-memory node work and is **never
  held across a call into the lower registry** (some native opens load
  DLLs); merged enumeration snapshots the upper names under the lock,
  releases it, then walks the lower layer. No holder can wait on the loader
  lock, so taking it inside a detour body under a foreign loader lock is
  safe.

The mount table is published copy-on-write through one atomic pointer and
old tables are never freed, so a module loaded later can mount a fully built
hive while hooks are live, without readers taking a lock.

### Path resolution

A call arriving on a real parent handle is resolved by native path
(`NtQueryKey(KeyNameInformation)`, with `WOW6432Node` stripped ordinal
case-insensitively); a set of mounted leaf names gates the syscall. Each
mount has a `view` option: `Both` (default; the stripped path matches either
view), `Only64` or `Only32`, decided from `KEY_WOW64_*` in `samDesired` and
the process bitness.

Known gap, documented: a real handle opened before the hooks committed that
points at or inside a mount keeps reaching the lower registry for calls made
on it directly; opens of children through it resolve correctly. Closing it
would cost an `NtQueryKey` on every call on a real handle.

## Detours adapter (`VReg::Detours`)

### Coverage

Every `Reg*` export that accepts an HKEY is hooked, because a virtual handle
reaching an unhooked API fails as an invalid handle. Targets resolve from
the `KernelBase.dll` export table, falling back to `advapi32.dll` for the
pre-`Ex` forms.

- **Emulated**: open and create (Ex and pre-Ex, A and W), close, query value
  (Ex and pre-Ex), set value (Ex and pre-Ex), `RegGetValue` (with `RRF_*`
  filtering and `REG_EXPAND_SZ` expansion), `RegSetKeyValue`, delete key
  (and Ex), delete value, `RegDeleteKeyValue`, `RegDeleteTree`, enumerate
  keys and values (Ex and pre-Ex), `RegQueryInfoKey`,
  `RegQueryMultipleValues`, `RegLoadMUIStringW`, `RegFlushKey` (no-op),
  `RegNotifyChangeKeyValue` (a frozen hive never signals; a writable one
  signals on writes).
- **Refused on virtual handles, passthrough on real ones**: save, restore,
  load, unload, replace, security get/set, rename, copy tree, transacted
  forms, reflection control. They answer `ERROR_NOT_SUPPORTED` on a virtual
  handle.
- `RegLoadMUIStringA` answers `ERROR_CALL_NOT_IMPLEMENTED` natively on every
  input; it is hooked only so a virtual handle gets that same answer instead
  of reaching the real function.

### Install

- `Attach(VirtualRegistry&)` only calls `DetourAttach`, for a consumer that
  owns the transaction (for example one that also carries other hooks and
  thread enrollment).
- `Install(VirtualRegistry&)` runs its own transaction around `Attach`.
- A consumer installing from `DllMain` builds, freezes and mounts its hives
  before the commit, so the first hooked call already finds them.

### Chaining and unloading

- Each copy's base registry is whatever its trampoline reaches: another
  hooker, another copy, or the real function. A copy answers what it owns
  (its mounts, its handles) and passes everything else down; on a path
  mounted by two copies the one on top wins (documented, not detected).
- The library never calls `DetourDetach`. Each hook's detour is a thunk
  allocated outside the module image (`VirtualAlloc`), `jmp [slot]`, with
  `slot` pointing at the detour body. On unload or on a detach request the
  library writes `slot = trampoline` atomically, so the layer becomes a
  pass-through and other hookers' chains stay intact in any order. Thunks,
  slots and trampolines are leaked on purpose.
- Precondition, documented: a thread already inside a detour body when the
  module unmaps still crashes; the consumer must not unload while registry
  calls are in flight.
- README recommendation, not a requirement: install before other registry
  hookers where possible.

### Rules for detour bodies

A detour body can run under a
foreign loader lock, so it never loads a module, creates a thread, or blocks
on a lock a loader-lock holder could own. Calls to the lower registry go
only through the base registry (the trampolines), never `::Reg*` directly,
which would re-enter the hooks and bypass test stubs.

## `.reg` format (`VReg::RegFile`)

### API

- `Parse(text) -> RegScript`: an ordered list of key and value operations.
- `Apply(const RegScript&, Registry&)`.
- `ExportSnapshot(Registry&, key, options)`: the merged view, no deletion
  lines; for fixtures and inspection.
- `ExportDelta(OverlayHive&, mountPath, options)`: the upper layer, with
  tombstones as `[-K]` and `"Name"=-` and opaque keys as `[-K]` followed by
  `[K]`; the persistence format for overlays. Re-importing over the same
  lower registry reproduces the view.
- File helpers on top. Hooks never perform file I/O.

### Import

- Headers: `Windows Registry Editor Version 5.00` as UTF-16LE with BOM, or
  as UTF-8 with or without BOM; `REGEDIT4` in `CP_ACP`.
- Keys: `[path]` creates, `[-path]` deletes with the subtree. Roots by full
  name or as `HKLM`, `HKCU`, `HKCR`, `HKU`, `HKCC`.
- Values: `"name"=` and `@=`; strings with `\\` and `\"` escapes; `dword:`;
  `hex:`; `hex(n):` for any type; `=-` deletes.
- Trailing-backslash continuations, `;` comments, blank lines.
- Through an overlay, deletions become tombstones and `[-K]` then `[K]` an
  opaque key; through any other `Registry`, they are real deletes.
- A tombstone without a lower counterpart is kept, not pruned. Agent's
  choice: the lower registry can gain that key later (another machine, an
  install), and the delta must still hide it; pruning on export was passed
  over.
- Two-phase: the whole file parses first, so a syntax error (reported with
  line and column) writes nothing. Apply stops at the first failing
  operation and reports which one; earlier ones stay applied. Agent's
  choice: no rollback, since native registries have none without KTM; a
  dry-run validation pass was passed over because it cannot catch
  access-denied.

### Export

- Byte-exact round trip: a value takes its friendly form only when that
  reproduces its bytes on re-import, otherwise `hex(n):` (a `REG_SZ` without
  terminator, with an embedded NUL or newline, or an odd byte count).
- Formats: v5 UTF-16LE (default, regedit-compatible); v5 UTF-8 for
  diff-friendly fixtures (whether regedit accepts it is checked in the plan,
  else documented as fixture-only); `REGEDIT4`, where data `CP_ACP` cannot
  represent fails with `ERROR_NO_UNICODE_TRANSLATION`.
- Keys and values sorted ordinal case-insensitive. Agent's choice: stable
  fixture diffs over mirroring regedit's enumeration order.
- Regedit's layout: CRLF, `hex:` wrapped at 80 columns with `\`
  continuations, a blank line between keys.

## Errors and diagnostics

- Every Win32-shaped entry point is `noexcept` and returns the `LSTATUS`
  native Windows returns in the same situation; native behavior is the
  spec. `std::bad_alloc` becomes `ERROR_OUTOFMEMORY` at that boundary.
- Agent's choice: no logging dependency; an optional diagnostic sink (a
  function pointer, off by default) bound by the detour-body rules. A
  logging library dependency was passed over.

## Testing

Catch2.

- **Native oracle**: each scenario runs against `NativeRegistry` on a
  private app hive (`RegLoadAppKey` on a temp file: real, no admin, no
  machine state) and against a `VirtualRegistry` over a `MemoryHive`;
  statuses, sizes and bytes must match. Covers A/W, buffer sizes and
  `ERROR_MORE_DATA`.
- **Ported unit tests**: overlay shadowing, enumeration index shifting,
  `QueryInfoKey` counts and failure cases, ancestor handles, native-path
  resolution, `WOW6432Node`.
- **`.reg`**: byte-exact round trips, deltas with tombstones and opaque keys,
  parse errors with line and column; an opt-in regedit compatibility check
  on HKCU.
- **Hooks**: a separate test process (x86 and x64) installs the hooks and
  calls the real `Reg*` APIs; it loads two DLLs that each link the library
  statically, covering the shared handle namespace, chaining and
  neutralize-on-unload.
- **Handle probe**: the parity measurement above as a permanent test.

### CI (GitHub Actions, public repository)

- Windows: {x86, x64} x {MSVC, clang-cl}, all tests.
- Wine on Ubuntu: the Windows-built x86 and x64 test binaries, all tests
  except the native oracle, which is skipped there. Agent's choice: native
  Windows behavior is the spec and Wine may deviate on edge-case statuses; a
  per-platform expectation list was passed over because it would make our
  behavior follow whichever Wine version CI installed.
- Proton and true remote `RegConnectRegistry` stay manual checks; the
  original implementation already runs under Wine and Proton.

## Packaging

- CMake 3.25+, C++20, MSVC 2022 and clang-cl.
- Works through `add_subdirectory`/`FetchContent` (tests build only when
  VReg is the top-level project) and through
  `find_package(VReg CONFIG COMPONENTS Core RegFile Detours)` after install.
- Static by default; `BUILD_SHARED_LIBS` is honored.
- Detours is found as the vcpkg port lays it out (header plus library, no
  CMake config).

## License

MIT (the maintainer's default; Detours is MIT as well). Alternatives:

- BSL-1.0: no attribution required in shipped binaries, convenient for a
  library embedded in redistributed DLLs.
- Apache-2.0: explicit patent grant, NOTICE file.
- 0BSD or MIT-0: no conditions at all.

## Naming

Spelled VReg wherever no convention forces lowercase: C++ namespace `VReg`,
CMake package `VReg` with targets `VReg::Core`, `VReg::RegFile` and
`VReg::Detours`. Lowercase only for the vcpkg port (`vreg`); CMake options
are `VREG_*`. Flat functions, where any exist, are `VReg` plus the Win32 name (`VRegOpenKeyExW`), each
reading as the virtual counterpart of its `Reg*` function.

