# Hives as a function table

A hive can be supplied by the caller as a function table plus a context
pointer, mounted like any other hive. A caller can then expose a data
structure of its own as registry keys, or compute values when they are
read instead of storing them up front. The virtual registry keeps every
Win32 rule (handles, caller buffers, ANSI and wide, views, mounts); a hive
answers a small node-level interface with owned values.

The API cleanup that preceded this (the allocation context, the test seams,
one currency for `.reg` content, one hook installer, private names below
the line, `RegLoadMUIStringW` resolving indirect strings itself) has landed;
git holds its plan. This file is deleted when the last step lands.

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
- Every commit that changes `vreg.h` raises its version as CONTRIBUTING
  says; a step that removes or changes a public function raises the minor
  version, a purely internal one the patch version.

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

## Design

### Nodes and handles

- A slot works on key records. Open and create take a parent record plus
  the path below it and answer whether the key exists plus an optional
  token that the record keeps. Agent's choice: the public slots take that
  path as one backslash-separated `LPCWSTR` below the parent, the shape
  `RegOpenKeyExW` gives its subkey, so no public string-view type is
  needed; the hive joins the components it resolved, and the internal
  registry implementation joins them again onto the lower path. Passed
  over: a public view type (pointer plus length), one more type for one
  parameter.
- Slots receive the record and read it through `VRegGetHiveKeyToken`,
  `VRegGetHiveKeyName` and `VRegGetHiveKeyParent`, all allocation-free
  and valid for the hive's life. Agent's choice: name and parent instead
  of a joined path, which would need storage the caller frees or a buffer
  rule; a provider that wants the path walks up and joins it. The access
  mask and the hive's own request flags (such as whether a frozen open
  wants a redirect) stay private and never reach a provider.
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
through small sink functions on an opaque `VRegHiveResult` that copy:
value type and data, a name, a class, a last-write time, and the key-info
counts. A computed value can live on the provider's stack and nothing it
hands over needs a lifetime rule. The token of an open and the "created"
answer of a create are plain out-parameters of those slots, not sinks.
The internal registry implementation fills the private types directly,
so nothing is copied twice. Passed over: caller buffers with `ERROR_MORE_DATA`, which
brings back the buffer rules the table exists to hide, and returned
pointers valid until the next call, which every concurrent caller would
have to reason about.

A value can change between a caller's sizing call and its read, since it
is computed on each one. The owned reads already retry when a value grows;
the virtual registry needs nothing new, and the provider documentation says
that two reads may differ.

### Redirects instead of the layered hive's escape hatches

Two parts of the interface exist only for the layered hive:

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

Steps 1 to 6 were the API cleanup and have landed. Each remaining step
lands on its own with every test passing, checked against native Windows
as CONTRIBUTING describes, raises the header's version as CONTRIBUTING
says, and corrects the README and ARCHITECTURE where it makes them wrong.

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
