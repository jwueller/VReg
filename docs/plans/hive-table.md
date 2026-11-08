# A minimal API and hives as a function table

Two pieces of work, in this order: first the public API is cut down to what
earns its place, then a hive can be supplied by the caller as a function
table plus a context pointer, mounted like any other hive. A caller can
then expose a data structure of its own as registry keys, or compute values
when they are read instead of storing them up front. The virtual registry
keeps every Win32 rule (handles, caller buffers, ANSI and wide, views,
mounts); a hive answers a small node-level interface with owned values.

The cleanup comes first so that the table is built on the final shape of
the API. This file is deleted when the last step lands; git keeps how it
got there.

## Why

- The seam already exists. The virtual registry reaches hives only through
  the private `VReg_Hive*` and `VReg_Node*` functions ("Hive internals" in
  `vreg.h`) and reads no field of a hive or a node. Those functions are the
  slots of a table in all but name.
- The layered hive already does what a custom hive needs: on open,
  `VReg_HiveOpenKey` adds a transparent node for every lower key it
  reaches, a VReg-owned record over a key space the hive does not own. A
  custom provider is a lower that is a table instead of a registry.
- The Win32 surface is large (45 forms, buffer rules, `ERROR_MORE_DATA`,
  terminators); the node-level interface is about a dozen operations on
  owned values. A data source written against the second gets every Win32
  rule right for free, which a `VRegVtbl` implementation does not.
- It is the shape of FUSE and of the Projected File System: a handle space
  and the API rules in one place, the data behind callbacks.

## Decisions

Settled with the maintainer:

- `VReg` is opaque; a registry of the caller's is made with
  `VRegCreateCustom` over a `VRegVtbl` and a delegate. That stays: it is the
  extension point for wrappers that intercept the API (tracing, fault
  injection), and it keeps every slot.
- A hive becomes a function table with a context pointer, as
  `fopencookie` makes a `FILE` from functions and a cookie. This is a
  second extension point beside `VRegVtbl`: data, not interception.
- One hive struct, not a tagged pair of kinds. What varies is the lower
  behind the node tree: nothing (in memory), a registry subtree (layered),
  or the caller's table (custom). The layered hive's lower calls go through
  the same table, via an internal implementation over a registry, so the
  overlay tests exercise the seam and the virtual registry is untouched.
- A custom hive's key records are the hive's nodes, without values, plus a
  token the provider's `OpenKey` fills. Identity is the path, compared with
  `VReg_CompareNames` as for every node; the token is the provider's cache.
