# VReg

A virtual Windows registry for C, as an overlay over the real one, or held
entirely in memory. A process sees keys that don't exist on the machine and
values added, overridden or deleted in keys that do, while the real
registry stays unchanged.

It grew out of the virtual registry in [DirectRetrofit](https://retrofit.games).

## What it does

- **The Win32 `Reg*` function family as an interface.** A `VReg` is an
  opaque registry handle; the flat `VReg_*` functions take it first and
  mirror every member of the family, names and signatures alike, and the
  `UNICODE` macros select among them as among the functions.
- **Hives mounted into a virtual namespace.** A virtual registry mounts
  hives at registry paths and passes everything else to a base registry
  (usually the real one).
- **Your own data as a hive.** A table of slots over an object of yours
  shows a data structure, or values computed on each read, as registry
  keys.
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

## Registries and hives

A `VReg` is behavior. It answers the whole `Reg*` API: every form, ANSI
and wide, caller buffers and `ERROR_MORE_DATA`, predefined roots, WOW64
views, paths and handles. It is what a caller, or the hooks, talk to.
Three kinds: `VReg_CreateNative` is the real registry, `VReg_CreateVirtual`
mounts hives over an optional base, and `VReg_CreateCustom` is yours, a
table with a slot per form, for tracing or filtering.

A `VRegHive` is content. It is a tree of keys and values below one root,
with none of those rules: it knows no `HKEY_LOCAL_MACHINE`, no buffers and
no views, and becomes visible only by mounting into a virtual registry,
which supplies every Win32 rule on its behalf. Three kinds:
`VRegHive_CreateInMemory` starts empty, `VRegHive_CreateOverlay` adds,
overrides and deletes over a key of another registry, and
`VRegHive_CreateCustom` is yours, a table of slots that answer data and
nothing else.

Supply a hive when you have data. Supply a registry when you must change
how the API behaves.

## Using it

Needs a C99 compiler (MSVC 2013 or later, clang-cl, or MinGW-w64 GCC
[as a single header](#as-a-single-header); the headers compile as C++ too)
and `_WIN32_WINNT` at `0x0600` (Windows Vista) or later. Its
one dependency is `Windows.h`: no C runtime beyond the `memcpy`, `memmove`
and `memset` the compiler calls, and `VREG_MALLOC`, `VREG_REALLOC` and
`VREG_FREE` replace the allocator (the defaults use the process heap).
`test/nocrt` links without a C runtime, and its
[`CMakeLists.txt`](./test/nocrt/CMakeLists.txt) lists the compiler features
to switch off for that, such as `/forceInterlockedFunctions-` with MSVC for
ARM64. The whole library is one file,
[`vreg.h`](./vreg.h): copy it [as a single header](#as-a-single-header) and
no build system is needed. The CMake targets need CMake 3.25 or later.

CMake targets (static libraries, both included as `<vreg.h>`):
- `VReg::VReg`: the library: the function table, the registries, the hives,
  the handles, and `.reg` import and export.
- `VReg::Detours`: the hooks. Enable it with `-DVREG_WITH_DETOURS=ON`;
  needs Detours. Linking it defines `VREG_WITH_DETOURS`, which declares
  the hooks.

Consume it through `add_subdirectory`/`FetchContent` (VReg then installs
nothing with your project unless you set `VREG_INSTALL`), or install it and use
`find_package(VReg CONFIG)`, with `COMPONENTS Detours` for the hooks.

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

### As a single header

[`vreg.h`](./vreg.h) is the whole library, and each
[release](https://github.com/jwueller/VReg/releases) attaches it. Include it
anywhere, from C or C++; in exactly one C file per module, compile the
library there:

```c
#define VREG_IMPLEMENTATION
#include "vreg.h"
```

It links `advapi32` by itself. `VREG_STATIC`, defined wherever it is
included, makes every function `static`, so a module can hold a private
copy beside another's. For the hooks, define `VREG_WITH_DETOURS` wherever it
is included, with `<detours/detours.h>` on the include path and the Detours
library linked.

MinGW-w64 GCC compiles the single header without the hooks; the CMake
targets need MSVC or clang-cl. Link `advapi32` yourself there, and pass
`-Wno-unknown-pragmas` under `-Wall`: the header's MSVC pragmas are unknown
to GCC.

### Overlay over the real registry

Keys and values added, overridden or deleted under one real key; everything
else is the real registry.

```c
VReg* native;
VReg* registry;
VRegHive* overlay;
VReg_CreateNative(&native);
VRegHive_CreateOverlay(native, HKEY_CURRENT_USER, L"Software\\Vendor\\Game", 0, &overlay);
VReg_CreateVirtual(native, &registry);
VReg_Mount(registry, HKEY_CURRENT_USER, L"Software\\Vendor\\Game", overlay, 0);

const DWORD dwOne = 1;
VReg_RegSetKeyValueW(registry, HKEY_CURRENT_USER, L"Software\\Vendor\\Game", L"Windowed", REG_DWORD, &dwOne, sizeof(dwOne));
```

Writes stay in memory. `VRegScript_CreateDelta` collects them as a `.reg` script
whose operations `VRegScript_WriteFile` saves and `VReg_ApplyOperations`
replays later. Every status is the `LSTATUS` the native function answers;
the creation functions answer `ERROR_OUTOFMEMORY` when memory ran out.

The key is named twice, by the hive as its lower key and by the mount as
its path: one hive can be mounted in several registries, so the lower key
stays the hive's own.

### Held entirely in memory

```c
VRegHive* machine;
VRegHive* user;
VReg* registry;
VRegHive_CreateInMemory(&machine);
VRegHive_CreateInMemory(&user);
VReg_CreateVirtual(NULL, &registry);   // no base: nothing else exists
VReg_Mount(registry, HKEY_LOCAL_MACHINE, NULL, machine, 0);
VReg_Mount(registry, HKEY_CURRENT_USER, NULL, user, 0);
```

### Supplying a hive

A table of slots answers what a hive holds, on demand. This one is a single
key with one value, computed on each read:

```c
static LSTATUS WINAPI OpenKey(VRegHive* hive, VRegHiveKey* parent, LPCWSTR name, void** token)
{
    *token = NULL;
    return ERROR_FILE_NOT_FOUND;   // no subkeys
}

static LSTATUS SetTicks(VRegHiveResult* result)
{
    const DWORD ticks = GetTickCount();
    return VRegHiveResult_SetValue(result, REG_DWORD, (const BYTE*)&ticks, sizeof(ticks));
}

static LSTATUS WINAPI QueryValue(VRegHive* hive, VRegHiveKey* key, LPCWSTR name, VRegHiveResult* result)
{
    return VReg_CompareNames(name, L"Ticks") == 0 ? SetTicks(result) : ERROR_FILE_NOT_FOUND;
}

static LSTATUS WINAPI EnumKey(VRegHive* hive, VRegHiveKey* key, DWORD index, VRegHiveResult* result)
{
    return ERROR_NO_MORE_ITEMS;
}

static LSTATUS WINAPI EnumValue(VRegHive* hive, VRegHiveKey* key, DWORD index, VRegHiveResult* result)
{
    if (index != 0)
        return ERROR_NO_MORE_ITEMS;
    const LSTATUS status = VRegHiveResult_SetName(result, L"Ticks");
    return status == ERROR_SUCCESS ? SetTicks(result) : status;
}

static const VRegHiveVtbl CLOCK = {.OpenKey = OpenKey, .QueryValue = QueryValue, .EnumKey = EnumKey, .EnumValue = EnumValue};

VRegHive* clock;
VReg* registry;
VRegHive_CreateCustom(&CLOCK, NULL, &clock);   // the context: VRegHive_GetContext
VReg_CreateVirtual(NULL, &registry);
VReg_Mount(registry, HKEY_CURRENT_USER, L"Software\\Clock", clock, 0);
```

`OpenKey`, `QueryValue`, `EnumKey` and `EnumValue` are required. VReg opens
and creates one name at a time and keeps a record of each key it reached,
which a slot receives as its `VRegHiveKey`. Results go through the
`VRegHiveResult_Set*` functions, which copy, so a computed value can live on
the stack. What a null slot answers, how `DeleteTree` and `QueryInfo` are
derived, and the rules on threads, names and deletion are the doc comment
on `VRegHiveVtbl` in `vreg.h`. A change the provider makes on its own fires
watches through `VRegHive_NotifyChange`.

### Hooked into the process

```c
// Never destroyed: the hooks use them until the process ends.
VReg* native;
VReg* registry;
VRegHive* hive;
VReg_CreateNative(&native);
VReg_CreateVirtual(native, &registry);
VRegHive_CreateInMemory(&hive);
VReg_Mount(registry, HKEY_LOCAL_MACHINE, L"Software\\Vendor\\Game", hive, 0);
// Fill the hive through registry (or VReg_ApplyOperations), then:
VRegHive_Freeze(hive);
VRegHooks_Install(registry, native);
```

From then on the process's own `Reg*` calls see the mount. A frozen hive is
read-only and its readers take no lock; leave it writable to let the process
write into it.

- **Install early**: the install SHOULD come before other code hooks the
  registry functions. Later hookers chain on top and work, but a key handle
  opened before the install that points into a mount keeps reaching the real
  registry for calls made on it directly.
- **From `DllMain`:** build, fill, freeze and mount before installing, so the
  first hooked call already finds the hive.
- **Unloading** is never safe: pin every module a hook can reach, right
  after installing, as the doc comment on `VRegHooks_Install` says.
- **Several modules** MAY each link VReg and install their own hooks; their
  handles never collide, and each answers only its own mounts.
- **Tracing:** `VRegHooks_Install` takes any `VReg`, so a table
  that logs each call and forwards it to the virtual registry can stand in
  front of it; a form it leaves null reaches it as the wide `Ex` form.
- **MUI strings:** `RegLoadMUIString` resolves an `@` indirect string in a
  hive value as native does.

Code that runs inside a hook (a registry table of your own used as a base,
for instance) may run under another module's loader lock, so the rule in the
doc comment on `VReg` binds it, and it MUST reach the real registry only
through the native registry below the hooks, never the `Reg*` functions
themselves.

Calls straight to `ntdll` (`NtOpenKey` and friends) bypass the hooks; see
[Scope](#scope) and
[Known divergences from native Windows](#known-divergences-from-native-windows).

## Supported functions

As flat functions (`VReg_RegOpenKeyExW` and so on, the registry first) and
table slots, and, once hooked, for every virtual key.

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
unchanged on a real one. They are hooked but have no slot in
`VRegVtbl`.

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
- Binary hive files (`regf`, what `reg save` writes and `RegLoadAppKey`
  reads): loading a hive from one, saving a hive to one, and with that
  answers for the hive-file functions the hooks refuse. `.reg` scripts are
  the import and export for now; `docs/plans/hive-files.md` sketches the
  door.
- A diagnostic sink: an optional function pointer, off by default and bound
  by the detour-body rules, reporting what VReg decides. Call tracing needs
  no sink: a registry table installed in front of the virtual registry
  sees every call.

## Known divergences from native Windows

### ANSI forms

Measured by the oracle; each is an input no sane caller relies on.

- Strings set without a terminator: native `RegSetValueExW` and
  `RegSetValueExA` read one character past `cbData` and store an extra
  terminator when it is zero. Ours never reads past `cbData`: the wide form
  stores exactly the bytes given, and the ANSI form always terminates. For
  `REG_SZ` data passed to `RegSetValueExA` without its terminator, native's
  stored size depends on the byte after the buffer, which varies with memory
  layout; VReg always stores the terminator.
- `RegGetValueA` on a string with an odd byte count: native keeps the stray
  byte in the buffer tail.
- `RegQueryMultipleValuesA` with a buffer that holds some narrowed values
  but not all: native answers `ERROR_INVALID_DATA` after a partial fill; ours
  answers `ERROR_MORE_DATA`.
- `RegEnumValueA` with a too-small name buffer: native may write a
  terminator past `cbData`; ours stays in bounds.
- `RegQueryValueA` on a key without a default value and a zero-size buffer:
  native writes the terminator into the buffer; ours stays in bounds and
  still answers success with a size of 1.
- Null pointers on which native faults (`RegEnumKeyExA` and `RegEnumValueA`
  names, `RegQueryMultipleValues` lists and buffers) answer an error code.

### Virtual keys

Measured by running the oracle catalogue against a virtual registry over
an in-memory hive; the scenarios skip exactly these calls for the virtual run
(and for its native reference).

- Access rights are per node, not per open: a key opened `KEY_READ` can
  still be written through its handle, and `KEY_QUERY_VALUE` is never
  checked. (A zero `samDesired` is refused on open, as native does.)
- Handle identity: two opens of a node return the same handle, and an `Ex`
  open or create of an empty subkey returns the parent's own handle where
  native returns a new one. A hive keeps a record of every key opened until
  it is destroyed, so a provider over an unbounded key space grows by one
  record per distinct key opened.
- A provider that enumerates in another order than native (subkeys by
  name, values in creation order) diverges in that order.
- `REG_OPTION_BACKUP_RESTORE` opens succeed; native needs
  `SeBackupPrivilege` and answers `ERROR_ACCESS_DENIED` without it.
- `REG_OPTION_CREATE_LINK` creates an ordinary key.
- `RegQueryInfoKey` reports a security descriptor size of 0.
- Through an overlay, an entry the upper layer shadows or tombstones carries
  the letter case of the first call that named it, not the lower key's case
  (a name is found without regard to case, but enumerated as stored).
- Only calls on a real handle that name a mount point's last component are
  resolved by native path; a mount below `HKEY_CLASSES_ROOT` and the other
  roots without a native path is not reachable through a real parent handle.
- A call by predefined root and path finds a mount by comparing path
  components literally, so opening `Software\WOW6432Node\X` does not reach a
  32-bit mount at `Software\X`; only a real handle's native path does. A mount
  at a `WOW6432Node` path is reached by native path through the 32-bit view
  only, and `VReg_Mount` refuses two mounts of one real key for the same view
  however they are spelled.

### Transactions

Mounts take no part in Kernel Transaction Manager transactions. A
transacted create, open or delete answers `ERROR_NOT_SUPPORTED` where native
would act on a virtual key inside the transaction: through its virtual
handle, or through a real handle whose subkey path leads into a mount.

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md).

## License

[MIT](./LICENSE).
