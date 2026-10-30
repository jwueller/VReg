# VReg

A virtual Windows registry for C++, as an overlay over the real one, or held
entirely in memory. A process sees keys that don't exist on the machine and
values added, overridden or deleted in keys that do, while the real
registry stays unchanged.

It grew out of the virtual registry in [DirectRetrofit](https://retrofit.games).

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

Windows only (x86, x64 and ARM64), and runs under Wine and Proton.

## Using it

Needs C++20 (MSVC 2022 or later, or clang-cl) and CMake 3.25 or later.

CMake targets (static libraries):
- `VReg::Core`: the interface, the registries, the hives and the handles.
- `VReg::RegFile`: `.reg` import and export.
- `VReg::Detours`: the hooks. Enable it with `-DVREG_WITH_DETOURS=ON`;
  needs Detours.

Consume it through `add_subdirectory`/`FetchContent` (VReg then installs
nothing with your project unless you set `VREG_INSTALL`), or install it and use
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

The hooks on ARM64 need a Detours newer than 4.0.1 (vcpkg's `detours`
2025-06-20 or later, so a vcpkg baseline that has it): 4.0.1 overwrites more
of each ARM64 function it hooks than it relocates, and every call passed
through to the real registry crashes.

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
  then on); the module MUST NOT unload while another thread may be inside
  a registry call.
- **Several modules** may each link VReg and install their own hooks; their
  handles never collide, and each answers only its own mounts.
- **Tracing:** `Install` takes any `Registry`, so a wrapper that logs each
  call and forwards it to the `VirtualRegistry` can stand in front of it.
- **MUI strings:** `RegLoadMUIString` never resolves a hive value's `@`
  resource reference (that would load a module) and answers
  `ERROR_INVALID_DATA`. A value stored with `Hive::SetMuiTextValue` answers
  with its text instead.

Code that runs inside a hook (a custom `Registry` used as a base, for
instance) may run under another module's loader lock: it MUST NOT load a
module, create a thread, or wait on a lock a loader-lock holder could own,
and it MUST reach the real registry only through the base registry, never
`::Reg*`.

Calls straight to `ntdll` (`NtOpenKey` and friends) bypass the hooks; see
[Scope](#scope) and
[Known divergences from native Windows](#known-divergences-from-native-windows).

## Supported functions

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

### Refused on virtual handles

They answer `ERROR_NOT_SUPPORTED` on a virtual handle and pass through
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

They also answer `ERROR_NOT_SUPPORTED` on a real handle whose subkey path
leads into a mount.

#### Reflection and overrides

- [`RegDisableReflectionKey`][RegDisableReflectionKey]
- [`RegEnableReflectionKey`][RegEnableReflectionKey]
- [`RegQueryReflectionKey`][RegQueryReflectionKey]
- [`RegOverridePredefKey`][RegOverridePredefKey]

### Not hooked

They take no key handle, so they cannot reach a virtual key. The handles
they return are real; a later call on one reaches a mount only by its
native path, as for any real handle, so nothing below `HKEY_CLASSES_ROOT`
is reachable through `RegOpenUserClassesRoot`.

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

## Scope

### Non-goals

- Calls made straight to `ntdll` (`NtOpenKey` and friends); they bypass the
  hooks.
- Persistence beyond explicit import/export calls (no file-backed hives, no
  automatic flush).
- Hive *file* parsing (`.dat`, `.hiv`); the native registry is reached only
  through its public API.

### Missing

- ARM64EC builds. Its emulated x64 callers and native callers reach the
  `Reg*` functions through separate entry points, which would each need
  hooks.
- Answers on virtual keys for the functions the hooks refuse there (see
  [Refused on virtual handles](#refused-on-virtual-handles));
  `RegRenameKey` and `RegCopyTree` are the ones a hive could answer most
  readily.
- A diagnostic sink: an optional function pointer, off by default and bound
  by the detour-body rules, reporting what VReg decides. Call tracing needs
  no sink: a `Registry` wrapper installed in front of the `VirtualRegistry`
  sees every call.

## Known divergences from native Windows

### ANSI forms

Measured by the oracle; each is an input no sane caller relies on.

- Strings set without a terminator: native `RegSetValueExW` and
  `RegSetValueExA` read one character past `cbData` and store an extra
  terminator when it is zero. Ours never reads past `cbData`: the wide form
  stores exactly the bytes given, and the ANSI form always terminates.
- `RegGetValueA` on a string with an odd byte count: native keeps the stray
  byte in the buffer tail.
- `RegQueryMultipleValuesA` with a buffer that holds some narrowed values
  but not all: native answers `ERROR_INVALID_DATA` after a partial fill; ours
  answers `ERROR_MORE_DATA`.
- `RegEnumValueA` with a too-small name buffer: native may write a
  terminator past `cbData`; ours stays in bounds.
- Null pointers on which native faults (`RegEnumKeyExA` and `RegEnumValueA`
  names, `RegQueryMultipleValues` lists and buffers) answer an error code.

### Virtual keys

Measured by running the oracle catalogue against `VirtualRegistry` over a
`MemoryHive`; the scenarios skip exactly these calls for the virtual run
(and for its native reference).

- Access rights are per node, not per open: a key opened `KEY_READ` can
  still be written through its handle, and `KEY_QUERY_VALUE` is never
  checked. (A zero `samDesired` is refused on open, as native does.)
- Handle identity: two opens of a node return the same handle, and an `Ex`
  open or create of an empty subkey returns the parent's own handle where
  native returns a new one.
- `REG_OPTION_BACKUP_RESTORE` opens succeed; native needs
  `SeBackupPrivilege` and answers `ERROR_ACCESS_DENIED` without it.
- `REG_OPTION_CREATE_LINK` creates an ordinary key.
- `RegQueryInfoKey` reports a security descriptor size of 0.
- `RegLoadMUIString` answers `ERROR_INVALID_DATA` for a hive value whether
  or not it is an `@` indirect string; resolving one would load a resource
  module, which a detour body MUST NOT do. A value stored with
  `Hive::SetMuiTextValue` instead stands for the text an indirect string
  resolves to: it answers with that text, sized and truncated as native
  answers a resolved string, and ignores `pszDirectory` (there is no module
  to find).
- Through an overlay, an entry the upper layer shadows or tombstones carries
  the letter case of the first call that named it, not the lower key's case
  (a name is found without regard to case, but enumerated as stored).
- Only calls on a real handle that name a mount point's last component are
  resolved by native path; a mount below `HKEY_CLASSES_ROOT` and the other
  roots without a native path is not reachable through a real parent handle.

### Transactions

Mounts take no part in Kernel Transaction Manager transactions. A
transacted create, open or delete answers `ERROR_NOT_SUPPORTED` where native
would act on a virtual key inside the transaction: through its virtual
handle, or through a real handle whose subkey path leads into a mount.

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md).

## License

[MIT](./LICENSE).
