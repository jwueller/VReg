# VReg architecture

How VReg is built, for contributors. What it does and how to use it is in
the [README](./README.md), how to build, test and release it in
[CONTRIBUTING.md](./CONTRIBUTING.md).

## Components

### Targets

The whole library is one hand-maintained file, `vreg.h`, in the stb style:
public declarations, the hooks' declarations (behind `VREG_WITH_DETOURS`),
private declarations, the implementation (behind `VREG_IMPLEMENTATION`) and
the hooks' implementation, each part split into titled sections. The CMake
targets compile it from one-line files under `src/`:

- `VReg::VReg`: the registry function table and its flat functions, the
  native and virtual registries, hives, handle encoding and the shared
  handle namespace, and `.reg` parse, apply, snapshot, delta and write
  (`src/vreg.c`).
- `VReg::Detours`: the hook adapter (`src/detours/hooks.c`); built only with
  `VREG_WITH_DETOURS` (off by default), since it needs Microsoft Detours. It
  compiles the hooks' part alone (`VREG_INTERNAL_HOOKS_IMPLEMENTATION`) over the
  private declarations, and links `VReg::VReg` for the rest.

The private declarations show under `VREG_INTERNAL` without the
implementation, for the tests, which reach into hives, the handle space and
the export table. It is one file rather than a tree of sources merged into
a generated header, so what a consumer copies is what contributors edit;
the cost is a long file, navigated by its section banners.

One library target for the core, not one per area: nothing uses `.reg`
support without the core, and a static library costs a consumer only the
objects it links. The handle layer lives in the
core: every non-native implementation hands out handles, so none of them
exists without it.

