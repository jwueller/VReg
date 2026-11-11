# AGENTS.md

Guidance for coding agents. **Read [README.md](./README.md) and
[CONTRIBUTING.md](./CONTRIBUTING.md) first**: the README owns what VReg is
and how to use it, its scope and its known divergences; CONTRIBUTING owns
how to build, test, package and release it, and where things are in
`vreg.h`. A fact has one home: a doc comment states the contract of its declaration,
a section comment the mechanism, and the README points at them instead of
restating. Anything a human needs belongs there, not here. A
plan for unfinished work goes in `docs/plans/` and is deleted once the
work is done; git keeps how it got there.

## Working in this repository

- Commit autonomously; push only with the maintainer's sign-off. Never
  force-push or rewrite published history unless told to.
- Keep history to settled decisions. Unpushed commits that record churn
  (a discussion resolved before it landed, a fix to the previous commit)
  are amended or squashed, not stacked.
- Keep formatting-only commits separate from semantic ones.
- Documentation is corrected in place, never appended to: when a change
  makes a sentence wrong, fix or delete it. Git history is the record of
  what was true when.
- Spell the name VReg wherever no convention forces lowercase (the vcpkg
  port `vreg`) or uppercase (CMake options `VREG_*`).
- VReg grew out of DirectRetrofit, and saying so is fine. Never reference
  DirectRetrofit's internals (its paths, symbols or docs) in files or
  commit messages here.
- Tests come first, checked against native Windows as
  [CONTRIBUTING.md](./CONTRIBUTING.md) describes: status codes, sizes and
  bytes alike.

## Invariants that are easy to break

- **Detour bodies can run under a foreign loader lock.** The `@note` on
  `VReg` in `vreg.h` states the rule; the hooks section comment names the
  one load it allows, and the hives' locking comment why their lock is
  never held across a call into the lower.
- **Lower-registry access goes only through the flat functions on the base
  registry** (after hooking: the trampolines). A direct `::Reg*` call
  inside the library re-enters the hooks and bypasses test stubs.
- **Frozen built-in hives are immutable** (in-memory or overlay), and their
  readers take no lock; a frozen custom hive only refuses writes, and keeps
  locking to add the records of keys it opens. The mount table is replaced
  copy-on-write, and old tables are never freed.
- **Virtual handles are odd values at most `0x7FFFFFFF`.** Never hand one to
  a real registry API. The shared handle section's layout is an ABI between
  separately built copies, so it never changes.
- **Hooks are never detached**; nothing calls `DetourDetach`. The hooks
  section comment of `vreg.h` says how they are neutralized instead.
- **No `std::mutex` in the library.** It locks with `SRWLOCK`; Wine's
  `msvcp140.dll` aborts on the current `std::mutex`.

## Code conventions

These follow Win32 naming, so the library reads as a natural extension of
the API it sits beside.

- C99 as MSVC 2013 and later compile it (declarations anywhere, `//`
  comments, designated initializers; no variable-length arrays, no `_Bool`
  in the API), MSVC and clang-cl, x86, x64 and ARM64, and MinGW-w64 GCC for
  the core on x64. The headers compile
  as C and as C++, and the tests are C++20. `WIN32_LEAN_AND_MEAN`,
  `NOMINMAX` and `UNICODE` everywhere in the library's own build.
