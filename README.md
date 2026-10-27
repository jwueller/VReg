# VReg

A virtualized Windows registry for C++. VReg lets a process see registry
keys that don't exist on the machine, override values in keys that do, or
run against a registry held entirely in memory, without touching the real
registry.

It grew out of the virtual registry in
[DirectRetrofit](https://retrofit.games), which is its first consumer.

## What it does

- **Win32-shaped interface.** `Registry` has the `Reg*` signatures (A and W,
  `Ex` and pre-`Ex`), with owned-value overloads on top. Native, virtual and
  in-memory registries are interchangeable behind it.
- **Hives mounted into a virtual namespace.** A `VirtualRegistry` mounts
  in-memory or overlay hives at registry paths and passes everything else to
  a base registry (usually the real one). An overlay hive adds, overrides
  and deletes keys and values over the registry below it, while real values
  it does not override stay visible.
- **Pure in-memory mode.** With no base registry, nothing outside the
  mounted hives exists.
- **`.reg` files.** Import and export (REGEDIT4 and version 5), including
  deletions, for persistence and test fixtures.
- **Optional auto-hooking** of the process's `Reg*` calls through Microsoft
  Detours. It is safe under the loader lock, coexists with other hookers, and
  works with several modules in one process that each link VReg statically.

Windows only (x86 and x64), and runs under Wine and Proton.

## Status

In development, not yet usable; work follows the implementation plan.
- Design: `docs/specs/2026-10-27-VReg-design.md`
- Implementation plan: `docs/plans/2026-10-27-VReg-plan.md`

## Using it

CMake targets:
- `VReg::Core`: the interface, the registries, the hives and the handles.
- `VReg::RegFile`: `.reg` import and export.
- `VReg::Detours`: the hooks. Enable it with `-DVREG_WITH_DETOURS=ON`;
  needs Detours.

Consume it through `add_subdirectory`/`FetchContent`, or install it and use
`find_package(VReg CONFIG COMPONENTS Core RegFile Detours)`.

## Building

Needs Visual Studio 2022 or later (MSVC or clang-cl), CMake 3.25+, Ninja and
vcpkg (`VCPKG_ROOT` set; the copy bundled with Visual Studio works). From a
developer prompt for the target architecture:

```
cmake --preset msvc-x64
cmake --build --preset msvc-x64-debug
ctest --preset msvc-x64-debug
```

Presets: `{msvc,clang-cl}-{x86,x64}` to configure; append `-debug` or
`-release` to build and test. They enable the tests and `VReg::Detours`, with
Catch2 and Detours from vcpkg.

## License

MIT.
