# GitHub Copilot Instructions for activemq-cpp

## Coding Standards and Constraints

### Concurrency and Threading
- **DO NOT use `std::recursive_mutex`** - The codebase uses `std::mutex` for all synchronization primitives
- Use `std::mutex` and `std::condition_variable` from the C++ standard library
- The `decaf::util::concurrent::Mutex` class is a non-recursive wrapper around `std::mutex`
- Avoid implementing recursive locking semantics manually - the mutex design is intentionally non-recursive

### C++ Standard
- This project uses **C++17**
- Prefer standard library features over custom implementations where appropriate

## Building the Project

### Using CMake Presets
The project uses CMake presets for consistent build configuration. Available presets:

**Linux:**
- `x64-linux-debug` - Debug build with tests enabled
- `x64-linux-release` - Release build with tests enabled

**Basic workflow:**
```bash
# Configure
cmake --preset x64-linux-debug

# Build
cmake --build --preset x64-linux-debug

# Test (if tests are enabled)
# Tests have a 5-minute (300 second) timeout configured
ctest --preset x64-linux-debug

# Install
cmake --install output/build/x64-linux-debug
```

**Key preset features:**
- Uses Ninja generator
- Integrates with vcpkg for dependency management
- Requires `VCPKG_ROOT` environment variable to be set
- Tests can be enabled/disabled via `WITH_TESTS` cache variable
- Build artifacts go to `output/build/<preset-name>/`
- Install location: `output/install/<preset-name>/`

### Running Tests
Tests are automatically configured with a 5-minute (300 second) timeout to prevent hung tests.

**Run all tests:**
```bash
ctest --preset x64-linux-debug
```

**Run tests with verbose output:**
```bash
ctest --preset x64-linux-debug --verbose
```

**Run tests and show output on failure:**
```bash
ctest --preset x64-linux-debug --output-on-failure
```

**Run a specific test:**
```bash
ctest --preset x64-linux-debug -V:w
 -R activemq-unit-tests
```

**Run the test executable directly:**
```bash
ctest --preset x64-linux-debug -V
```

## Architecture Notes
- The project provides a Java-style synchronization API (`Synchronizable` interface, `Mutex` class, `Lock` RAII wrapper)
- The implementation uses C++ standard library primitives internally
- The `synchronized()` macro provides Java-like synchronized blocks using the `Lock` class
