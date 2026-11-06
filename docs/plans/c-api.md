# VReg as a C library

Turn VReg from a C++20 library into a C99 single-header library in the
stb style (`docs/stb_howto.txt` in nothings/stb), with `windows.h` as its
one dependency and Microsoft Detours as the optional one behind the hooks.
This file is deleted when the last step lands; git keeps how it got there.

## Why

- C++20 is a tall requirement for a library that lives inside other
  people's processes; the single header compiles its implementation in the
  consumer's translation unit, so even the internals' standard is forced on
  every consumer, as is `/EHsc`.
- STL types in the public API tie the static library to the consumer's
  toolset. Win32 types plus a function table are stable across toolsets and
  reachable from C, C#, Rust and Delphi.
- The registry is Windows-specific, so `windows.h` already provides what the
  STL was used for: memory, strings, code pages, locking, atomics, files.

## Decisions

Settled with the maintainer:

- C API only; no C++ wrappers. Objects are opaque pointers passed explicitly
  as the first argument.
- Flat functions named `VReg` plus the Win32 name: a call site ports from
  `Reg` to `VReg` plus one leading registry argument. This is the shape of a
  COM method call with the vtable hidden behind the flat function.
- C99 as MSVC 2013 and later compile it: declarations anywhere, `//`
  comments, designated initializers; no variable-length arrays, no `_Bool`
  in the API (`BOOL` as Win32).
- Allocation goes through `VREG_MALLOC(ctx, cb)`, `VREG_REALLOC(ctx, p, cb)`
  and `VREG_FREE(ctx, p)`, overridable before the implementation include.
  The context is given when a registry or hive is created and stored in it.
- Relicensing is open to the maintainer; the stb norm is dual public domain
  and MIT. Pending: pick one and update `LICENSE`, the amalgamated notice and
  the vcpkg port.
- Every exported function and every vtable slot is `WINAPI`, like the `Reg*`
  family: x86 `__stdcall` parity, and the default for C# and Rust bindings.
- A vtable holds a slot for all 45 forms; a NULL slot means "derive from the
  wide `Ex` form", which the flat function does. This keeps the rule that a
  tracing wrapper can tell an ANSI call from a wide one without forcing every
  implementation to fill 45 slots. A plain table of the 15 wide forms was
  passed over because wrappers lose the form.
- Default allocation macros use `HeapAlloc`, `HeapReAlloc` and `HeapFree` on
  the process heap, not `malloc`: no CRT dependency, safe under the loader
  lock. `VREG_ASSERT(x)` defaults to `__debugbreak()` under an attached
  debugger in `_DEBUG` builds and to nothing otherwise.
- `VREG_STATIC` makes every definition `static` (rule 4 of the howto). The
  core has no global beyond the per-copy handle space, so two static copies
  in one module behave as two copies; two sets of hooks in one module become
  two layers, legal and pointless. Documented, not forbidden.
- Private symbols carry `VReg__` (double underscore, rule 6 of the howto),
  replacing the rule that anonymous-namespace helpers need unique names.
- `FileVersion` becomes the packed `ULONGLONG` of `MAKEDLLVERULL`, which it
  already is in memory.
- The `.reg` writer's `Record` and the hive's `UpperRecord` dissolve into the
  one `VRegOperation`: snapshot and delta produce a script, and one writer
  lays a script out. Parse and write become symmetric.

To be evaluated:

- Whether to keep/reimplement optional C++ conveniences like owned-buffer forms:
  `QueryValue`, `GetString`, etc.

## API sketch

Types:

- `VRegRegistry`: `struct VRegRegistry { const VRegRegistryVtbl* lpVtbl; }`
  as the first member; everything after it is private to its implementation.
  A wrapper embeds it as its first member.
- `VRegRegistryVtbl`: one `WINAPI` slot per form `Registry` has today, with
  the self pointer first; plus `Destroy(self)`, NULL for an object the caller
  owns otherwise.
- `VRegNativeFunctions`: the table of the 15 wide `Ex` entry points the
  native registry calls and the hooks attach to; `decltype(&::name)` fields
  become the SDK's function pointer types written out.
- `VRegHive`: opaque.
- `VRegScript`, `VRegOperation`, `VRegParseError`, `VRegFormat`: the `.reg`
  model. `VRegOperation` is a plain struct (`kind`, `hRoot`, `pszPath`,
  `pszName`, `dwType`, `pData`, `cbData`, `line`); a script exposes its
  operations as an array plus count.

