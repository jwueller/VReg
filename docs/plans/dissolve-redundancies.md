# Dissolve architecture redundancies

Approved 2026-11-05. One commit per item, each building and passing the
suite on msvc-x64 Debug before the next starts. Items are ordered so the
mechanical build changes land before the API changes, and the hive merge
(the largest) comes after the small API changes it would otherwise
conflict with.

- [x] 1. One predefined-key predicate: `IsPredefinedHKey` in `handle.h`,
  replacing the copies in `registry.cpp` and `virtual_registry.cpp`.
- [x] 2. One private include convention: `"core/x.h"` and `"regfile/x.h"`
  from `src/`, for the library sources and the amalgamator alike.
- [x] 3. One library target `VReg::VReg` in place of `VReg::Core` and
  `VReg::RegFile`; `VReg::Detours` stays separate for its dependency.
  Consumers, install, config, tests, port usage, README and ARCHITECTURE
  follow.
- [x] 4. One amalgamation CMake file (script and target in
  `VRegAmalgamate.cmake`, as `VRegVersion.cmake` already does), and the
  version emitted where `version_info.h` is included instead of as macros
  at the top and `#undef`s at the bottom.
- [x] 5. One WOW64 view vocabulary: `VirtualRegistry::Mount` takes a
  `REGSAM` view (`KEY_WOW64_64KEY`, `KEY_WOW64_32KEY` or 0) like the
  `OverlayHive` constructor; `MountView` goes.
- [x] 6. One hive class: `MemoryHive` and `OverlayHive` become one concrete
  `Hive` with an optional lower registry; the `Hive` interface, `Tree` as a
  separate class and the overlay-only flags' qualifiers go with them.
- [ ] 7. `NativeRegistry::Functions` holds only the functions
  `NativeRegistry` calls; the Detours adapter keeps the trampoline storage
  for the rest, and the test oracle its own full table.
- [ ] 8. Drop `VirtualRegistry::Mount(const Hive&)` unless a consumer
  needs a mount that is read-only through one registry while the hive
  stays writable elsewhere.
