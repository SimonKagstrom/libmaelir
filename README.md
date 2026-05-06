# libmaelir
libmaelir is a support library for esp32 development, also targeting unittest and
qt builds. It's used by https://github.com/SimonKagstrom/maelir and
https://github.com/SimonKagstrom/radbuzz currently, but maybe others in the
future.

## Building
Only the unit tests can be built separately:

```
cmake -GNinja -B radbuzz_unittest <path-to-libmaelir>/test/unittest/
```

## Linking
To use from other project, use `add_subdirectory()` from your `CMakeLists.txt`, adding
either `qt/`, `esp32/` or the `test/` directories.
