# Build the decaf utility library

file(GLOB_RECURSE DECAF_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cxx
)

# Exclude test directories if any
list(FILTER DECAF_SOURCES EXCLUDE REGEX ".*/test/.*|.*/tests?/.*")

# Exclude platform-specific implementations that don't apply to the current
# platform. Decaf contains both `internal/security/windows/*` and
# `internal/security/unix/*` implementations; ensure we only compile the
# appropriate set for the target platform to avoid headers like <windows.h>
# being required on Linux and POSIX sources on Windows.
# Note: internal/util/concurrent/* now uses C++ standard library and is cross-platform
if(WIN32)
  list(FILTER DECAF_SOURCES EXCLUDE REGEX ".*/internal/security/unix/.*")
  list(FILTER DECAF_SOURCES EXCLUDE REGEX ".*/internal/util/concurrent/unix/.*")
  list(FILTER DECAF_SOURCES EXCLUDE REGEX ".*/internal/util/concurrent/windows/.*")
else()
  list(FILTER DECAF_SOURCES EXCLUDE REGEX ".*/internal/security/windows/.*")
  list(FILTER DECAF_SOURCES EXCLUDE REGEX ".*/internal/util/concurrent/unix/.*")
  list(FILTER DECAF_SOURCES EXCLUDE REGEX ".*/internal/util/concurrent/windows/.*")
endif()

if(AMQCPP_SHARED_LIB)
  add_library(decaf SHARED ${DECAF_SOURCES})
else()
  add_library(decaf STATIC ${DECAF_SOURCES})
endif()

target_include_directories(decaf
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/main
)

if(AMQCPP_USE_SSL)
  find_package(OpenSSL REQUIRED)
else()
  find_package(OpenSSL QUIET)
endif()

if(OpenSSL_FOUND)
  target_compile_definitions(decaf PUBLIC HAVE_OPENSSL)
  target_link_libraries(decaf PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

set_target_properties(decaf PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

# Windows-specific settings
if(WIN32)
  # Ensure symbols are exported when building shared libs on Windows
  if(AMQCPP_SHARED_LIB)
    set_target_properties(decaf PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()
  target_link_libraries(decaf PRIVATE ws2_32)
  target_compile_definitions(decaf PRIVATE _WIN32_WINNT=0x0601)
endif()

# Ensure C++ threading is available on all platforms
find_package(Threads REQUIRED)
target_link_libraries(decaf PRIVATE Threads::Threads)

# Ensure POSIX threading macros are defined on non-Windows platforms for compatibility
if(NOT WIN32)
  # Define numeric macro so headers using `#if HAVE_PTHREAD_H` work correctly
  target_compile_definitions(decaf PUBLIC HAVE_PTHREAD_H=1)
endif()

## Locate APR using vcpkg only (no pkg-config or automatic system fallback)
set(_apr_found FALSE)

## Link APR targets (use whichever imported target names are available).
# Support common variants provided by different vcpkg/packaging layouts.
set(_apr_linked FALSE)
if(TARGET apr::apr)
  target_link_libraries(decaf PUBLIC apr::apr)
  set(_apr_linked TRUE)
elseif(TARGET apr::apr-1)
  target_link_libraries(decaf PUBLIC apr::apr-1)
  set(_apr_linked TRUE)
elseif(TARGET apr::libapr-1)
  target_link_libraries(decaf PUBLIC apr::libapr-1)
  set(_apr_linked TRUE)
endif()

set(_aprapp_linked FALSE)
if(TARGET apr::aprapp)
  target_link_libraries(decaf PUBLIC apr::aprapp)
  set(_aprapp_linked TRUE)
elseif(TARGET apr::aprapp-1)
  target_link_libraries(decaf PUBLIC apr::aprapp-1)
  set(_aprapp_linked TRUE)
elseif(TARGET apr::libaprapp-1)
  target_link_libraries(decaf PUBLIC apr::libaprapp-1)
  set(_aprapp_linked TRUE)
endif()

if(NOT _apr_linked OR NOT _aprapp_linked)
  message(FATAL_ERROR "APR imported targets not found. Ensure APR is installed and available as CMake config targets (e.g. via vcpkg apr + apr-util), or set APR_ROOT/CMAKE_PREFIX_PATH to a prefix containing aprConfig.cmake.")
endif()