Functions, by group (the `Reg*` family is `VReg` plus the Win32 name, wide
and ANSI, with `#define VRegOpenKeyEx VRegOpenKeyExW` under `UNICODE` and
the ANSI form otherwise, for every pair):

- Registries: `VRegCreateNativeRegistry(pFunctions /* NULL = resolve */,
  pAllocContext, &pRegistry)`, `VRegGetNativeFunctions(pRegistry)`,
  `VRegResolveNativeFunctions(&functions)`,
  `VRegCreateVirtualRegistry(pBase, pfnNtQueryKey, pAllocContext,
  &pRegistry)`, `VRegMount(pRegistry, hRoot, pszPath, pHive, samView)`,
  `VRegDestroyRegistry(pRegistry)`.
- Hives: `VRegCreateHive(pAllocContext, &pHive)`,
  `VRegCreateLayeredHive(pLower, hLowerRoot, pszLowerPath, samLowerView,
  pAllocContext, &pHive)`, `VRegFreezeHive`, `VRegIsHiveFrozen`,
  `VRegSetHiveMuiTextValue(pHive, pszPath, pszName, pszText)` (by path from
  the hive root, since nodes are private), `VRegDestroyHive`.
- Handles: `VRegIsVirtualHKey`, `VRegIsPredefinedHKey`,
  `VREG_VIRTUAL_HKEY_BASE`.
- `.reg`: `VRegParseScript(pFile, cbFile, pAllocContext, &pScript, pError)`,
  `VRegParseScriptFile(pszPath, ...)`, `VRegApplyScript(pScript, pRegistry,
  pFailedIndex)`, `VRegSnapshotScript(pRegistry, hRoot, pszPath,
  pAllocContext, &pScript)`, `VRegDeltaScript(pHive, hRoot, pszPath,
  pAllocContext, &pScript)`, `VRegWriteScript(pScript, format, &pBytes,
  &cbBytes)` (freed with `VRegFreeScriptBytes`), `VRegWriteScriptFile(pScript,
  format, pszPath)`, `VRegDestroyScript`.
- Hooks (`VREG_WITH_DETOURS`): `VRegAttachHooks(pRegistry, pNative)`,
  `VRegInstallHooks`, `VRegNeutralizeHooks`.
- Version: `VRegGetVersion()` (text), `VRegGetFileVersion()` (`ULONGLONG`).

Every function keeps its Win32 SAL annotations, its `LSTATUS` answer and the
behavior the native oracle checks today. Doxygen stays `///` with `@ref`.

## Steps

Bottom-up, each module rewritten in C behind a C header while the C++
remainder calls it; the C++ public classes shrink to forwarding shims and go
when nothing is left behind them. The full suite passes after every step,
with the native oracle judging bytes and statuses as before. CMake adds `C`
to the project languages at step 1.

1. **Handles and the handle space.** `handle.h` and `handle_space.*` to C.
   Leaf code; `test_handle.cpp` moves to the C functions. The shared
   section layout is an ABI and does not change.
2. **Hive.** `hive.cpp` and `node.h` to C behind `VRegHive`, with the
   allocation macros and context, exceptions replaced by status returns, the
   node-level API private (`VReg__Hive*` in `core/hive.h`). The vtable type
   and the flat wide `Ex` functions land here too, since a layered hive's
   lower registry is a `VRegRegistry`; the C++ `Registry` class exposes
   itself as one through a forwarding table. The C++ `Hive` class forwards
   until step 3, and the tests keep constructing hives through it until
   then, because `VirtualRegistry::Mount` still takes the C++ class.
3. **Native and virtual registry.** `native_registry.cpp` and
   `virtual_registry.cpp` to C as vtable implementations
   (`VRegCreateNativeRegistry`, `VRegCreateVirtualRegistry`, `VRegMount`).
   The C++ `NativeRegistry` and `VirtualRegistry` classes become shims over
   the C objects, and `Registry::CObject()` turns virtual so a shim answers
   its C object directly; the tests stay on the shims, since the C API they
   would move to is complete only after step 4.
4. **Forms and `.reg`.** The ANSI, pre-`Ex` and convenience forms
   (`registry.cpp`) and `regfile/` to C, with `VRegOperation` replacing the
   three record structs; the owned-buffer helpers the tests need live in
   `test/`. The C++ `Registry` base members and a `RegFile` shim forward to
   the flat functions until step 5 moves the tests.
