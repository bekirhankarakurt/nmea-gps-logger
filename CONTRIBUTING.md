# Contributing

Thanks for your interest in improving nmea-gps-logger.

## Development setup

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DNMEA_SANITIZE=ON -DNMEA_WARNINGS_AS_ERRORS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

No GNSS hardware is needed: every input path is covered by the replay fixture,
a pseudo-terminal (serial) and a UDP loopback test.

## Before opening a pull request

- Keep the build warning-free with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` on GCC and Clang.
- Add or update tests in `tests/` for every behaviour change; new sentence types need parser tests with real-world samples.
- Run the static analysers used in CI:

  ```bash
  cppcheck --enable=warning,style,performance,portability --std=c++17 --language=c++ \
    --error-exitcode=1 --inline-suppr --suppress=missingIncludeSystem --suppress=unusedFunction -I include src app
  cmake -S . -B build-tidy -DCMAKE_CXX_COMPILER=clang++ -DNMEA_BUILD_TESTS=OFF
  run-clang-tidy -p build-tidy -quiet 'src/|app/'
  ```

- If you change `tools/gen_fixture.py`, regenerate `tests/fixtures/metu_ride.nmea` and update the expected counts.
- Add an entry under `[Unreleased]` in `CHANGELOG.md`.

## Style

Modern C++17: RAII for every resource, no raw `new`/`delete`, `enum class`,
`[[nodiscard]]` on value-returning functions, `const` wherever possible. Four-space
indentation, 120-column lines. Commit messages are short imperative sentences
("Add GLL parser", not "Added GLL parser").
