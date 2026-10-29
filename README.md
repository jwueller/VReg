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
  deletions, for persistence and test fixtures. Exports match `reg export`
  byte for byte apart from the order of values; the UTF-8 form imports
  with `regedit /s` and `reg import` alike.
- **Optional auto-hooking** of the process's `Reg*` calls through Microsoft
  Detours. It is safe under the loader lock, coexists with other hookers, and
  works with several modules in one process that each link VReg statically.

Windows only (x86 and x64), and runs under Wine and Proton.

## Status

v1 released as v0.1.0.
- Design: `docs/specs/2026-10-27-VReg-design.md`
- Implementation plan: `docs/plans/2026-10-27-VReg-plan.md`

## Using it

CMake targets (static libraries):
- `VReg::Core`: the interface, the registries, the hives and the handles.
- `VReg::RegFile`: `.reg` import and export.
- `VReg::Detours`: the hooks. Enable it with `-DVREG_WITH_DETOURS=ON`;
  needs Detours.

Consume it through `add_subdirectory`/`FetchContent`, or install it and use
`find_package(VReg CONFIG COMPONENTS Core RegFile Detours)`.

### Through vcpkg

This repository is a vcpkg registry. Add it to the consumer's
`vcpkg-configuration.json`, with a VReg commit (the latest on `master`) as
the baseline:

```json
{
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/jwueller/VReg",
      "baseline": "<VReg commit>",
      "packages": ["vreg"]
    }
  ]
}
```

Then depend on `vreg`, or `{"name": "vreg", "features": ["detours"]}` for
the hooks, in `vcpkg.json`. The port prints its CMake usage on install.

A change to `ports/vreg` needs a new version entry (bump `version-semver`
or `port-version` first), committed after the port change itself, since it
records the port's git tree:

```
vcpkg x-add-version vreg --x-builtin-ports-root=ports --x-builtin-registry-versions-dir=versions
```

### Overlay over the real registry

Keys and values added, overridden or deleted under one real key; everything
else is the real registry.

```cpp
VReg::NativeRegistry native;
VReg::OverlayHive overlay(native, HKEY_CURRENT_USER, L"Software\\Vendor\\Game");
VReg::VirtualRegistry registry(&native);
registry.Mount(HKEY_CURRENT_USER, L"Software\\Vendor\\Game", overlay);

const DWORD dwOne = 1;
registry.SetKeyValueW(HKEY_CURRENT_USER, L"Software\\Vendor\\Game", L"Windowed", REG_DWORD, &dwOne, sizeof(dwOne));
```

Writes stay in memory. `VReg::RegFile::ExportDelta` saves them as a `.reg`
file that `VReg::RegFile::Apply` replays later.

### Held entirely in memory

```cpp
VReg::MemoryHive machine;
VReg::MemoryHive user;
VReg::VirtualRegistry registry;   // no base: nothing else exists
registry.Mount(HKEY_LOCAL_MACHINE, L"", machine);
registry.Mount(HKEY_CURRENT_USER, L"", user);
```

### Hooked into the process

```cpp
// Leaked on purpose: the hooks use them until the process ends.
auto* native = new VReg::NativeRegistry;
auto* registry = new VReg::VirtualRegistry(native);
auto* hive = new VReg::MemoryHive;
registry->Mount(HKEY_LOCAL_MACHINE, L"Software\\Vendor\\Game", *hive);
// Fill the hive through registry (or RegFile::Apply), then:
hive->Freeze();
VReg::Detours::Install(*registry, *native);
```

From then on the process's own `Reg*` calls see the mount. A frozen hive is
read-only and its readers take no lock; leave it writable to let the process
write into it.

- **Install early**, before other code hooks the registry functions, where
  you can. Later hookers chain on top and work, but a key handle opened
  before the install that points into a mount keeps reaching the real
  registry for calls made on it directly.
- **From `DllMain`:** build, fill, freeze and mount before installing, so the
  first hooked call already finds the hive.
- **Unloading** neutralizes the hooks (they pass everything through from
  then on); never unload while another thread may be inside a registry call.
- **Several modules** may each link VReg and install their own hooks; their
  handles never collide, and each answers only its own mounts.
- **MUI strings:** `RegLoadMUIString` never resolves a hive value's `@`
  resource reference (that would load a module) and answers
  `ERROR_INVALID_DATA`. A value stored with `Hive::SetMuiTextValue` answers
  with its text instead.

Code that runs inside a hook (a custom `Registry` used as a base, for
instance) may run under another module's loader lock: it must not load a
module, create a thread, or wait on a lock a loader-lock holder could own,
and it reaches the real registry only through the base registry, never
`::Reg*`.

Not covered: calls straight to `ntdll` (`NtOpenKey` and friends) bypass the
hooks. Where VReg cannot match native Windows exactly, the spec lists it
under "Known divergences".

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
