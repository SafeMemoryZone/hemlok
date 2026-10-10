# Hemlok

Simple C++23 sampling profiler with Dear ImGui.

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
area. To format tracked sources, run `./scripts/format.sh`, review the changes, and stage
them before committing again.

## Continuous integration

The CI workflow runs on pushes to `main` and pull requests targeting `main`,
with two parallel jobs:

- **CI / Build and test** builds in Debug mode with GCC 14 on Ubuntu and runs
  CTest. Build failures, failing tests, and an empty test suite fail the check.
- **CI / Check formatting** checks all tracked C/C++ sources and headers
  against `.clang-format` without modifying them. It uses Homebrew's current
  `clang-format` on macOS because the configuration requires recent options.
  The formatter version is printed in the workflow log for local reproduction.

Push workflows report failures after a push has been accepted; they cannot undo
or reject that push. To block failing changes from entering `main`, configure a
GitHub branch protection rule or ruleset for `main` that requires pull requests
and the **Build and test** and **Check formatting** status checks to pass before
merging. These repository settings are separate from the workflow files.

## License

Licensed under the [MIT License](LICENSE).
