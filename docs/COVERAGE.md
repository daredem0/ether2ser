\page coverage_report Coverage Report

# Coverage Report

This project can embed unit test coverage into generated Doxygen HTML output.

- If docs were built with `BUILD_TESTS=ON` and `ENABLE_COVERAGE=ON`, open:
  - [Coverage HTML Report](coverage.html)
- If the file is not present, rebuild docs in coverage mode.

## Build Commands

```bash
cmake -S . -B build-tests -DBUILD_TESTS=ON -DENABLE_COVERAGE=ON
cmake --build build-tests --target docs
```

Then open:

```text
build-tests/docs/doxygen/html/index.html
```
