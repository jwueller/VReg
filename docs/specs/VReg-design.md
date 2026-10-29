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

### `Registry`: the `Reg*` function family

- Virtual methods with the `Reg*ExW` signatures: open, create, close, query
  value, set value, delete value, delete key, delete tree, enumerate keys
  and values, query info, load MUI string, get value, notify, flush.
- The ANSI forms and the pre-`Ex` forms (`RegOpenKeyA`, `RegEnumKeyW`, ...) are
  implemented once over the wide virtuals, widening and narrowing through
  `CP_ACP` in both directions (ANSI callers use the system code page; UTF-8
  there corrupts non-ASCII names and data).
- `RegSetKeyValue`, `RegDeleteKeyValue` and `RegQueryMultipleValues`
  (ANSI and wide) are built from the virtuals as Win32 builds them; they are
  hooked, so every `Registry` must answer them.
- Every ANSI form is virtual, with that implementation as its default, so a
  wrapper in front of a registry (one that traces each call) can tell an
  ANSI call from a wide call; it forwards an ANSI form it overrides to the
  ANSI form of the registry it wraps. The wide pre-`Ex` forms and
  conveniences stay non-virtual.
- Members carry their Win32 names, `Reg` prefix included (`RegOpenKeyExW`),
  so the `UNICODE` selector macros of `winreg.h` (`RegOpenKeyEx`) pick the
  ANSI or wide member as they pick the function. Only ANSI and wide names are
  declared: a member with the neutral name would be renamed by the macro.
- Queries are `const` members: they change nothing the registry answers
  afterwards. Opening a key is not a query, since it hands out a handle the
  caller owns; neither is pre-`Ex` `RegQueryValue`, which opens and closes
  a subkey.
- `Registry` holds the `Reg*` function family and nothing else. The reads
  that size and fill a `std::wstring` or `std::vector<std::byte>` themselves
  are free functions over a `const Registry&` (`owned_values.h`), without the
  `Reg` prefix, which would collide with Win32 macros such as
  `RegQueryValue`.

Implementations:

- `NativeRegistry`: forwards through a table of function pointers. Defaults
  resolve from the export tables of `KernelBase.dll` first and `advapi32.dll`
  for the pre-`Ex` forms it alone exports, never via `GetProcAddress` (an
  apphelp shim stub would otherwise stand in for the entry point). The
  Detours adapter hands these fields to `DetourAttach`, so after the commit
  they hold the trampolines.
- `VirtualRegistry`: a mount table of hives plus an optional base
  `Registry`. Paths outside every mount go to the base; with no base they
  answer `ERROR_FILE_NOT_FOUND`. It owns the ANSI/wide and caller-buffer rules
  (sizes in characters vs bytes, terminators, `ERROR_MORE_DATA`), once, for
  every hive.
  - Overlay over native: base = native, overlay hives mounted where keys
    are added or overridden.
  - Pure in-memory: no base, `HKLM` and `HKCU` each mounted to an empty
    `MemoryHive`.

### `Hive`: storage

A tree with one anonymous root, mounted at a path; several can be mounted at
once, as with `RegLoadKey`. The interface is node-level with owned values;
buffer and ANSI/wide rules stay in `VirtualRegistry`.