- Writes have a mode per hive: copy-up (the overlay) or through (to the
  provider's slots). The constructor sets it; no public parameter yet.
- No `Watch` slot. Watches stay in the node, and `VRegNotifyHiveChange` is
  the hive's own notify made public.
- Redirects replace the layered hive's escape hatches but stay private in
  this round.
- Records live as long as the hive, as nodes do. The README's
  handle-identity divergence gets a sentence on growth.
- The allocation context goes: nothing ever passed one, and a consumer who
  overrides the allocator macros reaches their own state from the macros.
- `RegLoadMUIStringW` resolves an indirect string of a hive value itself,
  as native resolves one of a real value: the module load is the call's own
  documented effect on the caller's thread, not a load VReg adds, so the
  loader-lock rule does not apply to it. The MUI mark and its setter go.
- The API cleanup below.

Considered and left alone:

- An overlay names its key twice: `VRegCreateLayeredHive` takes the lower
  root and path, and `VRegMount` repeats them. The lower must stay a hive
  property, since one hive can be mounted in several registries, so the
  repetition stays. Step 11 gives the README one sentence that says so.
- There is no unmount. Hives and registries only grow and must outlive
  each other, which fits a hooked process that lives until exit; the
  copy-on-write mount table would support one if something needs it.

Proposed, open until the maintainer confirms (each marked where it is
recorded):

- Agent's choice: the public pair mirrors the registry's,
  `VRegCreateCustomHive(pVtbl, pDelegate, &pHive)` and
  `VRegGetHiveDelegate(pHive)`, freed with `VRegDestroyHive`. A table of
  slots named `VRegHiveVtbl`. Passed over: a separate opaque type for
  custom hives, which would split every mount and script function in two.
- Agent's choice: slots take the hive, as `VRegVtbl` slots take the
  registry, and reach their object through `VRegGetHiveDelegate`.
- Agent's choice: the resolver reads the string table itself through
  `FindResourceExW`, `LoadResource` and `LockResource`, so the library
  still links nothing beyond `kernel32` and `advapi32`. Passed over:
  `LoadStringW`, which adds `user32`.

## Part 1: the API cleanup

Each item lands on its own with every test passing. Where a change makes
a sentence of the README or ARCHITECTURE wrong, the same commit fixes it.

### Allocation context

Every constructor loses `pAllocContext`. `VREG_MALLOC`, `VREG_REALLOC` and
`VREG_FREE` lose their first argument, and so does every private
allocation helper; the hive's and the registries' context fields and
`VReg_HiveAllocContext` go. `VRegFreeScriptBytes(pScript, pBytes)` becomes
`VRegFree(p)`, the one function that frees anything the library hands out.
Mechanical, but it touches most signatures in `vreg.h`, so it is one
commit of its own with no other change in it.

### Test seams leave the public constructors

- `VRegCreateNative` takes no table. The live table is reached through
  `VRegGetNativeFunctions`, and whoever needs to replace an entry (the
  tests) does so there before anything uses the registry.
  `VRegResolveNativeFunctions` goes; its body becomes the constructor's.
- `VRegCreateVirtual(pBase, &pRegistry)`: the `NtQueryKey` parameter goes.
  The registry resolves the export itself, as it does today when the
  parameter is null, and the tests set the private field under
  `VREG_INTERNAL`.

### One currency for `.reg` content

Producers (`VRegParseScript`, `VRegParseScriptFile`, `VRegSnapshotScript`,
`VRegDeltaScript`) answer a `VRegScript`; every consumer takes an operation
array. `VRegApplyScript` goes, since `VRegGetScriptOperations` plus
`VRegApplyOperations` is the same call. `VRegWriteScript` and
`VRegWriteScriptFile` take `(pOperations, cOperations, format, ...)`, which
closes today's hole where a hand-built array can be applied but not
written. The bytes `VRegWriteScript` answers are freed with `VRegFree`.

### One hook installer

`VRegInstallHooks` stays; `VRegAttachHooks` goes. A consumer with hooks of
its own calls it beside its own transaction, since sharing one transaction
buys nothing.

### Private things leave the public part

- `VRegIsHiveFrozen` and `VRegIsPredefinedHKey` become `VReg_` private
  functions; the tests build with `VREG_INTERNAL` and keep using them.
  `VRegIsVirtualHKey` stays public, since a wrapper in front of a registry
  has a reason to ask it.
- The `VReg_Node` typedef, `VREG_INTERNAL_VIRTUAL_HKEY_BASE` and the three
  `VREG_INTERNAL_REGISTRY_*` X-macros move below the public part. The
  public documentation of `VRegIsVirtualHKey` states the range in words.
  (The node typedef comes back as the public key record in part 2.)

### MUI text

Native `RegLoadMUIStringW` answers `ERROR_INVALID_DATA` for a plain string
(the oracle's `Plain` rows agree on native and virtual keys already) and
resolves an indirect one, `@module,-id`, by loading the module as a data
file and reading its string table; `pszDirectory` is where a relative
module path is looked up. VReg does the same for a value a hive holds:
read the value through the merged read, release every lock of its own,
expand environment strings, load with `LOAD_LIBRARY_AS_IMAGE_RESOURCE` and
`LOAD_LIBRARY_AS_DATAFILE`, read the string, free the module. The
`Indirect*` oracle rows then run on virtual keys and must match native,
status, sizes and bytes.

With that, `VRegSetHiveMuiTextValue`, `VReg_HiveSetMuiTextValue`,
`VReg_HiveQueryMuiText`, `VReg_HiveOpenLowerValueKey`, the per-value mark
and its handling in the export go; a lower-backed value is read like any
other and resolved by the same code instead of being forwarded to the
lower key. The README loses the `RegLoadMUIString` divergence, and the
loader-lock invariant in AGENTS and ARCHITECTURE gains its one qualifier:
a load the hooked call itself makes natively is the caller's, not VReg's.

## Part 2: hives as a function table

### Nodes and handles

- A slot works on key records. Open and create take a parent record plus
  the path components below it, as `VReg_HiveOpenKey` does today, and
  answer whether the key exists plus an optional token that the record
  keeps. Slots receive the record and read its token and its hive-relative
  path through `VRegGetHiveKeyToken` and `VRegGetHiveKeyPath`, so a
  provider that only knows paths stores nothing and interns nothing.
- The record is the hive's node without values (`VReg_Node`, public as the
  opaque `VRegHiveKey`), so a virtual handle maps to it as it maps to any
  node (`VReg_NodeHandle`, `VReg_NodeOf`, `VReg_NodeHive`) and path
  resolution, handle lookup and the watches change nothing. Identity lives
  in the path: one handle per record, `RegCloseKey` a no-op, a record whose
  key was deleted through VReg answering `ERROR_KEY_DELETED` for good
  (ARCHITECTURE, "One handle per node"). A provider that answers the same
  token for two paths gets two records; nothing compares tokens.
- Records live until the hive is destroyed, as nodes do. A provider over
  an unbounded key space grows by one record per distinct key opened; the
  README says so beside the handle-identity divergence. Reclaiming would
  need per-open handles with a release, which is additive if ever wanted.
- Records are added lazily under the hive's write lock, as the writable
  layered hive adds transparent nodes today, and never while a slot runs:
  look up, unlock, call the slot, lock again and insert (finding what
  another thread inserted meanwhile).

### The lower

- The hive's lower is a `VRegHiveVtbl` plus a delegate. `VRegCreateHive`
  sets none; `VRegCreateLayeredHive` sets the internal implementation over
  a registry subtree (the owned reads, redirects when frozen) with the
  lower registry, root, path and view as its delegate;
  `VRegCreateCustomHive` sets the caller's.
- Write mode is a field the constructor sets: copy-up for the layered hive,
  through for a custom hive. Copy-up over a custom lower (an overlay over
  computed data) and through over a registry lower (a junction) need no new
  design, only a constructor, and are not part of this work.
- Agent's choice: freezing a custom hive refuses writes through VReg and
  changes nothing else. Its records are still discovered lazily, so opens
  keep taking the lock; the "readers take no lock" invariant in AGENTS and
  ARCHITECTURE is qualified to the built-in tree. Passed over: refusing the
  freeze, which would need `VRegFreezeHive` to answer a status.

### Slots

Required (a hive without them cannot be made; `VRegCreateCustomHive`
answers `ERROR_INVALID_PARAMETER`):

- `OpenKey`: whether a key exists below a parent, plus its token, or
  `ERROR_FILE_NOT_FOUND`.
- `QueryValue`: type and data of a named value.
- `EnumKey`, `EnumValue`: by index, in an order that stays stable while
  nothing changes. Native enumerates subkeys by name and values in creation
  order; a provider that does otherwise diverges, and the README says so.

Optional, each with what VReg does when it is null:

- `CreateKey`, `DeleteKey`, `DeleteTree`, `SetValue`, `DeleteValue`:
  `ERROR_ACCESS_DENIED`, as a frozen hive answers; a read-only provider
  fills none of them. `CreateKey` receives the options and answers whether
  it created the key, from which VReg derives the disposition. Agent's
  choice: `DeleteTree` is never derived from the other slots, because a
  partial failure would leave a half-deleted tree that native never shows.
- `QueryInfo`: derived by enumerating keys and values once and taking
  counts and maximum lengths; correct but linear, which a provider with
  large keys avoids by filling the slot.
- `CheckKey` (what `RegFlushKey` needs): success.
- `Destroy`: frees what the delegate owns before VReg frees the hive.

### Results without private types

The private interface fills `VReg_WString` and `VReg_Buffer`, which a
public slot cannot name. Agent's choice: a slot hands results to VReg
through small sink functions that copy: value type and data, a name, a
class, a last-write time. A computed value can live on the provider's
stack and nothing it hands over needs a lifetime rule. The internal
registry implementation fills the private types directly, so nothing is
copied twice. Passed over: caller buffers with `ERROR_MORE_DATA`, which
brings back the buffer rules the table exists to hide, and returned
pointers valid until the next call, which every concurrent caller would
have to reason about.

A value can change between a caller's sizing call and its read, since it
is computed on each one. The owned reads already retry when a value grows;
the virtual registry needs nothing new, and the provider documentation says
that two reads may differ.

### Redirects instead of the layered hive's escape hatches

Two parts of the interface exist only for the layered hive (a third, the
lower key behind a MUI string, goes in part 1):

- `VReg_OpenResult.hLower`: a frozen layered hive answers an untouched key
  with a handle of its lower registry.
- `VReg_HiveLower`: the registry that answers those handles.

They become one general result of open, a redirect: "this key is answered
by registry R at key K". The internal registry implementation is the only
one that redirects: the sink that produces a redirect stays private until
a provider needs symlinks, because its constraint is a sharp edge to
publish: later calls on a redirected handle go to the base registry of the
virtual registry the hive is mounted in, so R must be that base or answer
the same handles.

### Change notification

Watches live in the node and fire on the hive's own writes; that does not
change. For a custom hive, writes through VReg fire them as today. A
provider whose data changes on its own calls
`VRegNotifyHiveChange(pHive, pKey, dwFilter)`, which signals the watches
the filter selects on the record and the subtree watches above it. A key
the provider deletes on its own is not marked deleted; its record answers
whatever the provider answers from then on.

### Rules a provider inherits

These are VReg's invariants, now binding on code VReg does not own, so the
public documentation states them as requirements:

- Slots may run under a foreign loader lock, on any thread, concurrently:
  no module loads, no thread creation, no waits on a lock a loader-lock
  holder could own.
- VReg holds no lock of its own across a slot call; the provider locks its
  own state.
- Names compare as the registry compares them, case-insensitively by
  uppercase ordinal. VReg passes names unchanged and makes its collation
  public as `VRegCompareNames`, so providers match native without guessing.
- `DeleteKey` on a key with subkeys answers `ERROR_ACCESS_DENIED`, as
  native does; VReg does not check first.
- The access mask of an open is never passed to a provider; access is per
  key, as the README's divergence for virtual keys already says.

### What stays built-in only

The upper-layer snapshot that `VRegDeltaScript` reads belongs to the
built-in tree. Agent's choice: on a custom hive it answers an empty
script, which is what the hive holds itself.

## Steps

Each step lands on its own with every test passing, checked against native
Windows as CONTRIBUTING describes, and corrects the README and
ARCHITECTURE where it makes them wrong.

Part 1, in the order of its sections:

1. The allocation context, alone.
2. The native and virtual constructors lose their test seams.
3. `.reg` content: operations as the one input, `VRegApplyScript` gone.
4. `VRegAttachHooks` gone.
5. Private things below the line.
6. `RegLoadMUIStringW` resolves indirect strings itself; the mark and the
   lower-key forward go. Tests first: the `Indirect*` oracle rows run on
   virtual keys.

Part 2:

7. Replace `hLower` and `VReg_HiveLower` with redirects, handled once in
   the virtual registry. No behaviour changes; the overlay tests cover it.
8. The internal lower table: the layered hive calls its lower only through
   a `VRegHiveVtbl` implemented over a registry subtree. No behaviour
   changes; the overlay tests cover it.
9. The public table: `VRegCreateCustomHive`, `VRegGetHiveDelegate`, the key
   accessors, the sinks, write-through, the fallbacks for null slots and
   `VRegCompareNames`. Tests first:
   - a custom hive whose table forwards through the flat functions to a
     registry over an in-memory hive runs the same scenarios as that hive
     mounted directly and answers the same, status, sizes and bytes;
   - a read-only computed provider (values derived on each read) answers
     `ERROR_ACCESS_DENIED` to every write and derives `RegQueryInfoKey`;
   - a provider whose value grows between sizing and read is read whole by
     `RegGetValueW`;
   - a hive missing a required slot is refused.
10. `VRegNotifyHiveChange`, with tests that a provider's own change signals
    a watch on the key and a subtree watch above it.
11. Documentation: the README's "What it does" and the provider rules,
    ARCHITECTURE's hive section; then delete this file.