5. **Tests to the C API.** Every test file calls the flat functions
   (`registry.RegOpenKeyExW(...)` becomes `VRegOpenKeyExW(pRegistry, ...)`),
   owns hives, registries and scripts through the owners in
   `test/objects.h`, and the three test registries (the real Win32 API, the
   intercepting wrapper, the app-hive mapper) are C vtable implementations.
   A scenario runs against an `Api` (a registry plus whether its keys are
   virtual) instead of a template parameter. The hooks' entry points become
   C here too (`VRegAttachHooks`, `VRegInstallHooks`, `VRegNeutralizeHooks`),
   since the hook tests mount through C objects; the hook bodies stay C++
   until step 6.
6. **Hooks, shim removal, packaging.** `hooks.cpp` to C (`Emulated<>` and
   `Refused<>` become a generated table of hook bodies); delete the C++
   headers and shims; the amalgamator emits one C header with the
   implementation in C; `VREG_STATIC`; the headers test matrix compiles the
   header as C (`/TC`) and as C++, and the implementation as C, under every
   combination of `WIN32_LEAN_AND_MEAN`, `UNICODE` and `NOMINMAX`; CMake
   target language C, `/W4 /WX`, `/analyze` on C; the vcpkg port and the
   README's usage and supported-functions sections follow; AGENTS.md's code
   conventions become the C ones (prefixes, `VReg__`, no classes). Relicense
   here.

Done means: the repository builds the library and the tests from C sources
plus Catch2 tests in C++, `vreg.h` compiles as C99 and as C++, no `std::`
symbol remains in the library, and every test that passes today passes.

## Conventions to carry over or change

- File names stay lowercase `snake_case`; sources become `.c`.
- Win32 naming stays: PascalCase functions, Hungarian parameters, ALL_CAPS
  constants now prefixed `VREG_` (no namespace carries the prefix anymore).
- SAL on every declaration and definition, one parameter per line in the
  Win32-shaped functions, as today.
- No exceptions, so every allocation site checks and unwinds; the function
  try-block convention and `StatusFromException` go.
- `min`/`max`: the parenthesized call rule becomes moot in C; use local
  helpers, never the `windows.h` macros, since consumers may define
  `NOMINMAX`.
- Sorting: children stay a sorted array with insertion at the found index;
  the `.reg` export sorts a snapshot once with a small own sort.
- Until step 5 the C sources are written in the subset C99 and C++20 share
  (explicit casts from `void*`, no compound literals, designated initializers
  in declaration order), because the amalgamation tests compile the inlined
  `.c` files as C++ until the generator emits C.
- Agent's choice: struct members take Win32's shape (`pShared`, `cbData`)
  without `m_`, since C has no member functions for a local to be confused
  with. Private functions (`VReg__Name`) take the default calling
  convention; only the public ones are `WINAPI`.
- Lock-free loads and stores use `ReadPointerAcquire` and
  `WritePointerRelease`; one-time initialization uses
  `InitOnceExecuteOnce`. Both are Vista APIs, the minimum VReg targets.
- The handle space allocates its block tables from the process heap
  directly, not through the allocation macros: they live for the process
  and belong to no hive or registry whose context could own them.
- Agent's choice: `VRegRegistry` carries `pAllocContext` after `lpVtbl`, so
  the flat functions that derive a form (an ANSI call widening its
  arguments, say) allocate their temporaries with the registry's context.
  A `GetAllocContext` slot was passed over: every implementation would fill
  it the same way, and a wrapper copies the field as it copies the table.
- `VRegOperation` points at its path, name and data instead of owning them;
  a script owns the storage of the operations it holds, and a caller that
  builds operations by hand applies them with `VRegApplyOperations`. The
  writer lays a value out under the last key line it wrote and opens a
  `[key]` line of its own for a value whose key differs, so a hand-built
  script lays out as a parsed one would.
- The parse error is zeroed on every outcome but a syntax or encoding error,
  so the parameter is a plain output; the analyzer cannot follow an output
  written on failure paths only.

## Open

- The license to relicense to.
- Whether `VRegMount` stays a flat function that only a virtual registry
  answers (`ERROR_INVALID_HANDLE` otherwise) or becomes a slot; flat is
  planned.
- Whether to keep a prebuilt static library in the vcpkg port once the
  header compiles as C in any toolset, or make the port header-only.
