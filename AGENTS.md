# AGENTS.md

Guidance for coding agents. **Read [README.md](./README.md) and
[CONTRIBUTING.md](./CONTRIBUTING.md) first**: the README owns what VReg is
and how to use it; CONTRIBUTING owns how to build, test and release it,
and where the design lives. Anything a human needs belongs there, not
here. The design is owned by [docs/specs/](./docs/specs/), the order of
work by [docs/plans/](./docs/plans/).

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

- **Detour bodies can run under a foreign loader lock.** Nothing reachable
  from one may load a module, create a thread, or wait on a lock that a
  loader-lock holder could own. A hive's lock is never held across a call
  into the lower registry.
- **Lower-registry access goes only through the base `Registry`** (after
  hooking: the trampolines). A direct `::Reg*` call inside the library
  re-enters the hooks and bypasses test stubs.
- **Frozen hives are immutable**; their readers take no lock. The mount
  table is replaced copy-on-write, and old tables are never freed.
- **Virtual handles are odd values at most `0x7FFFFFFF`.** Never hand one to
  a real registry API. The shared handle section's layout is an ABI between
  separately built copies, so it never changes.
- **Hooks are never detached**; they are neutralized through their slot.
  Nothing calls `DetourDetach`.
- **No `std::mutex` in the library.** It locks with `SRWLOCK`
  ([src/core/lock.h](./src/core/lock.h) says why); Wine's `msvcp140.dll`
  aborts on the current `std::mutex`.

## Code conventions

Agent's choice: these follow Win32 naming, because the library sits beside
the API it reimplements. Change them freely before the first release.

- C++20, MSVC and clang-cl, x86 and x64. `WIN32_LEAN_AND_MEAN`, `NOMINMAX`
  and `UNICODE` everywhere.
- Namespace `VReg`, bodies not indented. Types and functions PascalCase;
  members `m_`, globals `g_`, Hungarian prefixes like the Win32 API beside
  them. Flat functions are `VReg` plus the Win32 name.
- `Registry` members are the Win32 functions, `Reg` prefix included, and
  only their A and W names exist: a neutral name (`RegOpenKeyEx`) is a
  `winreg.h` macro. Nothing else joins the class; helpers over it are free
  functions.
- Win32-shaped entry points are `noexcept` and return `LSTATUS`.
- Conversions that return their result are named `YFromX`.
- Parameter names are never omitted or commented out; an unused one is
  `(void)name;` in the body.
- Signatures of the Win32-shaped functions and every call to a `Reg*`
  function (Win32 or a `Registry` member) put one parameter or argument per
  line, with blank lines between multi-line declarations. Any other call or
  signature does the same once it would run its line past 80 columns.
- No init-statement in an `if`, and no state-changing call inside its
  condition: assign the result in a statement of its own first. A single
  declaration as the condition is fine.
- Doc comments are Doxygen with leading `///`.
