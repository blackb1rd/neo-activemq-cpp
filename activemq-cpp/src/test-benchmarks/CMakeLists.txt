# Build benchmarks under src/test-benchmarks

set(BENCHMARK_SRCS
  # Top level launcher / registry
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/testRegistry.cpp

  # Benchmark utilities
  benchmark/PerformanceTimer.cpp

  # ActiveMQ benchmarks
  activemq/util/PrimitiveMapBenchmark.cpp

  # Decaf I/O benchmarks
  decaf/io/BufferedInputStreamBenchmark.cpp
  decaf/io/ByteArrayInputStreamBenchmark.cpp
  decaf/io/ByteArrayOutputStreamBenchmark.cpp
  decaf/io/DataInputStreamBenchmark.cpp
  decaf/io/DataOutputStreamBenchmark.cpp

  # Decaf language benchmarks
  decaf/lang/BooleanBenchmark.cpp
  decaf/lang/ThreadBenchmark.cpp

  # Decaf utility benchmarks
  decaf/util/HashMapBenchmark.cpp
  decaf/util/LinkedListBenchmark.cpp
  decaf/util/PropertiesBenchmark.cpp
  decaf/util/QueueBenchmark.cpp
  decaf/util/SetBenchmark.cpp
  decaf/util/StlListBenchmark.cpp
  decaf/util/StlMapBenchmark.cpp
)

set(BENCHMARK_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

# Build a single benchmark harness driven by the top-level main.cpp
list(REMOVE_ITEM BENCHMARK_SRCS ${BENCHMARK_MAIN})
add_executable(activemq-benchmark ${BENCHMARK_MAIN} ${BENCHMARK_SRCS})
target_include_directories(activemq-benchmark PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
  ${CMAKE_CURRENT_SOURCE_DIR}
)
# Link libraries with proper dependency order for static linking
# Use LINK_GROUP with RESCAN to ensure circular dependencies between static libraries are resolved
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  target_link_libraries(activemq-benchmark PRIVATE "$<LINK_GROUP:RESCAN,activemq_cms,activemq_core,decaf>")
else()
  # For older CMake, list libraries multiple times to resolve circular dependencies
  target_link_libraries(activemq-benchmark PRIVATE activemq_cms activemq_core decaf activemq_cms)
endif()

if(TARGET CppUnit::CppUnit)
  target_link_libraries(activemq-benchmark PRIVATE CppUnit::CppUnit)
elseif(TARGET CppUnit)
  target_link_libraries(activemq-benchmark PRIVATE CppUnit)
else()
  target_link_libraries(activemq-benchmark PRIVATE CppUnit)
endif()
