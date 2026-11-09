# Binary hive files

Not in scope yet. This records how `regf` support would fit, so the door
stays open while `.reg` scripts remain the only import and export.

## What the format holds

A hive file is what `reg save` writes and `RegLoadAppKey` reads: a base
block (`regf` signature, two sequence numbers, a timestamp, the root cell's
offset, the data size, a checksum) followed by 4 KiB-aligned bins (`hbin`)
of cells. Cells are key nodes (`nk`: name, class, last write, flags, the
offsets of its subkey list, value list and security), value keys (`vk`:
name, type, data or, for up to four bytes, the data inline), subkey lists
(`lf`/`lh` with name hashes, `li` plain, `ri` of lists), big data (`db`,
for values over 16344 bytes) and security descriptors (`sk`, shared). Names
are compressed to one byte per character when every character is Latin-1.
Transaction logs (`.LOG1`, `.LOG2`) matter only for hives written in place;
a file written whole and never reopened for writing needs none.

Everything an in-memory hive holds maps: keys with class names and last
write times, values of every type and size. Two things do not: security
descriptors, which VReg has no notion of and would read past and write as
one default descriptor for every key, and the tombstones of an overlay,
which no snapshot format can carry.

## Shape

Hive files are a hive concern, not a script one. A script is a sequence of
operations, deletions included, which `VRegScript_CreateDelta` needs; a hive
file is a snapshot of a subtree, which is what a hive is. So:

- `VRegHive_CreateFromFile(path, &hive)`: an in-memory hive loaded from a
  file, with class names and last write times kept.
- `VRegHive_SaveFile(hive, path)`: the hive's content as a file; an overlay
  writes what it answers, since tombstones cannot be written.
- With those, the hive-file functions the hooks refuse today can answer on
  virtual keys: `RegSaveKey` and `RegSaveKeyEx` save the key's subtree,
  `RegLoadAppKey` loads a file into a hive mounted as a private root,
  `RegLoadKey` and `RegUnLoadKey` mount and unmount one below a predefined
  root, `RegRestoreKey` replaces a key's subtree from a file, and
  `RegReplaceKey` stays refused (it schedules a swap for the next boot).
  Each of these gets a slot in `VRegVtbl` then, since a custom registry
  must be able to answer them too.

Passed over: a `VRegFormat` value for `VRegScript_ParseFile` and
`VRegScript_WriteFile`, which would reuse the script pipeline but has to
refuse every script that deletes, and would make a snapshot look like a
list of operations.

## Where it goes

- `vreg.h`: a titled area "Hive files" in the public part after the hives,
  and the matching implementation section after the `.reg` one; the reader
  and writer are private `VRegInternalHiveFile_*` functions over a
  `VRegInternalHiveFile` cursor, as the `.reg` parser is over
  `VRegScriptInternalParser`.
- README: the two functions beside the `.reg` ones, and the hive-file
  functions move out of "Refused on virtual handles".
- Tests: native Windows is the oracle without privileges. A file VReg
  writes is loaded with `RegLoadAppKeyW` and enumerated through the real
  API; a file the test makes natively (`RegLoadAppKeyW` on a fresh path,
  then writes, then close) is read by VReg and compared with the real
  registry's view of it. Round trips through both hold byte-for-byte
  content equality, not file equality: cell layout is free.
- Wine: its `regf` reader and writer exist (`RegLoadAppKey` is a stub, so
  those scenarios skip there, as the app-hive ones do).

## Size

Reader and writer are each a few hundred lines of C: cell walking with
bounds checks against a hostile file, the name compression, the `lh` hash
(`hash = hash * 37 + towupper(ch)`), big data, and the header checksum (XOR
of the first 508 bytes as 32-bit words).