- Public functions are `VReg`, the object, an underscore and the verb,
  as COM's `IUnknown_QueryInterface` macros are: `VRegHive_CreateInMemory`,
  `VRegScript_Parse`. The registry itself is the implied object
  (`VReg_Create`, `VReg_Mount`), and so is the library for what has no
  object (`VReg_CompareNames`); the `Reg*` family is `VReg_` plus the
  whole Win32 name (`VReg_RegOpenKeyExW`), so that name stays greppable.
  All are `WINAPI`, declared with `VREG_DECL`, and are all a single-header
  consumer gets with external linkage. A private function, static
  ones included, carries `Internal` once, opening the private part of its
  name: the type when that is private (`VRegInternalWString_Free`), the
  verb when the object is public (`VRegHive_InternalCreate`,
  `VReg_InternalAlloc`). Its name is unique across `vreg.h`, since the
  implementation is one translation unit. A private function is `static`
  unless the hooks implementation or a test calls it; only those are
  declared in the private declarations, with `VREG_INTERNAL_DECL` (`extern`
  where the library's build or a test defines `VREG_INTERNAL`, `static`
  otherwise; the header never defines it), and every other
  one is defined `static` in the implementation, with a `static` forward
  declaration in the titled section of its area where definition order needs
  one. A function only a test calls belongs in the test: the library's
  single-header build would leave it unused. Types carry no underscore:
  public ones are `VRegName`, private ones `VRegInternalName`. A private
  type that one public object's code alone uses carries that object's name
  before `Internal` (`VRegScriptInternalParser`, `VRegHiveInternalNode`),
  and so do its functions; one shared across objects, or owned by the
  registry itself, stays `VRegInternalName` (`VRegInternalWString`,
  `VRegInternalMountPoint`). A struct or enum has a tag only where C needs
  one, for an opaque or forward-declared type
  (`typedef struct VRegHive VRegHive;`); any other is anonymous.
- Win32's typedefs wherever they say more than the C type: `LPCWSTR` and
  `LPCSTR` for a terminated string, `LPCWCH` and `LPCCH` for counted
  characters, `LPWSTR` for a buffer that becomes one. No `_z_` beside them:
  the SDK's `LPCWSTR` already carries `_Null_terminated_`, so it writes
  `_In_opt_ LPCWSTR`, and so does VReg. `const` precedes what
  it qualifies (`const LPCWSTR psz`, never `LPCWSTR const psz`).
- Hungarian prefixes like the Win32 API beside them: parameters and locals
  (`pszPath`, `cbData`, `hKey`), struct members in Win32's shape without
  `m_` (`pShared`, `cbData`), globals `g_`. A global is named like a
  function of its object: `g_`, the type it is an instance of
  (`g_VRegVtbl_Empty`, `g_VRegHiveVtbl_RegistryLower`) or the public object
  it belongs to (`g_VRegHooks_pRegistry`, `g_VReg_pfnImportAdvapi32`), an
  underscore and the Hungarian name. The implementation compiles inside a
  consumer's own file, where a plain `g_pRegistry` could collide with the
  consumer's.
- File names are lowercase `snake_case`: a mis-cased include works on a
  case-insensitive filesystem and breaks on a case-sensitive one, so case
  never carries meaning in a path. For the same reason, a Windows SDK header
  is included as the SDK spells its file (`Windows.h`, `Shlwapi.h`).
