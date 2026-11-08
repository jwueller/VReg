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
`-release` to build and test. They enable the tests and `VReg::Detours`,
with Catch2 and Detours from vcpkg.

The MSVC x64 job also builds the library with code analysis (`/analyze`),
whose findings fail the build like any warning. Configure with
`-DVREG_ANALYZE=ON` to run it locally.

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
