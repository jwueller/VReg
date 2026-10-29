# VReg v1 implementation plan

Implements [the design](../specs/VReg-design.md). Each milestone ends green
on x86 and x64 and is committed before the next starts. Tests come first in
every task (red, then green); "oracle" below means the native-behavior
harness from M2.

## M0 -- Project skeleton

1. `CMakeLists.txt` (3.25+, C++20): `VReg::Core` (with `GetVersion`, so
   copies in one process can tell each other apart); `VReg::RegFile` and
   `VReg::Detours` (behind `VREG_WITH_DETOURS`, off) are declared by M6 and
   M7; `VREG_BUILD_TESTS` defaulting to "top-level project only".
2. `CMakePresets.json`: {x86, x64} x {MSVC, clang-cl}, Debug and Release.
3. `vcpkg.json` manifest with a `tests` feature (Catch2) and a `detours`
   feature. Agent's choice: vcpkg only for the project's own tests and the
   optional Detours target, so `add_subdirectory`/`FetchContent` consumers
   need nothing; vendoring Catch2 was passed over.
4. Install/export: `VRegConfig.cmake` with components `Core`, `RegFile`,
   `Detours`; a `find_package` smoke consumer under `test/consumer/`.
5. `LICENSE` (MIT), `.gitattributes`
   (`*.reg` binary, since UTF-16 fixtures must not be normalized),
   `.gitignore`.
6. `CONTRIBUTING.md` § "Building and testing": the build presets and how to run the tests.

Done when: every preset configures, builds and runs the version test and
the `find_package` consumer.

## M1 -- Handles

1. `IsVirtualHKey` and the index-to-handle encoding (31-bit range on both
   architectures); unit tests including predefined roots on x64.
2. The handle probe from the spec as a permanent test (every route listed
   there; bit 0 never set).
3. Shared handle namespace: the `Local\VReg-handles-<pid>` section, magic
   claim, block reservation, ownership check. Tests: two independent
   allocators in one process (simulating two static copies) never collide
   and each recognizes only its own blocks.

## M2 -- `Registry` interface, `NativeRegistry`, oracle harness

1. `Registry` with the wide virtuals listed in the spec; the ANSI and pre-`Ex`
   forms as adapters (`CP_ACP`), the ANSI forms virtual; the owned-value reads
   as free functions beside it.
2. `NativeRegistry` with its pointer table, resolved from the
   `KernelBase.dll` export table (`advapi32.dll` for the pre-`Ex` forms),
   not `GetProcAddress`.
3. Oracle harness: a fixture that loads a fresh app hive from a temp file
   (`RegLoadAppKey`) and runs one scenario against any `Registry` rooted
   there, recording each call's status, out-sizes and bytes.
4. Scenario catalogue, run against `NativeRegistry` only for now: open,
   create, set and query every value type, buffer too small, null buffers,
   `ERROR_MORE_DATA` sizes, ANSI and wide, enumeration to `ERROR_NO_MORE_ITEMS`,
   `QueryInfoKey` with each out-parameter null, delete key, value and tree.

## M3 -- `MemoryHive` and `VirtualRegistry`

1. `Hive` interface; `MemoryHive` (nodes, values, ordinal case-insensitive
   names, one handle per node, deleted nodes answering `ERROR_KEY_DELETED`).
2. `VirtualRegistry`: mount table (copy-on-write, atomic publish), optional
   base, the caller-buffer and ANSI/wide rules.
3. Run the M2 scenario catalogue against `VirtualRegistry` over a
   `MemoryHive` and require identical recordings to native.
4. Pure in-memory configuration (no base, `HKLM` and `HKCU` mounted):
   unmounted paths answer `ERROR_FILE_NOT_FOUND`.

## M4 -- `OverlayHive`, path resolution, frozen mounts

1. Overlay reads, value and key tombstones, opaque keys; merged enumeration
   and counts (index shifting, shadowed names stepped over, failed lower
   query never shrinking into success, class from the caller's width).
2. Overlay regression cases (shadowing, `QueryInfoKey` failure cases, ancestor handles,
   whole-path keys not enumerating under their root, `RegLoadMUIStringW`
   forwarding).
3. Frozen mounts: `Freeze()` per hive, writes refused, lower-only keys
   answered with the lower handle.
4. Native-path resolution through an injectable `NtQueryKey`, leaf-name
   gate, `WOW6432Node` stripping, the per-mount `view` option.

## M5 -- Writable mounts

1. Per-hive SRWLOCK; transparent nodes created lazily so every open inside a
   writable mount returns a virtual handle; copy-up on write.
2. Lower-registry calls made outside the lock (merged enumeration snapshots
   upper names first). A test with a lower registry stub that asserts the
   lock is not held when it is called.
3. `RegNotifyChangeKeyValue`: never signals on frozen hives, signals on
   writes in writable ones.
4. Concurrency stress test: readers and writers on several threads.

## M6 -- `.reg` (`VReg::RegFile`)

1. Parser: both headers and encodings, keys, deletions, every value form,
   continuations, comments; errors with line and column. Fuzz-ish table of
   malformed inputs.
2. `Apply` (two-phase, stops at the first failing operation and reports it);
   through an overlay, deletions become tombstones and opaque keys.
3. `ExportSnapshot` and `ExportDelta`: byte-exact round trips for every
   value type, the `hex(n)` fallbacks, deterministic order, regedit layout,
   `REGEDIT4` refusing unrepresentable data.
4. Check whether `regedit.exe /s` imports v5 UTF-8 (on HKCU, opt-in test);
   record the result in the README and the spec.

## M7 -- Detours adapter (`VReg::Detours`)

1. Thunks: `VirtualAlloc`'d `jmp [slot]` per hook, slot switching to the
   trampoline on unload or detach; nothing ever calls `DetourDetach`.
2. `Attach` (inside a consumer's transaction) and `Install` (own
   transaction); the `NativeRegistry` table fields handed to `DetourAttach`.
3. Hook set per the spec: emulated, refused-on-virtual, and
   `RegLoadMUIStringA`.
4. Hook test process (x86 and x64): installs, then drives the real `Reg*`
   APIs against a mounted hive and against passthrough keys.
5. Two DLLs, each linking the library statically, loaded into one test
   process: disjoint handles, chaining (each answers its own mounts),
   unloading the lower one leaves the upper working.

## M8 -- CI and documentation

1. GitHub Actions on Windows: {x86, x64} x {MSVC, clang-cl}, all tests and
   the `find_package` consumer.
2. Wine job on Ubuntu: runs the Windows-built test binaries under Wine; the
   oracle tests detect Wine and skip; the handle probe runs.
3. README: usage for the three configurations (overlay over native, pure in-memory, hooked), the detour-body rules, the
   documented gaps (pre-commit real handles, `ntdll` callers), the
   install-early recommendation.

## After v1 (not in this plan)

- DirectRetrofit migrates onto the library, in its own repository.
- vcpkg port with a `detours` feature.
