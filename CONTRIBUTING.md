# Contributing to VReg

## Where things are

The whole library is [vreg.h](./vreg.h): public declarations, the hooks'
declarations, private declarations, the implementation and the hooks'
implementation, each split into titled sections. The comment that opens an
implementation section owns the design of what it holds: the mechanism and
why it was chosen over the alternatives. The CMake targets compile the file
from the one-line sources under `src/`; they and the tests define
`VREG_INTERNAL`, which links the private functions they share. What is missing
and where VReg knowingly differs from native Windows are in the
[README](./README.md).

## Building and testing

Needs Visual Studio 2022 or later (MSVC or clang-cl), CMake 3.25+, Ninja and
vcpkg (`VCPKG_ROOT` set; the copy bundled with Visual Studio works). From a
developer prompt for the target architecture:

```
cmake --preset msvc-x64
cmake --build --preset msvc-x64-debug
ctest --preset msvc-x64-debug
```

Presets: `{msvc,clang-cl}-{x86,x64,arm64}` to configure; append `-debug` or
`-release` to build and test. They enable the tests and `VReg::Detours`,
with Catch2 and Detours from vcpkg.

The MSVC x64 job also builds the library with code analysis (`/analyze`),
whose findings fail the build like any warning. Configure with
`-DVREG_ANALYZE=ON` to run it locally.

## Tests

Catch2, under `test/`. Native Windows behavior is the spec: the oracle runs
each scenario against the real `Reg*` API on a private app hive
(`RegLoadAppKey` on a temp file: real, no admin, no machine state) and
against a virtual registry over an in-memory hive, and statuses, sizes and
bytes must match; a scenario skips what virtual keys cannot match for both
runs. The concurrency tests require only plausible statuses, no hang and a
final state that agrees with itself. The hooks are tested by a process per
architecture that installs them and calls the real API through two DLLs
that each link the library, which covers the shared handle namespace,
chaining and a pin outliving `FreeLibrary`. `test/headers` compiles the
implementation as C and the declarations as C++ after a plain
`#include <Windows.h>`, under every combination of `WIN32_LEAN_AND_MEAN`,
`UNICODE` and `NOMINMAX`, and for Vista and Windows 7 as `_WIN32_WINNT`.
`test/nocrt` links the core without the C runtime and runs a round trip,
which proves that `support.c` lists everything the library needs besides
Windows.

CI builds {x86, x64, ARM64} x {MSVC, clang-cl} and runs every test on
Windows, and the x86 and x64 binaries under Wine on Ubuntu, where the
scenarios on a private app hive skip themselves. Where Wine deviates, that
is a Wine bug, worked around only when the workaround costs nothing on
Windows: a per-platform expectation list would make VReg's behavior follow
whichever Wine version CI installed. Proton and true remote
`RegConnectRegistry` are manual checks.

## Packaging

Static libraries only: each module links its own copy and the copies meet
in the shared handle namespace, so a DLL would buy nothing, and two modules
shipping different builds of it would collide on its name. One target for
the core rather than one per area: nothing uses `.reg` support without the
core, and a static library costs a consumer only the objects it links.
`vreg.h` is the same code the libraries compile, one hand-maintained file
rather than sources merged into a generated header, so what a consumer
copies is what contributors edit; it carries the license text and a UTF-8
BOM, since it is copied on its own and MSVC would otherwise read that text
in the consumer's code page. Tests build only when VReg is the top-level
project, and Detours is found as the vcpkg port lays it out.

## The vcpkg port

This repository is the vcpkg registry for the `vreg` port. A change to
[ports/vreg](./ports/vreg) needs a new version entry (bump `version-semver`
or `port-version` first), committed after the port change itself, since it
records the port's git tree:

```
vcpkg x-add-version vreg --x-builtin-ports-root=ports --x-builtin-registry-versions-dir=versions
```

## Releasing

`VREG_VERSION_MAJOR`, `VREG_VERSION_MINOR`, `VREG_VERSION_PATCH` and
`VREG_VERSION_TWEAK` in [vreg.h](./vreg.h) are the version and the one
place it is written: `VREG_VERSION` packs them as `MAKEDLLVERULL` does, and
CMake takes its project version from them (`cmake/VRegVersion.cmake`).
vreg.h is copied straight from the repository, so every commit that
changes it raises its version as semver does, and the header is always
exactly its version: the patch version for a compatible change, the minor
version for a new feature. A breaking change raises the major version from
1.0 on; until then the API is unstable and a breaking change raises the
minor version, as the C API did when it moved from 0.4.1 to 0.5.0.
`VREG_VERSION_TWEAK` stays 0: the fourth part exists only because the
packed form has four. A tag `vX.Y.Z` and the port publish some of these
versions, not all. CI checks that a change to vreg.h raises the version,
that a release tag names `X.Y.Z`, that the port names a tagged release, and
that `versions/` is up to date with the port.

1. Pick the commit to release; its vreg.h names the version. Push it.
2. Wait for CI to pass on that commit, the Wine jobs included: local test
   runs do not cover Wine. Tag only a commit CI passed on.
3. Tag it `vX.Y.Z` and push the tag. Once CI passes on the tag, it
   publishes the GitHub release, its notes the commit subjects since the
   previous tag, with the tag's `vreg.h` attached.
4. Point the port at the release: set `version-semver` in
   [ports/vreg/vcpkg.json](./ports/vreg/vcpkg.json), and in
   [ports/vreg/portfile.cmake](./ports/vreg/portfile.cmake) the SHA512 of
   `https://github.com/jwueller/VReg/archive/vX.Y.Z.tar.gz`. Commit, add
   the version entry as [The vcpkg port](#the-vcpkg-port) describes,
   commit, and push.
