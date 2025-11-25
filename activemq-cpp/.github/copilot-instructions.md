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

## Architecture Notes
- The project provides a Java-style synchronization API (`Synchronizable` interface, `Mutex` class, `Lock` RAII wrapper)
- The implementation uses C++ standard library primitives internally
- The `synchronized()` macro provides Java-like synchronized blocks using the `Lock` class