The library is C99 with `windows.h` as its one dependency: it allocates
through `VREG_MALLOC`, `VREG_REALLOC` and `VREG_FREE` (the process heap by
default), and reaches no C runtime function (the hooks' `atexit` excepted).
Every function is declared `VREG_DECL`, which `VREG_STATIC` turns into `static`.

### `VReg`: the `Reg*` function family

- A registry is an opaque handle, as a hive and a script are, so nothing
  outside the implementation depends on its layout. Inside it is one tagged
  struct: its kind and a union with what the kind needs. The native and virtual registries are kinds of their own, so the
  flat functions call them directly and neither needs a function table, a
  struct of its own or a cast.
- Any other implementation is a custom registry: `VReg_CreateCustom` makes
  one over a `VRegVtbl` and a context, the implementation's own object,
  which its slots reach through `VReg_GetContext`. A wrapper's context
  keeps the registry it wraps. The shape is `fopencookie`'s: a handle made
  from a function table and a context pointer. The slots take the
  registry rather than the context (as `fopencookie`'s callbacks do) so
  that they share the flat functions' typedefs; taking the context would
  need a second typedef per form. Agent's choice: the name
  `VReg_CreateCustom`, over a bare `VRegCreate` that reads like
  `VReg_CreateKey`.
- Each form of the family has one function typedef
  (`VRegOpenKeyExWFunction`) that carries its signature, annotations and
  documentation link. The flat function is declared with it, the slot
  points to it, and VReg's own implementations are declared with it, so
  only definitions spell a signature out again (`/analyze` checks a body
  against the annotations of its own definition).
- The table has one `WINAPI` slot per form of the family, 45 in all, with
  the object first, plus `Destroy`: the wide `Ex` forms (open, create,
  close, query value, set value, delete value, delete key, delete tree,
  enumerate keys and values, query info, load MUI string, get value, notify,
  flush), the ANSI forms, the pre-`Ex` forms (`RegOpenKeyA`,
  `RegEnumKeyW`, ...) and the conveniences Win32 builds from other calls
  (`RegSetKeyValue`, `RegDeleteKeyValue`, `RegQueryMultipleValues`).
- Callers use the flat functions, `VReg` plus the Win32 name with the
  registry first (`VReg_OpenKeyExW(pRegistry, hKey, ...)`). A flat wide
  function switches on the kind. Every other flat function calls its slot
  when a custom table fills it; otherwise it derives the call from
  the wide `Ex` forms, once, in the "Derived forms" section: ANSI forms widen and
  narrow through `CP_ACP` in both directions (ANSI callers use the system
  code page; UTF-8 there corrupts non-ASCII names and data), and the
  conveniences are built as Win32 builds them. A custom implementation fills
  the wide surface; a flat wide function whose slot is null answers
  `ERROR_CALL_NOT_IMPLEMENTED`.
- A wrapper in front of a registry (one that traces each call) can tell an
  ANSI call from a wide call by filling the ANSI slot; one that leaves it
  null sees the call as the wide form it becomes.
- Slots and flat functions carry their Win32 names, `Reg` prefix included,
  and the `UNICODE` selector macros (`VReg_OpenKeyEx`) pick the ANSI or wide
  function as `winreg.h` picks the Win32 one. Only ANSI and wide names are
  declared: a slot with the neutral name would be renamed by the macro.
- The table holds the `Reg*` function family and nothing else. The reads
  that size and fill an owned buffer themselves are the tests'
  (`test/owned_values.h`), without the `Reg` prefix, which would collide
  with Win32 macros such as `RegQueryValue`.

Implementations:

- The native registry (`VReg_CreateNative`): the flat wide functions call
  its `VRegNativeFunctions` table, one field per wide form, and every other
  form is derived. The table resolves from the export table of
  `KernelBase.dll`, never via `GetProcAddress` (an apphelp shim stub would
  otherwise stand in for the entry point). The Detours adapter hands these
  fields to `DetourAttach`, so after the commit they hold the trampolines;
  the trampolines of the ANSI and pre-`Ex` hooks, which no registry calls,
  stay in the adapter.
- The virtual registry (`VReg_CreateVirtual`, `VReg_Mount`): a mount
  table of hives plus an optional base registry. Paths outside every mount
  go to the base; with no base they answer `ERROR_FILE_NOT_FOUND`. It owns
  the ANSI/wide and caller-buffer rules (sizes in characters vs bytes,
  terminators, `ERROR_MORE_DATA`), once, for every hive.
  - Overlay over native: base = native, overlay hives mounted where keys
    are added or overridden.
  - Pure in-memory: no base, `HKLM` and `HKCU` each mounted to an empty
    hive.

### `VRegHive`: storage

A tree with one anonymous root, mounted at a path; several can be mounted at
once, as with `RegLoadKey`. The virtual registry reaches a hive only through
the node-level interface of the private "Hive internals" section
(`VReg_Hive*`), which answers owned values, while buffer and ANSI/wide rules
stay in the virtual registry.

One struct covers every kind of hive. What varies is the lower behind the
node tree, which the hive reaches only through a `VRegHiveVtbl` and its
context, as `fopencookie` makes a `FILE` from functions and a cookie:

- `VRegHive_Create`: nothing below; in memory, starts empty. The flags that
  describe a node's relation to the lower layer stay clear in it.
- `VRegHive_CreateCustom(table, context)`: the caller's table, the second
  extension point beside `VRegVtbl`: data, not interception. Its writes go
  through to the slots.
- `VRegHive_CreateLayered(lower, lower root, lower path, view)`: a registry
  subtree, through an internal implementation of the table ("Hive lowers
  over a registry" in `vreg.h`) whose context holds the lower registry,
  root, path and view. It reads its
  own upper tree first and falls through to the lower key. The lower layer
  can be native or another virtual registry. A frozen layered hive answers
  an untouched key with a handle of its lower registry, and the virtual
  registry it is mounted in sends later calls on that handle to its base,
  so the lower registry must be that base or answer the same handles.
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

The nodes are the hive's records of its keys:

- A custom hive's nodes hold no values: each is the record of a key the
  provider answered, published as the opaque `VRegHiveKey`, keeping the
  token its `OpenKey` or `CreateKey` returned. Slots read it through
  `VRegHiveKey_GetToken`, `VRegHiveKey_GetName` and `VRegHiveKey_GetParent`,
  allocation-free and valid for the hive's life. Agent's choice: name and
  parent instead of a joined path, which would need storage the caller
  frees or a buffer rule; a provider that wants the path walks up and joins
  it.
- Identity is the path, compared with `VRegInternal_CompareNames` as for every
  node, so a virtual handle maps to a record as to any node: one handle per
  record, and a record whose key was deleted through VReg answers
  `ERROR_KEY_DELETED` for good. A provider that answers the same token for
  two paths gets two records; nothing compares tokens. Records live until
  the hive is destroyed.
- Records are added lazily under the hive's write lock, never while a slot
  runs: look up, unlock, call the slot, lock again and insert, finding what
  another thread inserted meanwhile. A custom hive opens and creates one
  name per slot call, so every record holds the token of its own key; the
  layered hive opens the whole rest of a path in one lower call.
- Writes have a mode per hive, which its constructor sets: copy-up (in
  memory and layered) keeps them in the hive's own layer; write-through
  (custom) sends them to the slots and then updates the records: a created
  key gets a record with its token, a deleted one is marked deleted, and
  the watches fire as for any write. A null write slot answers
  `ERROR_ACCESS_DENIED`; without `CreateKey` an existing key still opens,
  as on a frozen hive. A null `QueryInfo` is derived by enumerating keys and
  values once; a null `CheckKey` succeeds.
- The access mask of an open and the frozen hand-out never reach a
  provider. The layered hive opens its lower through a private variant of
  `OpenKey` that takes the access and can hand the opened key out as a
  redirect ("this key is answered by registry R at key K"); a frozen
  layered hive answers an untouched key that way, and the virtual registry
  forwards a call on a redirect through one helper that closes the key
  after. The redirect stays private: its constraint, that later calls on
  the handle go to the mounting registry's base, is a sharp edge to
  publish.
- Slots hand results over through the `VRegHiveResult_Set*` sinks, which
  copy into storage the hive points them at; the internal implementation
  fills that storage directly.
- VReg's invariants bind the provider's code too, and the public header
  states them as requirements: slots run on any thread, concurrently and
  under a foreign loader lock; VReg holds no lock of its own across a slot
  call; names arrive unchanged and compare as `VReg_CompareNames` does;
  `DeleteKey` on a key with subkeys answers `ERROR_ACCESS_DENIED` without
  VReg checking first; the access mask is never passed.
- The upper-layer snapshot that `VRegScript_Delta` reads belongs to the
  built-in tree.
- Slots take the hive, as `VRegVtbl` slots take the registry, and reach
  their object through `VRegHive_GetContext`, so both tables read alike and
  a slot holds what `VRegHive_NotifyChange` needs.
- The public pair mirrors the registry's: `VRegHive_CreateCustom(table,
  context, &hive)` and `VRegHive_GetContext(hive)`, freed with
  `VRegHive_Destroy`, over a `VRegHiveVtbl`. A custom hive is a `VRegHive`
  like any other: a separate opaque type would split every mount and
  script function in two.
- Freezing a custom hive refuses writes through VReg and changes nothing
  else: its records are still discovered lazily, so opens keep taking the
  lock, and it keeps its watches.
- A custom hive's delta is empty, which is what the hive holds itself: its
  records are all transparent, so the snapshot of the upper layer has
  nothing to write.
- The layered hive's lower reopens its key by path on every call and keeps
  no handle per record (the comment at "Hive lowers over a registry" in
  `vreg.h`): the reopen is transparent to the caller, and a handle per
  record would stay pinned for the hive's life.

Agent's choices, open until the maintainer confirms them:

- `DeleteTree` is never derived from the other slots: a partial failure
  would leave a half-deleted tree that native never shows. Passed over:
  deleting subkeys one by one through `DeleteKey`.
- Results go through copying sinks, so a computed value can live on the
  provider's stack and nothing it hands over needs a lifetime rule. Passed
  over: caller buffers with `ERROR_MORE_DATA`, which bring back the buffer
  rules the table exists to hide, and returned pointers valid until the
  next call, which every concurrent caller would have to reason about.

## Handles, freeze and locking

### Encoding

Virtual HKEYs are odd values counting down from `0x7FFFFFFF` on both x86 and
x64. Real handles are even; predefined roots lie above the base
(sign-extended to `0xFFFFFFFF8000000x` on x64). `VReg_IsVirtualHKey` is "nonzero,
odd, at most the base"; the private `VRegInternal_IsPredefinedHKey` is the range from
`HKEY_CLASSES_ROOT` to `HKEY_CURRENT_USER_LOCAL_SETTINGS`, the one test for a
root everywhere in the library.

The range stays 31-bit on x64 so code that truncates an HKEY to a `DWORD`
survives; about 1G handles is far beyond any hive.

Measured on Windows 11 26200, x86 and x64 (a probe opening keys through
plain, WOW64, HKCR, HKCU, HKU, HKCC, `RegOpenCurrentUser`,
`RegOpenUserClassesRoot`, `RegConnectRegistry`, transacted and app-hive
routes, plus 2000 repeated opens): bit 0 was never set on a real handle.
Bit 1 is advapi32's tag for merged `HKEY_CLASSES_ROOT` handles; the kernel
ignores both low bits, so `NtQueryKey` resolves tagged handles. An odd
handle passed to the real API (a virtual handle leaked past a neutralized
hook) answers `ERROR_INVALID_HANDLE` from every function, `RegCloseKey`
included, also under the strict-handle-check mitigation: kernelbase rejects
it before the kernel, so it never aliases the real handle it would name with
bit 0 dropped. Not covered
by the probe: true remote `RegConnectRegistry` (connecting to the own
machine name may short-circuit to local) and Wine; the handle probe becomes
a permanent test, run under Wine in CI.

### One handle per node

Opening a key returns its node's handle; `RegCloseKey` on a virtual handle
is a no-op. A deleted node keeps its slot and calls on it answer
`ERROR_KEY_DELETED`.

This keeps reads lock-free and handles stable, at the cost of never
reclaiming deleted nodes and of two opens returning the same value.
Per-open refcounted handles would be closer to Win32 identity but need a
lock on every open and close.

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
- A section rather than a named mutex, since the counter needs shared
  storage either way and the section needs no lock; blocks of 4096 rather
  than per-handle increments, for one cross-copy operation per block.
- A same-session process could pre-create the section; it can already open
  this process and do worse, so no defense is added.

### Mount modes

Chosen per mount:

- **Frozen**: the hive is built, then frozen (`Freeze()` on the hive) before
  it is mounted, and is immutable from then on; every read of a built-in
  hive is lock-free (a custom hive keeps locking to add its records).
  Writes answer `ERROR_ACCESS_DENIED`. Opening a lower-only key (nothing in
  the upper layer at or below it) returns the lower handle itself, at no
  cost on keys the mount does not touch.
- **Writable**: each hive has an SRWLOCK (shared for readers, exclusive for
  writers). Every open inside the mount returns a virtual handle, creating a
  transparent node lazily, or a real handle handed out earlier would miss a
  later copy-up. The lock covers only in-memory node work and is **never
  held across a call into the lower registry** (some native opens load
  DLLs); merged enumeration snapshots the upper names under the lock,
  releases it, then walks the lower layer. No holder can wait on the loader
  lock, so taking it inside a detour body under a foreign loader lock is
  safe.

### Change notification

`RegNotifyChangeKeyValue` on a virtual key registers a one-shot watch on the
key's node, kept in the hive's tree (so every hive kind has it).

- A frozen built-in hive accepts the call and never signals; it has no
  writes to watch. Freezing it drops the watches already registered without
  signaling. A frozen custom hive keeps its watches and registers new ones,
  since its provider still signals its own changes.
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
- A custom hive's provider reports a change it made on its own with
  `VRegHive_NotifyChange(hive, key, filter)`, which fires the watches as a
  write through the hive would, under the same lock; it must not be called
  from inside a slot. A key the provider deletes on its own is reported to
  its parent and is not marked deleted.
- A synchronous call (`fAsynchronous` false) waits for the change on a
  private event, like the native call, so a watcher thread written for the
  native registry keeps working; it never returns on a key nothing writes
  to, as natively. Answering at once would spin such a loop, and
  `ERROR_NOT_SUPPORTED` would end it. The wait is on an event that only a
  writer sets, never on a lock.
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
  (never `GetProcAddress`, and no module load in a call); the tests
  replace it in the registry's private field. A failing query sends the
  call to the base. A virtual handle of another copy of the library is
  never queried.
- Native root names: `HKEY_LOCAL_MACHINE` is `\REGISTRY\MACHINE`,
  `HKEY_USERS` is `\REGISTRY\USER`, `HKEY_CURRENT_USER` is
  `\REGISTRY\USER\<SID string of the process token>`, computed when the
  mount is added. A mount below any other predefined root has no native path
  and is reached only through that root.
- `WOW6432Node` components are skipped, whole and ordinal
  case-insensitively, while the path is matched against the mount point.
  Below the mount point the name is an ordinary key, since a hive's own tree
  is not a registry view. The longest matching mount wins.
- Each mount has a view, in the `REGSAM` vocabulary `VRegHive_CreateLayered`
  uses for its lower view too: `KEY_WOW64_64KEY`,
  `KEY_WOW64_32KEY`, or 0 (the default) for both. A call is of the view its
  `samDesired` names (`RRF_SUBKEY_WOW64*KEY` for `RegGetValue`), else of the
  process bitness; a native path that contained `WOW6432Node` is always the
  32-bit view. A mount answers only calls of its view, through predefined
  roots as well.

Known gap, documented: a real handle opened before the hooks committed that
points at or inside a mount keeps reaching the lower registry for calls made
on it directly; opens of children through it resolve correctly. Closing it
would cost an `NtQueryKey` on every call on a real handle.

## Detours adapter (`VReg::Detours`)

### Coverage

Every `Reg*` export that accepts an HKEY is hooked, because a virtual handle
reaching an unhooked API fails as an invalid handle. Targets resolve from
the `KernelBase.dll` export table, falling back to `advapi32.dll` for the
pre-`Ex` forms it alone exports. KernelBase holds the bodies: advapi32's `Ex` exports only
forward to them, and callers that import the API set
(`api-ms-win-core-registry-l1-1-0`, as system DLLs and `onecore.lib` builds
do) reach KernelBase without passing through advapi32, so only a hook on
the body sees every caller. That KernelBase hosts the API set is not
documented; `test/test_native_registry.cpp` checks the forwarding against
the running system. The README's
[Supported functions](./README.md#supported-functions) lists every function
of each group.

- **Emulated**: the slots of `VRegVtbl`, answered through the
  registry.
  `RegGetValue` applies the `RRF_*` filters and `REG_EXPAND_SZ` expansion,
  `RegFlushKey` is a no-op, and `RegNotifyChangeKeyValue` never signals on a
  frozen hive and signals on writes to a writable one.
  `RegLoadMUIStringA` answers `ERROR_CALL_NOT_IMPLEMENTED` natively on every
  input; it is hooked only so a virtual handle gets that same answer
  instead of reaching the real function.
- **Refused on virtual handles, passthrough on real ones**: they answer
  `ERROR_NOT_SUPPORTED` on a virtual handle of this copy, and the hook
  passes everything else to the trampoline without a registry. The
  transacted ones also refuse a real handle plus a path into a mount: the
  hook opens each prefix of the path through the registry and refuses when
  one opens as a virtual handle. They stay out of `VRegVtbl`: as
  slots they would either be required, so every wrapper forwards calls
  nothing answers, or carry a default that breaks passthrough behind a
  wrapper that leaves them null. One joins the table once a hive can answer
  it.
- **Not hooked**: the functions that take no key handle, which no virtual
  handle can reach.

### Install

- `VRegHooks_Install(pRegistry, pNative)` calls `DetourAttach` in a
  transaction of its own, enrolling the calling thread; a consumer with
  hooks of its own installs them beside it. Every hook calls through
  `pRegistry`, normally a virtual registry, or a wrapper in front of one
  (one that traces each call, say). `pNative` is the native registry below
  the hooks: its function table holds the targets handed to Detours, so
  after the commit it holds the trampolines. A second call answers
  `ERROR_ALREADY_INITIALIZED`.
- Install pins nothing, and nothing runs at unload. A body can run on any
  thread at any time, so no consumer can know the hooks are idle, and an
  unload crashes the next caller; neutralizing the slots at unload would
  protect only callers who arrive after it. The consumer pins every module
  a hook can reach, which VReg cannot know: the registry handed to Install
  and the hive providers may live in modules of their own, and a pin is
  irreversible.
- `VRegHooks_Neutralize()` makes every hook of the copy pass straight
  through; the module stays loaded, so the pass-through is safe.
- A consumer installing from `DllMain` builds, freezes and mounts its hives
  before the commit, so the first hooked call already finds them.

### Chaining and unloading

- Each copy's base registry is whatever its trampoline reaches: another
  hooker, another copy, or the real function. A copy answers what it owns
  (its mounts, its handles) and passes everything else down; on a path
  mounted by two copies the one on top wins (documented, not detected).
- The library never calls `DetourDetach`. Each hook's detour is a thunk
  allocated outside the module image (`VirtualAlloc`), `jmp [slot]`, with
  `slot` pointing at the detour body. On a `Neutralize` call the library
  writes `slot = trampoline` atomically, so the layer becomes a
  pass-through and other hookers' chains stay intact in any order. Thunks,
  slots and trampolines are leaked on purpose.
- The consumer keeps every module a hook can reach mapped, by pinning.
  Nothing short of that would do: a thread already inside a detour body when the module
  unmaps returns into unmapped code, switching the slot cannot reach it,
  and draining calls in flight is impossible, since detach runs under the
  loader lock, which a call inside a native open may be waiting for, and
  the count's decrement would run in the image itself.
- README recommendation, not a requirement: install before other registry
  hookers where possible.

### Rules for detour bodies

A detour body can run under a
foreign loader lock, so it never loads a module, creates a thread, or blocks
on a lock a loader-lock holder could own. A load the hooked call itself
makes natively is the caller's, not VReg's: `RegLoadMUIString` resolving an
indirect string loads its module, as native does on the caller's thread. Calls to the lower registry go
only through the base registry (the trampolines), never `::Reg*` directly,
which would re-enter the hooks and bypass test stubs.

## `.reg` format

### API

In the ".reg scripts" section, a `VRegScript` holds an ordered list of
`VRegOperation`s (create key, delete key, set value, delete value, each
with its root, path, name, type and data). Parsing, snapshot and delta
produce a script; applying and writing take an operation array
(`VRegScript_GetOperations` reads a script's), so operations built by hand
are applied and written like parsed ones. Parse and write are symmetric.

- `VRegScript_Parse(file bytes)`: a syntax error reports line and column.
- `VReg_ApplyOperations(operations, registry)`: reports the index of the
  failing operation.
- `VRegScript_Snapshot(registry, root, path)`: the merged view, no deletion
  operations; for fixtures and inspection.
- `VRegScript_Delta(hive, root, path)`: the hive's own layer, with
  tombstones as `[-K]` and `"Name"=-` and opaque keys as `[-K]` followed by
  `[K]`; the persistence format for overlays, and for an in-memory hive its
  whole contents. Re-importing over the same lower registry reproduces the
  view. `root` and `path` name where the hive is mounted; key names are that
  prefix plus the key's path in the hive. The walk is `VRegInternal_HiveSnapshotUpper`,
  a visitor under the hive's read lock that never calls the lower registry.
  A key that only shows a lower key through and holds no value of its own
  is omitted, so opening keys does not change the delta; one with upper
  values or value tombstones appears as `[K]` (it exists below, so creating
  it is harmless).
- `VRegScript_Write(operations, format)` lays operations out as a file; the
  file helpers (`VRegScript_ParseFile`, `VRegScript_WriteFile`) sit on top. Hooks
  never perform file I/O.

### Import

- Headers: `Windows Registry Editor Version 5.00` as UTF-16LE with BOM, or
  as UTF-8 with or without BOM; `REGEDIT4` in `CP_ACP`.
- Keys: `[path]` creates, `[-path]` deletes with the subtree. Roots by full
  name or as `HKLM`, `HKCU`, `HKCR`, `HKU`, `HKCC`.
- Values: `"name"=` and `@=`; strings with `\\` and `\"` escapes; `dword:`;
  `hex:`; `hex(n):` for any type; `=-` deletes.
- Trailing-backslash continuations, `;` comments, blank lines.
- Through an overlay, deletions become tombstones and `[-K]` then `[K]` an
  opaque key; through any other registry, they are real deletes.
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
- Keys and values sorted ordinal case-insensitive: stable fixture diffs
  matter more than mirroring regedit's enumeration order.
- `reg export`'s layout, byte for byte apart from the order: CRLF, hex data
  broken after the first comma past column 76 with a `\` continuation and
  a two-space indent, a blank line between keys.

## Errors

Every Win32-shaped entry point returns the `LSTATUS` native Windows
returns in the same situation; native behavior is the spec. There are no
exceptions: every allocation is checked where it happens and a failure
unwinds to a single cleanup label and answers `ERROR_OUTOFMEMORY`, as the
native registry does when it runs out of memory. `VREG_ASSERT` breaks into
an attached debugger in a `_DEBUG` build and does nothing otherwise. Where
VReg cannot match native Windows, the README lists it under "Known
divergences from native Windows".

## Testing

Catch2.

- **Native oracle**: each scenario runs against the real Win32 API (a
  registry table whose every slot is the import, `test/win32_api.h`) and
  the native registry on a private app hive (`RegLoadAppKey` on a temp
  file: real, no admin, no machine state), and against a virtual registry
  over an in-memory hive; statuses, sizes and bytes must match. A scenario
  takes an `Api`, the registry plus whether its keys are virtual, and skips
  what virtual keys cannot match for the reference run too. Covers
  ANSI/wide, buffer sizes and `ERROR_MORE_DATA`.
- **Ported unit tests**: overlay shadowing, enumeration index shifting,
  `QueryInfoKey` counts and failure cases, ancestor handles, native-path
  resolution, `WOW6432Node`.
- **Concurrency**: reader and writer threads (opens, creates, value writes
  and deletes, key deletes, enumerations, `QueryInfoKey`, watches, snapshot
  exports) on a writable in-memory hive, one frozen halfway through, and a
  writable hive layered over an app hive, each for about two seconds. They
  require only plausible status codes, no hang, and a final state that
  agrees with itself.
- **`.reg`**: byte-exact round trips, deltas with tombstones and opaque keys,
  parse errors with line and column; an opt-in regedit compatibility check
  on HKCU.
- **Hooks**: a separate test process (every architecture) installs the hooks and
  calls the real `Reg*` APIs; it loads two DLLs that each link the library
  statically, covering the shared handle namespace, chaining and a
  consumer's pin outliving `FreeLibrary`.
- **Handle probe**: the parity measurement above as a permanent test.
- **Single header** (`test/headers`): the implementation compiled
  warning-free as C, and the declarations as C++, after a plain
  `#include <Windows.h>`, without `/utf-8`, under every combination of
  `WIN32_LEAN_AND_MEAN`, `UNICODE` and `NOMINMAX`, and for Windows Vista and
  Windows 7 as `_WIN32_WINNT`.

### CI (GitHub Actions, public repository)

- Windows: {x86, x64, ARM64} x {MSVC, clang-cl}, all tests; ARM64 on an
  ARM64 runner.
- Wine on Ubuntu: the Windows-built x86 and x64 test binaries, all tests
  except those on a private app hive (the native oracle and every test
  against the real registry), which skip themselves there. Native Windows
  behavior is the spec, since it defines the API; where Wine deviates, that
  is a Wine bug, worked around in VReg only when the workaround costs
  nothing on Windows. A per-platform expectation list would make our
  behavior follow whichever Wine version CI installed.
- Proton and true remote `RegConnectRegistry` stay manual checks; the
  original implementation already runs under Wine and Proton.

## Packaging

- Static libraries only: each module links its own copy and the copies meet
  in the shared handle namespace, so a DLL would buy nothing, and two
  modules shipping different builds of it would collide on its name.
- `vreg.h` is the same code the libraries compile, so a consumer that
  copies it needs neither CMake nor vcpkg; the release attaches it too.
  - Its guards are explicit, not `#pragma once`: the file that defines
    `VREG_IMPLEMENTATION` may include it a second time after an earlier
    include without it.
  - The implementation is one translation unit, so every static helper
    carries `VReg_` and a name unique across the file.
    `VREG_STATIC` makes the library's own functions static as well.
  - The implementation `#undef`s its local helper macros (X-macro
    callbacks).
  - The version: `VREG_VERSION_MAJOR`, `_MINOR`, `_PATCH` and `_TWEAK` are
    written in the header, and CMake reads its project version from them.
    The header owns the version, as it owns everything else, so a copied
    file says which it is; a copy in `vcpkg.json` would be a second place
    to bump.
  - It carries the full license text, since it is copied on its own, and a
    UTF-8 BOM, since that text is not ASCII and MSVC otherwise reads it in
    the consumer's code page.
- Tests build only when VReg is the top-level project.
- Detours is found as the vcpkg port lays it out (header plus library, no
  CMake config).
