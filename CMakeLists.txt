cmake_minimum_required(VERSION 3.15)
project(activemq-cpp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(apr CONFIG REQUIRED)

# Options
option(AMQCPP_BUILD_EXAMPLES "Build example programs" ON)
option(WITH_TESTS "Build all tests (unit, benchmark, integration)" OFF)
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
add_subdirectory(activemq-cpp/src/main/decaf)
add_subdirectory(activemq-cpp/src/main/activemq)
add_subdirectory(activemq-cpp/src/main/cms)
add_subdirectory(activemq-cpp/src/examples)

if(WITH_TESTS)
    find_package(CppUnit CONFIG REQUIRED)
	add_subdirectory(activemq-cpp/src/test)
	add_subdirectory(activemq-cpp/src/test-benchmarks)
	add_subdirectory(activemq-cpp/src/test-integration)

	# Enable CTest and register the test executables
	enable_testing()

	# Register unit tests
	add_test(NAME activemq-unit-tests COMMAND activemq-test)
	set_tests_properties(activemq-unit-tests PROPERTIES TIMEOUT 300)

	# Register benchmark tests
	add_test(NAME activemq-benchmarks COMMAND activemq-benchmark)
	set_tests_properties(activemq-benchmarks PROPERTIES TIMEOUT 600)

	# Register integration tests with automatic Docker management
	# Setup fixture: Start Docker before integration tests
	add_test(NAME integration-docker-start
		COMMAND docker compose up -d
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	)
	set_tests_properties(integration-docker-start PROPERTIES
		FIXTURES_SETUP IntegrationFixture
		LABELS "integration;fixture"
	)

	# Wait for ActiveMQ to be ready (health check)
	add_test(NAME integration-docker-wait
		COMMAND ${CMAKE_COMMAND} -E sleep 10
	)
	set_tests_properties(integration-docker-wait PROPERTIES
		FIXTURES_SETUP IntegrationFixture
		DEPENDS integration-docker-start
		LABELS "integration;fixture"
	)

	# The actual integration tests
	add_test(NAME activemq-integration-tests COMMAND activemq-integration-test)
	set_tests_properties(activemq-integration-tests PROPERTIES
		TIMEOUT 600
		LABELS "integration"
		FIXTURES_REQUIRED IntegrationFixture
	)

	# Cleanup fixture: Stop Docker after integration tests
	add_test(NAME integration-docker-stop
		COMMAND docker compose down
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	)
	set_tests_properties(integration-docker-stop PROPERTIES
		FIXTURES_CLEANUP IntegrationFixture
		LABELS "integration;fixture"
	)

	# Docker targets for integration tests
	# These targets help start/stop ActiveMQ broker for integration testing

	# Target: Start ActiveMQ broker with Docker Compose
	add_custom_target(docker-up
		COMMAND docker compose up -d
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Starting ActiveMQ broker with Docker Compose..."
	)

	# Target: Stop ActiveMQ broker
	add_custom_target(docker-down
		COMMAND docker compose down
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Stopping ActiveMQ broker..."
	)

	# Target: View ActiveMQ logs
	add_custom_target(docker-logs
		COMMAND docker compose logs -f activemq
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Viewing ActiveMQ logs (Ctrl+C to exit)..."
	)

	# Target: Restart ActiveMQ broker
	add_custom_target(docker-restart
		COMMAND docker compose restart
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Restarting ActiveMQ broker..."
	)

	# Target: Show Docker container status
	add_custom_target(docker-ps
		COMMAND docker compose ps
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Docker container status:"
	)

	# Convenience target: Build integration tests and ensure Docker is mentioned
	add_custom_target(integration-tests
		DEPENDS activemq-integration-test
		COMMENT "Integration tests built. Start ActiveMQ with: cmake --build . --target docker-up"
	)

	# Full integration test workflow target
	add_custom_target(integration-full
		COMMAND docker compose up -d
		COMMAND ${CMAKE_COMMAND} -E sleep 10
		COMMAND ${CMAKE_COMMAND} -E echo "Running integration tests..."
		COMMAND $<TARGET_FILE:activemq-integration-test>
		COMMAND docker compose down
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		DEPENDS activemq-integration-test
		COMMENT "Full integration test workflow (start broker, test, stop broker)..."
	)

	message(STATUS "Integration test Docker targets available:")
	message(STATUS "  cmake --build --preset <preset> --target docker-up         # Start ActiveMQ")
	message(STATUS "  cmake --build --preset <preset> --target docker-down       # Stop ActiveMQ")
	message(STATUS "  cmake --build --preset <preset> --target docker-logs       # View logs")
	message(STATUS "  cmake --build --preset <preset> --target integration-full  # Full workflow")
endif()