- The library is `vreg.h` alone, included as `<vreg.h>`. A declaration or
  definition goes in the part its visibility gives it (CONTRIBUTING's "Where
  things are" lists the parts), inside the titled section of its area.
- `vreg.h` has explicit guards, one per part, since it is entered again
  with `VREG_IMPLEMENTATION` after an earlier include. Other headers (the
  tests') use `#pragma once`: every compiler that builds VReg supports it,
  and where it fails (one header reached through two paths) guards would
  silently mix versions instead.
- Constants are macros, ALL_CAPS like the Win32 ones beside them, prefixed
  `VREG_` when public (`VREG_VERSION_MAJOR`) and `VREG_INTERNAL_` when
  private (`VREG_INTERNAL_INITIAL_NAME_CHARS`). A name a Windows header already defines
  (`TEXT`, `NO_DATA`) is taken by it: pick another, or use the Win32
  constant itself. A compile-time invariant between constants, or of a
  layout, is `VREG_STATIC_ASSERT`, which works in C99 and C++, never a
  bare `static_assert` or the SDK's `C_ASSERT`.
- `VRegVtbl` holds one slot per form of the Win32 `Reg*` family,
  `Reg` prefix included, and only the ANSI and wide names exist: a neutral
  name (`RegOpenKeyEx`) is a `winreg.h` macro, and VReg defines the same
  selectors for its flat functions. Nothing else joins the table. Each form
  has one function typedef (`VRegOpenKeyExWFunction`) that links its
  Microsoft documentation with `@see`; its slot, its flat function and every
  declaration of an implementation use the typedef, and only definitions
  repeat the signature. The README's
  [Supported functions](./README.md#supported-functions) lists every `Reg*`
  function with the same links, and its order is the order of the typedefs,
  the slots and every list of declarations: the ANSI form right before the
  wide one, so a mismatch between the two stands out.
- Win32-shaped entry points return `LSTATUS`. There are no exceptions:
  every allocation is checked and a failure answers `ERROR_OUTOFMEMORY`, and
  a function with several resources frees them at one `done:` label.
- Win32-shaped functions carry the SDK's SAL annotations verbatim
  (`ulOptions` is `_In_`, since the analyzer rejects the SDK's `_In_opt_` on
  a value), repeated on every definition, because `/analyze` checks a body
  only against its own annotations and warns when a definition drops them.
  VReg's own pointer parameters and nullable pointer returns carry SAL too.
- A parameter and its annotations share one line, however long, so every
  parameter reads the same way. Only a long `_When_` condition (the ones on
  `RegGetValueW`'s `pvData`) spans lines above its parameter, laid out as
  the IDE formatter lays out a call: `_When_(` at the parameter's indent,
  one operand per line one level deeper with `||` leading, and `)` back at
  the parameter's indent.
- Every function name has a verb. An accessor is `Get` and what it answers
  (`VReg_GetContext`, `VRegInternalWString_GetText`), a lookup by key is
  `Find` (`VReg_InternalFindMount`, `VReg_InternalFindExport`), a
  conversion that returns its result is `YFromX`
  (`VReg_InternalViewFromText`), and any other computation names what it
  does (`VReg_InternalGrow`, `VReg_InternalAlignToDword`), never its result.
- A public object made by a `Create` function is ended by `Destroy`, and so
  is the slot that runs then; memory and a struct's contents are `Free`d
  (`VReg_Free`, `VRegInternalWString_Free`).
- Parameter names are never omitted or commented out; an unused one is
  `(void)name;` in the body.
- Signatures of the Win32-shaped functions and every call to a `Reg*`
  function (Win32 or a `VReg_*` flat function) put one parameter or argument
  per line, with blank lines between multi-line declarations. Any other
  signature does the same once it would run its line past column 80, the
  first ruler in `.editorconfig`; a call stays on one line where that reads
  better. A `(void)` signature stays on one line however long: it has no
  parameters to scan down.
- A braced list that spans lines opens with `{` alone, puts each item on a
  line of its own one level deeper with a trailing comma, and closes with
  `}` at the opening indent; never `{a,` continued as `b}`.
- Tests set off each test case and each `SECTION` with a blank line.
- Test data that reads as a table (rows of the same shape) keeps one row per
  line, columns aligned with spaces, between `// @formatter:off` and
  `// @formatter:on` so the IDE formatter leaves it alone.
- A CMake command that takes a list (sources, libraries, options,
  definitions) keeps everything before the list on its first line and puts
  each item, even a single one, on a line of its own; the IDE formatter
  keeps that layout. A multiline statement is set off by blank lines, with
  its comment attached.
- Neither `min` nor `max` is called: the library uses `VReg_InternalMinDword` and
  `VReg_InternalMaxDword` or a comparison, so the `Windows.h` macros never apply,
  even in a consumer that includes it without `NOMINMAX`. A C++ test calls
  them parenthesized, `(std::max)(a, b)`.
- Macros are tested with `#ifdef` or `#ifndef` in simple cases, and with
  `defined(...)` in an `#if` where a condition spans several macros. An
  include guard (`vreg.h` has them) keeps its usual shape and is never folded
  into another condition.
- No init-statement in an `if`, and no state-changing call inside its
  condition: assign the result in a statement of its own first. A single
  declaration as the condition is fine (C++ tests only; C has none).
- Shared state is read and written with the `ReadAcquire`,
  `WriteRelease`, `ReadPointerAcquire` and `WritePointerRelease`
  intrinsics and the `Interlocked*` functions; a one-time initialization
  uses `InitOnceExecuteOnce`. A phase under a lock is a helper function
  with a single unlock, so no path can leave the lock held.
- Doc comments are Doxygen with leading `///`. They name every symbol with
  `@ref`, Win32 ones included (`@ref ERROR_MORE_DATA`); backticks are for
  what is not a symbol: code fragments, file and module names, key names,
  patterns like `RRF_*`.
