# libmaelir
libmaelir is a support library for esp32 development, also targeting unittest and
qt builds. It's used by https://github.com/SimonKagstrom/maelir currently, but maybe
others in the future.

## Building
Only the unit tests can be built separately:

```
conan install -of libmaelir_conan --build=missing -s build_type=Debug conanfile.txt
cmake -B libmaelir_unittest -GNinja -DCMAKE_PREFIX_PATH="`pwd`/libmaelir_conan/build/Debug/generators/" -DCMAKE_BUILD_TYPE=Debug test/unittest
```

## Linking
To use from other project, use `add_subdirectory()` from your `CMakeLists.txt`, adding
either `qt/`, `esp32/` or the `test/` directories.
