# Hemlok

Simple C++20 sampling profiler with Dear ImGui.

## Building

```sh
cmake -S . -B build
cmake --build build
```

Public headers live in `include/` and can be included by their project-relative
path, for example `#include <process/process.hpp>`.

## Testing

Tests use Catch2 and are built by default:

```sh
ctest --test-dir build --output-on-failure
```

Add new test source files under `tests/`; CMake discovers `*.cc` files in that
directory automatically. Configure with `-DHEMLOK_BUILD_TESTS=OFF` to skip the
test dependency and test target.

## Formatting

Install `clang-format` and enable the pre-commit hook once per clone:

```sh
git config core.hooksPath .githooks
```

The hook checks staged C/C++ sources and headers against `.clang-format` and
blocks commits with formatting errors. It does not modify files or the staging
area. To format tracked sources, run `./format.sh`, review the changes, and stage
them before committing again.

## License

Licensed under the [MIT License](LICENSE).