- `MemoryHive`: starts empty.
- `OverlayHive(lower: Registry&, lower root: HKEY, lower path)`: reads its
  own upper tree first and falls through to the lower key. The upper tree is
  the overlay's own node storage (not a separate `MemoryHive` object), and
  the lower layer can be native or another `VirtualRegistry`. A frozen
  overlay answers an untouched key with a handle of its lower registry, and
  the `VirtualRegistry` it is mounted in sends later calls on that handle to
  its base, so the lower registry must be that base or answer the same
  handles.
  - Value and key **tombstones** hide lower entries.
  - An **opaque** key hides all lower children and values (a key deleted
    and recreated).
  - Enumeration and `QueryInfoKey` merge both layers, stepping over shadowed
    names and shifting lower indices by the upper count; counts include
    both sides. A failed lower query never becomes a smaller successful
    view, and the class of a key the upper layer did not create comes from
    the lower key, also after the key was written to.
  - Only direct children enumerate: a key registered as a whole path is not
    a subkey of its root.
  - An upper-only key ends its enumerations with `ERROR_NO_MORE_ITEMS`,
    never `ERROR_FILE_NOT_FOUND`.
  - `RegLoadMUIStringW` on a lower-backed value forwards to the lower key;
    an upper value answers as any hive value does (see "Known divergences of
    virtual keys").

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
  `{magic, nextBlock}`. This layout is the only ABI between copies and is
  frozen forever. The magic value carries the layout version, so claiming
  the section is a single compare-exchange with nothing published after it;
  a copy that finds a different nonzero magic treats the namespace as
  unusable.
- Each copy creates or opens and maps it on first use (`CreateFileMapping`
  is atomic open-or-create and needs no loader lock, the pages are
  zero-filled, the first copy claims `magic` with
  `InterlockedCompareExchange`). It is never unmapped or closed,
  `DLL_PROCESS_DETACH` included.
- A copy reserves blocks of handle values with one `InterlockedIncrement` and
  hands out values from its own blocks; owning a handle means it falls in
  one of the copy's blocks.
- A virtual handle from another copy's block is foreign: every entry point
  treats it exactly like a real handle and passes it to its base registry
  (in a hooked process, the trampoline: the next hooker, another copy or the
  real function), so it reaches the copy that issued it.
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

Independently of both, a `VirtualRegistry` mounts a hive passed as
`const Hive&` read-only: it calls only the hive's const members, and every
write through it answers `ERROR_ACCESS_DENIED` as on a frozen hive, while
whoever holds the hive non-const can still write it (and the mount sees
those writes, under the lock). Handles belong to nodes, not mounts, so one
registry mounts a hive either read-only or writable, never both.

### Change notification

`RegNotifyChangeKeyValue` on a virtual key registers a one-shot watch on the
key's node, kept in the hive's tree (so every hive kind has it).

- A frozen hive accepts the call and never signals; it has no writes to
  watch. Freezing drops the watches already registered without signaling.
- A writable hive signals the event of every watch that a write matches, and
  drops it (a registration fires once). `REG_NOTIFY_CHANGE_NAME` matches a
  subkey added or deleted on the watched key, `REG_NOTIFY_CHANGE_LAST_SET`
  a value set or deleted on it; `ATTRIBUTES` and `SECURITY` never match. A
  watch with `bWatchSubtree` also matches the same changes on any descendant.
  Deleting a watched key (or an ancestor) signals its watches whatever their
  filter. Emptying a key with `RegDeleteTree` counts as both changes.
- The hive duplicates the event handle at registration, so the caller may
  close its own. The event is signaled with `SetEvent` while the hive's write
  lock is held: neither it nor `CloseHandle` can wait on the loader lock or
  on a lock a hive caller could hold, and deferring them would need an
  allocation after the write was made.
- Through an overlay only writes made through the hive are seen. A change in
  the lower registry never signals, and opening a lower key (which creates a
  transparent node) is not a change.
- Agent's choice: a synchronous call (`fAsynchronous` false) waits for the
  change on a private event, like the native call, so a watcher thread written
  for the native registry keeps working; it never returns on a key nothing
  writes to, as natively. Passed over: answering at once, which would spin
  such a loop, and `ERROR_NOT_SUPPORTED`, which would end it. The wait is on
  an event that only a writer sets, never on a lock.
- A registration that never fires keeps its duplicate handle until the hive
  is destroyed or frozen; registering again on the same key and event without
  waiting adds another.

The mount table is published copy-on-write through one atomic pointer and
old tables are never freed, so a module loaded later can mount a fully built
hive while hooks are live, without readers taking a lock.

### Path resolution

A call lands in a mount through a virtual handle of its hive, through a
predefined root plus a path at or below the mount point, or, when the
registry has a base, through a real parent handle whose subkey path leads
into a mount. The last case is resolved by the parent's native path:

- The gate: a call is looked at only when it has a subkey path and some
  component of it equals (ordinal, case-insensitive) the last component of
  some mount point. Any other call goes to the base without a syscall, so
  real keys the mounts do not touch cost nothing. A mount at a root itself
  has no such component and is not reachable this way.
- The path comes from `NtQueryKey(KeyNameInformation)` through a function
  pointer resolved from `ntdll`'s export table when the registry is built
  (never `GetProcAddress`, and no module load in a call); a constructor
  parameter replaces it for tests. A failing query sends the call to the
  base. A virtual handle of another copy of the library is never queried.
- Native root names: `HKEY_LOCAL_MACHINE` is `\REGISTRY\MACHINE`,
  `HKEY_USERS` is `\REGISTRY\USER`, `HKEY_CURRENT_USER` is
  `\REGISTRY\USER\<SID string of the process token>`, computed when the
  mount is added. A mount below any other predefined root has no native path
  and is reached only through that root.
- `WOW6432Node` components are skipped, whole and ordinal
  case-insensitively, while the path is matched against the mount point.
  Agent's choice: below the mount point the name is an ordinary key, since a
  hive's own tree is not a registry view; stripping the whole path was passed
  over. The longest matching mount wins.
- Each mount has a `view`: `Both` (default), `Only64` or `Only32`. A call is
  of the view `KEY_WOW64_64KEY` or `KEY_WOW64_32KEY` in its `samDesired`
  names (`RRF_SUBKEY_WOW64*KEY` for `RegGetValue`), else of the process
  bitness; a native path that contained `WOW6432Node` is always the 32-bit
  view. A mount answers only calls of its view, through predefined roots as
  well.

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

- **Emulated**: open and create (Ex and pre-Ex, ANSI and wide), close, query
  value (Ex and pre-Ex), set value (Ex and pre-Ex), `RegGetValue` (with `RRF_*`
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

- `Attach(Registry&, NativeRegistry&)` only calls `DetourAttach`, for a
  consumer that owns the transaction (for example one that also carries
  other hooks and thread enrollment). Every hook calls through the
  `Registry`, normally a `VirtualRegistry`, or a wrapper in front of one
  (one that traces each call, say). The `NativeRegistry` is the registry
  below the hooks: its function table holds the targets handed to Detours,
  so after the commit it holds the trampolines. A second call answers
  `ERROR_ALREADY_INITIALIZED`.
- `Install(Registry&, NativeRegistry&)` runs its own transaction around
  `Attach`, enrolling the calling thread.
- `Neutralize()` makes every hook of the copy pass straight through; it also
  runs by itself when the module unloads, through a static destructor.
- A consumer installing from `DllMain` builds, freezes and mounts its hives
  before the commit, so the first hooked call already finds them.

### Chaining and unloading

- Each copy's base registry is whatever its trampoline reaches: another
  hooker, another copy, or the real function. A copy answers what it owns
  (its mounts, its handles) and passes everything else down; on a path
  mounted by two copies the one on top wins (documented, not detected).
- The library never calls `DetourDetach`. Each hook's detour is a thunk
  allocated outside the module image (`VirtualAlloc`), `jmp [slot]`, with
  `slot` pointing at the detour body. On unload or on a `Neutralize` call the
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

In namespace `VReg::RegFile` (`include/VReg/regfile.h`):

- `Parse(file bytes) -> Script`: an ordered list of key and value
  operations; a syntax error reports line and column.
- `Apply(const Script&, Registry&)`: reports the index of the failing
  operation.
- `ExportSnapshot(Registry&, root, path, format)`: the merged view, no
  deletion lines; for fixtures and inspection.
- `ExportDelta(OverlayHive&, root, path, format)`: the upper layer, with
  tombstones as `[-K]` and `"Name"=-` and opaque keys as `[-K]` followed by
  `[K]`; the persistence format for overlays. Re-importing over the same
  lower registry reproduces the view. `root` and `path` name where the hive
  is mounted; key names are that prefix plus the key's path in the hive. The
  walk is `OverlayHive::SnapshotUpper`, which reads the upper tree under the
  hive's read lock and never calls the lower registry. A key that only shows
  a lower key through and holds no value of its own is omitted, so opening
  keys does not change the delta; one with upper values or value tombstones
  appears as `[K]` (it exists below, so creating it is harmless).
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
- Formats: v5 UTF-16LE (default, what regedit writes); v5 UTF-8 without
  BOM for diff-friendly fixtures; `REGEDIT4`, where data `CP_ACP` cannot
  represent fails with `ERROR_NO_UNICODE_TRANSLATION`. Windows imports the
  UTF-8 form intact, non-BMP characters included: `regedit /s` with or
  without BOM, `reg import` only without one (with a BOM it answers "not a
  registry file").
- Keys and values sorted ordinal case-insensitive. Agent's choice: stable
  fixture diffs over mirroring regedit's enumeration order.
- `reg export`'s layout, byte for byte apart from the order: CRLF, hex data
  broken after the first comma past column 76 with a `\` continuation and
  a two-space indent, a blank line between keys.

## Errors and diagnostics

- Every Win32-shaped entry point is `noexcept` and returns the `LSTATUS`
  native Windows returns in the same situation; native behavior is the
  spec. `std::bad_alloc` becomes `ERROR_OUTOFMEMORY` at that boundary.
- Agent's choice: no logging dependency; an optional diagnostic sink (a
  function pointer, off by default) bound by the detour-body rules. A
  logging library dependency was passed over.

### Known divergences of the ANSI adapters

Measured by the oracle; each is an input no sane caller relies on.

- Strings set without a terminator: native `RegSetValueExW` and
  `RegSetValueExA` read one character past `cbData` and store an extra
  terminator when it is zero. Ours never reads past `cbData`: the wide form
  stores exactly the bytes given, and the ANSI form always terminates.
- `RegGetValueA` on a string with an odd byte count: native keeps the stray
  byte in the buffer tail.
- `RegQueryMultipleValuesA` with a buffer that holds some narrowed values
  but not all: native answers `ERROR_INVALID_DATA` after a partial fill; ours
  answers `ERROR_MORE_DATA`.
- `RegEnumValueA` with a too-small name buffer: native may write a
  terminator past `cbData`; ours stays in bounds.
- Null pointers on which native faults (`RegEnumKeyExA` and `RegEnumValueA`
  names, `RegQueryMultipleValues` lists and buffers) answer an error code.

### Known divergences of virtual keys

Measured by running the oracle catalogue against `VirtualRegistry` over a
`MemoryHive`; the scenarios skip exactly these calls for the virtual run
(and for its native reference).

- Access rights are per node, not per open: a key opened `KEY_READ` can
  still be written through its handle, and `KEY_QUERY_VALUE` is never
  checked. (A zero `samDesired` is refused on open, as native does.)
- Handle identity: two opens of a node return the same handle, and an `Ex`
  open or create of an empty subkey returns the parent's own handle where
  native returns a new one.
- `REG_OPTION_BACKUP_RESTORE` opens succeed; native needs
  `SeBackupPrivilege` and answers `ERROR_ACCESS_DENIED` without it.
- `REG_OPTION_CREATE_LINK` creates an ordinary key.
- `RegQueryInfoKey` reports a security descriptor size of 0.
- `RegLoadMUIString` answers `ERROR_INVALID_DATA` for a hive value whether
  or not it is an `@` indirect string; resolving one would load a resource
  module, which detour bodies may not do. A value stored with
  `Hive::SetMuiTextValue` instead stands for the text an indirect string
  resolves to: it answers with that text, sized and truncated as native
  answers a resolved string, and ignores `pszDirectory` (there is no module
  to find). Agent's choice: an explicit mark per value, set through the
  hive. A value type of our own would show through `RegQueryValueEx`, and
  answering every plain string with its text would change what unmarked
  values answer, which consumers rely on.
- Through an overlay, an entry the upper layer shadows or tombstones carries
  the letter case of the first call that named it, not the lower key's case
  (a name is found without regard to case, but enumerated as stored).
- Only calls on a real handle that name a mount point's last component are
  resolved by native path; a mount below `HKEY_CLASSES_ROOT` and the other
  roots without a native path is not reachable through a real parent handle.

## Testing

Catch2.

- **Native oracle**: each scenario runs against `NativeRegistry` on a
  private app hive (`RegLoadAppKey` on a temp file: real, no admin, no
  machine state) and against a `VirtualRegistry` over a `MemoryHive`;
  statuses, sizes and bytes must match. Covers ANSI/wide, buffer sizes and
  `ERROR_MORE_DATA`.
- **Ported unit tests**: overlay shadowing, enumeration index shifting,
  `QueryInfoKey` counts and failure cases, ancestor handles, native-path
  resolution, `WOW6432Node`.
- **Concurrency**: reader and writer threads (opens, creates, value writes
  and deletes, key deletes, enumerations, `QueryInfoKey`, watches, snapshot
  exports) on a writable `MemoryHive`, one frozen halfway through, and a
  writable `OverlayHive` over an app hive, each for about two seconds. They
  require only plausible status codes, no hang, and a final state that
  agrees with itself.
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
  except those on a private app hive (the native oracle and every test
  against the real registry), which skip themselves there. Agent's choice: native
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
- Static libraries only. Agent's choice: each module links its own copy and
  the copies meet in the shared handle namespace, so a DLL build would buy
  nothing and need an export surface; honoring `BUILD_SHARED_LIBS` was
  passed over.
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

