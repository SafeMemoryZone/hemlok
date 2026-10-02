#!/usr/bin/env bash
set -e

echo "Running clang-format..."

git ls-files -z \
    '*.c' '*.cc' '*.cpp' '*.cxx' \
    '*.h' '*.hh' '*.hpp' '*.hxx' |
    xargs -0 -r clang-format -i

echo "Formatting complete."
