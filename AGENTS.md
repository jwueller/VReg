# AGENTS.md

Guidance for coding agents. **Read `README.md` first**: it owns what VReg
is, how to build and use it, and where things live. Anything a human needs
belongs there, not here. The design is owned by `docs/specs/`, the order of
work by `docs/plans/`.

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
- Tests come first. Native Windows behavior is the spec: a scenario that
  can run against the real registry (a private app hive) runs against it
  too, and the virtual answer must match its status codes, sizes and bytes.

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

## Code conventions

Agent's choice: these follow Win32 naming, because the library sits beside
the API it reimplements. Change them freely before the first release.

- C++20, MSVC and clang-cl, x86 and x64. `WIN32_LEAN_AND_MEAN`, `NOMINMAX`
  and `UNICODE` everywhere.
- Namespace `VReg`, bodies not indented. Types and functions PascalCase;
  members `m_`, globals `g_`, Hungarian prefixes like the Win32 API beside
  them. Flat functions are `VReg` plus the Win32 name.
- Win32-shaped entry points are `noexcept` and return `LSTATUS`.
- Conversions that return their result are named `YFromX`.
- Parameter names are never omitted or commented out; an unused one is
  `(void)name;` in the body.
- Doc comments are Doxygen with leading `///`.
