# neoactivemq-cpp-config.cmake
# CMake configuration file for neoactivemq-cpp library
# This file allows find_package(neoactivemq-cpp) to work in consuming projects

@PACKAGE_INIT@

include(CMakeFindDependencyMacro)

# Find required dependencies
find_dependency(apr CONFIG)

# Optional: OpenSSL support
if(@AMQCPP_HAVE_OPENSSL@)
    find_dependency(OpenSSL)
endif()

# Include the targets file
include("${CMAKE_CURRENT_LIST_DIR}/neoactivemq-cpp-targets.cmake")

check_required_components(neoactivemq-cpp)
