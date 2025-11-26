cmake_minimum_required(VERSION 3.15)
project(activemq-cpp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(apr CONFIG REQUIRED)

# Options
option(AMQCPP_BUILD_EXAMPLES "Build example programs" ON)
option(WITH_TESTS "Build tests" OFF)
option(AMQCPP_SHARED_LIB "Build shared library instead of static" OFF)

# Option to enable/disable SSL support. When ON, OpenSSL is required.
option(AMQCPP_USE_SSL "Enable OpenSSL support (required when ON)" ON)

if(AMQCPP_USE_SSL)
	find_package(OpenSSL REQUIRED)
	message(STATUS "AMQCPP: SSL support enabled; OpenSSL will be required.")

	# If OpenSSL was found, define HAVE_OPENSSL for the code so sources
	# can use `#ifdef HAVE_OPENSSL` to enable OpenSSL-dependent features.
	if(OpenSSL_FOUND)
		message(STATUS "AMQCPP: Found OpenSSL ${OPENSSL_VERSION}")
		add_compile_definitions(HAVE_OPENSSL)
		set(AMQCPP_HAVE_OPENSSL ON CACHE INTERNAL "OpenSSL available")
	else()
		message(STATUS "AMQCPP: OpenSSL not found; HAVE_OPENSSL will not be defined")
		set(AMQCPP_HAVE_OPENSSL OFF CACHE INTERNAL "OpenSSL available")
	endif()
else()
	find_package(OpenSSL QUIET)
	message(STATUS "AMQCPP: SSL support disabled; OpenSSL will be optional.")
endif()


## Inline `src/CMakeLists.txt` content so project can be configured from
## the top-level CMake without a separate `src` directory CMakeLists.

# Build component libraries in `src/main` and tests/examples under `src`

# Add component subdirectories if present (paths are relative to project root)
add_subdirectory(src/main/decaf)
add_subdirectory(src/main/activemq)
add_subdirectory(src/main/cms)
add_subdirectory(src/examples)

if(WITH_TESTS)
    find_package(CppUnit CONFIG REQUIRED)
	# Enable CTest before adding test subdirectory
	enable_testing()
	add_subdirectory(src/test)

	# Register the test executable
	add_test(NAME activemq-unit-tests COMMAND activemq-test)

	# Set a timeout for the test (default: 300 seconds = 5 minutes)
	set_tests_properties(activemq-unit-tests PROPERTIES TIMEOUT 300)
endif()