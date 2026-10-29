# VReg

A virtualized Windows registry for C++. VReg lets a process see registry
keys that don't exist on the machine, override values in keys that do, or
run against a registry held entirely in memory, without touching the real
registry.

It grew out of the virtual registry in
[DirectRetrofit](https://retrofit.games), which is its first consumer.

## What it does

- **The Win32 `Reg*` function family as an interface.** `Registry` has
  every member of the family, names and signatures alike (ANSI and wide,
  `Ex` and pre-`Ex`), so the `UNICODE` macros select among its members as
  among the functions. Native, virtual and in-memory registries are
  interchangeable behind it. Reads into a `std::wstring` or
  `std::vector<std::byte>` are free functions on top.
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

### Overlay over the real registry

Keys and values added, overridden or deleted under one real key; everything
else is the real registry.

```cpp
VReg::NativeRegistry native;
VReg::OverlayHive overlay(native, HKEY_CURRENT_USER, L"Software\\Vendor\\Game");
VReg::VirtualRegistry registry(&native);
registry.Mount(HKEY_CURRENT_USER, L"Software\\Vendor\\Game", overlay);

const DWORD dwOne = 1;
registry.RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Vendor\\Game", L"Windowed", REG_DWORD, &dwOne, sizeof(dwOne));
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
write into it. Mounting a `const Hive&` makes the mount read-only while the
hive stays writable to whoever holds it.

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
- **Tracing:** `Install` takes any `Registry`, so a wrapper that logs each
  call and forwards it to the `VirtualRegistry` can stand in front of it.
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
hooks. Where VReg cannot match native Windows exactly, the
[design](./docs/specs/VReg-design.md) lists it under "Known
divergences".

## Supported functions

Each links to its Microsoft documentation; an ANSI and wide pair shares an
entry.

### Answered

By `Registry` (its members) and, once hooked, for every virtual key.

#### Open, create and close

- [`RegOpenKeyA`][RegOpenKeyA], [`RegOpenKeyW`][RegOpenKeyW]
- [`RegOpenKeyExA`][RegOpenKeyExA], [`RegOpenKeyExW`][RegOpenKeyExW]
- [`RegCreateKeyA`][RegCreateKeyA], [`RegCreateKeyW`][RegCreateKeyW]
- [`RegCreateKeyExA`][RegCreateKeyExA], [`RegCreateKeyExW`][RegCreateKeyExW]
- [`RegCloseKey`][RegCloseKey]

#### Values

- [`RegQueryValueA`][RegQueryValueA], [`RegQueryValueW`][RegQueryValueW]
- [`RegQueryValueExA`][RegQueryValueExA], [`RegQueryValueExW`][RegQueryValueExW]
- [`RegGetValueA`][RegGetValueA], [`RegGetValueW`][RegGetValueW]
- [`RegQueryMultipleValuesA`][RegQueryMultipleValuesA], [`RegQueryMultipleValuesW`][RegQueryMultipleValuesW]
- [`RegSetValueA`][RegSetValueA], [`RegSetValueW`][RegSetValueW]
- [`RegSetValueExA`][RegSetValueExA], [`RegSetValueExW`][RegSetValueExW]
- [`RegSetKeyValueA`][RegSetKeyValueA], [`RegSetKeyValueW`][RegSetKeyValueW]
- [`RegDeleteValueA`][RegDeleteValueA], [`RegDeleteValueW`][RegDeleteValueW]
- [`RegDeleteKeyValueA`][RegDeleteKeyValueA], [`RegDeleteKeyValueW`][RegDeleteKeyValueW]
- [`RegLoadMUIStringA`][RegLoadMUIStringA], [`RegLoadMUIStringW`][RegLoadMUIStringW]
  (`RegLoadMUIStringA` answers `ERROR_CALL_NOT_IMPLEMENTED`, as it does natively)

#### Keys

- [`RegDeleteKeyA`][RegDeleteKeyA], [`RegDeleteKeyW`][RegDeleteKeyW]
- [`RegDeleteKeyExA`][RegDeleteKeyExA], [`RegDeleteKeyExW`][RegDeleteKeyExW]
- [`RegDeleteTreeA`][RegDeleteTreeA], [`RegDeleteTreeW`][RegDeleteTreeW]
- [`RegFlushKey`][RegFlushKey]

#### Enumeration and information

- [`RegEnumKeyA`][RegEnumKeyA], [`RegEnumKeyW`][RegEnumKeyW]
- [`RegEnumKeyExA`][RegEnumKeyExA], [`RegEnumKeyExW`][RegEnumKeyExW]
- [`RegEnumValueA`][RegEnumValueA], [`RegEnumValueW`][RegEnumValueW]
- [`RegQueryInfoKeyA`][RegQueryInfoKeyA], [`RegQueryInfoKeyW`][RegQueryInfoKeyW]

#### Notification

- [`RegNotifyChangeKeyValue`][RegNotifyChangeKeyValue]

### Refused on virtual keys

They answer `ERROR_NOT_SUPPORTED` on a virtual key and pass through
unchanged on a real one. They are hooked but not part of `Registry`.

#### Hive files

- [`RegSaveKeyA`][RegSaveKeyA], [`RegSaveKeyW`][RegSaveKeyW]
- [`RegSaveKeyExA`][RegSaveKeyExA], [`RegSaveKeyExW`][RegSaveKeyExW]
- [`RegRestoreKeyA`][RegRestoreKeyA], [`RegRestoreKeyW`][RegRestoreKeyW]
- [`RegLoadKeyA`][RegLoadKeyA], [`RegLoadKeyW`][RegLoadKeyW]
- [`RegUnLoadKeyA`][RegUnLoadKeyA], [`RegUnLoadKeyW`][RegUnLoadKeyW]
- [`RegReplaceKeyA`][RegReplaceKeyA], [`RegReplaceKeyW`][RegReplaceKeyW]

#### Security

- [`RegGetKeySecurity`][RegGetKeySecurity]
- [`RegSetKeySecurity`][RegSetKeySecurity]

#### Structure

- [`RegRenameKey`][RegRenameKey]
- [`RegCopyTreeA`][RegCopyTreeA], [`RegCopyTreeW`][RegCopyTreeW]

#### Transactions

- [`RegCreateKeyTransactedA`][RegCreateKeyTransactedA], [`RegCreateKeyTransactedW`][RegCreateKeyTransactedW]
- [`RegOpenKeyTransactedA`][RegOpenKeyTransactedA], [`RegOpenKeyTransactedW`][RegOpenKeyTransactedW]
- [`RegDeleteKeyTransactedA`][RegDeleteKeyTransactedA], [`RegDeleteKeyTransactedW`][RegDeleteKeyTransactedW]

#### Reflection and overrides

- [`RegDisableReflectionKey`][RegDisableReflectionKey]
- [`RegEnableReflectionKey`][RegEnableReflectionKey]
- [`RegQueryReflectionKey`][RegQueryReflectionKey]
- [`RegOverridePredefKey`][RegOverridePredefKey]

### Not hooked

They take no key handle a virtual key could reach. The keys they return
are real; a later call on one reaches a mount only by its native path, as
for any real key, so nothing below `HKEY_CLASSES_ROOT` is reachable through
`RegOpenUserClassesRoot`.

- [`RegConnectRegistryA`][RegConnectRegistryA], [`RegConnectRegistryW`][RegConnectRegistryW]
- [`RegDisablePredefinedCache`][RegDisablePredefinedCache]
- [`RegDisablePredefinedCacheEx`][RegDisablePredefinedCacheEx]
- [`RegLoadAppKeyA`][RegLoadAppKeyA], [`RegLoadAppKeyW`][RegLoadAppKeyW]
- [`RegOpenCurrentUser`][RegOpenCurrentUser]
- [`RegOpenUserClassesRoot`][RegOpenUserClassesRoot]

[RegOpenKeyA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeya
[RegOpenKeyW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyw
[RegOpenKeyExA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexa
[RegOpenKeyExW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeyexw
[RegCreateKeyA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeya
[RegCreateKeyW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyw
[RegCreateKeyExA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexa
[RegCreateKeyExW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeyexw
[RegCloseKey]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regclosekey
[RegQueryValueA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluea
[RegQueryValueW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvaluew
[RegQueryValueExA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexa
[RegQueryValueExW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryvalueexw
[RegGetValueA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluea
[RegGetValueW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetvaluew
[RegQueryMultipleValuesA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesa
[RegQueryMultipleValuesW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regquerymultiplevaluesw
[RegSetValueA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluea
[RegSetValueW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvaluew
[RegSetValueExA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexa
[RegSetValueExW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetvalueexw
[RegSetKeyValueA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluea
[RegSetKeyValueW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeyvaluew
[RegDeleteValueA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluea
[RegDeleteValueW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletevaluew
[RegDeleteKeyValueA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluea
[RegDeleteKeyValueW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyvaluew
[RegLoadMUIStringA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringa
[RegLoadMUIStringW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadmuistringw
[RegDeleteKeyA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeya
[RegDeleteKeyW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyw
[RegDeleteKeyExA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexa
[RegDeleteKeyExW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeyexw
[RegDeleteTreeA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreea
[RegDeleteTreeW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletetreew
[RegFlushKey]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regflushkey
[RegEnumKeyA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeya
[RegEnumKeyW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyw
[RegEnumKeyExA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexa
[RegEnumKeyExW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumkeyexw
[RegEnumValueA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluea
[RegEnumValueW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenumvaluew
[RegQueryInfoKeyA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeya
[RegQueryInfoKeyW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryinfokeyw
[RegNotifyChangeKeyValue]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue
[RegSaveKeyA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsavekeya
[RegSaveKeyW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsavekeyw
[RegSaveKeyExA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsavekeyexa
[RegSaveKeyExW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsavekeyexw
[RegRestoreKeyA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regrestorekeya
[RegRestoreKeyW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regrestorekeyw
[RegLoadKeyA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadkeya
[RegLoadKeyW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadkeyw
[RegUnLoadKeyA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regunloadkeya
[RegUnLoadKeyW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regunloadkeyw
[RegReplaceKeyA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regreplacekeya
[RegReplaceKeyW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regreplacekeyw
[RegGetKeySecurity]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-reggetkeysecurity
[RegSetKeySecurity]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regsetkeysecurity
[RegRenameKey]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regrenamekey
[RegCopyTreeA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcopytreea
[RegCopyTreeW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcopytreew
[RegCreateKeyTransactedA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeytransacteda
[RegCreateKeyTransactedW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regcreatekeytransactedw
[RegOpenKeyTransactedA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeytransacteda
[RegOpenKeyTransactedW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenkeytransactedw
[RegDeleteKeyTransactedA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeytransacteda
[RegDeleteKeyTransactedW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdeletekeytransactedw
[RegDisableReflectionKey]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdisablereflectionkey
[RegEnableReflectionKey]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regenablereflectionkey
[RegQueryReflectionKey]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regqueryreflectionkey
[RegOverridePredefKey]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regoverridepredefkey
[RegConnectRegistryA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regconnectregistrya
[RegConnectRegistryW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regconnectregistryw
[RegDisablePredefinedCache]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdisablepredefinedcache
[RegDisablePredefinedCacheEx]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regdisablepredefinedcacheex
[RegLoadAppKeyA]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadappkeya
[RegLoadAppKeyW]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regloadappkeyw
[RegOpenCurrentUser]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopencurrentuser
[RegOpenUserClassesRoot]: https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regopenuserclassesroot

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md).

## License

[MIT](./LICENSE).
