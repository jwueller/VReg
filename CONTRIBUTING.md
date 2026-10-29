# Contributing to VReg

## Architecture

[ARCHITECTURE.md](./ARCHITECTURE.md) owns how VReg is built: its
components, handles and locking, the Detours adapter, the `.reg` format and
the test suite. What is missing and where VReg knowingly differs from
native Windows are in the [README](./README.md).

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
`-release` to build and test. They enable the tests and `VReg::Detours`, with
Catch2 and Detours from vcpkg.

Native Windows behavior is the spec: a scenario that can run against the real
registry (a private app hive) runs against it too, and the virtual answer must
match it. CI also runs the tests under Wine, where those scenarios skip
themselves.

## The vcpkg port

This repository is the vcpkg registry for the `vreg` port. A change to
[ports/vreg](./ports/vreg) needs a new version entry (bump `version-semver`
or `port-version` first), committed after the port change itself, since it
records the port's git tree:

```
vcpkg x-add-version vreg --x-builtin-ports-root=ports --x-builtin-registry-versions-dir=versions
```

## Releasing

1. Bump [include/VReg/version.h](./include/VReg/version.h) (CMake reads the
   version from it) and [vcpkg.json](./vcpkg.json), commit, and push.
2. Wait for CI to pass on that commit, the Wine jobs included: local test
   runs cover neither Wine nor clang-cl. Tag only a commit CI passed on.
3. Tag it `vX.Y.Z` and push the tag.
4. Point the port at the release: set `version-semver` in
   [ports/vreg/vcpkg.json](./ports/vreg/vcpkg.json), and in
   [ports/vreg/portfile.cmake](./ports/vreg/portfile.cmake) the SHA512 of
   `https://github.com/jwueller/VReg/archive/vX.Y.Z.tar.gz`. Commit, add
   the version entry as above, commit, and push.
