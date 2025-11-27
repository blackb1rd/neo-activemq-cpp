# Build integration tests under src/test-integration
#
# IMPORTANT: Integration tests require a running ActiveMQ broker.
# Before running these tests, start the broker using Docker Compose:
#
#   docker-compose up -d
#
# Or using the helper script:
#   ./start-activemq-docker.sh
#
# Or manually with docker run:
#   docker run -d --rm --name activemq-test \
#     -p 61616:61616 -p 61613:61613 -p 8161:8161 \
#     apache/activemq-classic:5.18.7
#
# To stop the broker after testing:
#   docker-compose down
#   # OR: docker stop activemq-test

set(INTEGRATION_TEST_SRCS
  # Top level launcher / registry
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TestRegistry.cpp

  # Utilities and support
  util/teamcity/TeamCityProgressListener.cpp
  activemq/util/CMSListener.cpp
  activemq/util/CMSProvider.cpp
  activemq/util/IntegrationCommon.cpp

  # Base integration tests
  activemq/test/AdvisoryTest.cpp
  activemq/test/AsyncSenderTest.cpp
  activemq/test/BulkMessageTest.cpp
  activemq/test/CmsConnectionStartStopTest.cpp
  activemq/test/CmsSendWithAsyncCallbackTest.cpp
  activemq/test/CmsTemplateTest.cpp
  activemq/test/DurableTest.cpp
  activemq/test/ExpirationTest.cpp
  activemq/test/JmsMessageGroupsTest.cpp
  activemq/test/MapMessageTest.cpp
  activemq/test/MessageCompressionTest.cpp
  activemq/test/MessagePriorityTest.cpp
  activemq/test/QueueBrowserTest.cpp
  activemq/test/SimpleRollbackTest.cpp
  activemq/test/SimpleTest.cpp
  activemq/test/SlowListenerTest.cpp
  activemq/test/TransactionTest.cpp
  activemq/test/VirtualTopicTest.cpp

  # OpenWire protocol tests
  activemq/test/openwire/OpenwireAdvisoryTest.cpp
  activemq/test/openwire/OpenwireAsyncSenderTest.cpp
  activemq/test/openwire/OpenwireClientAckTest.cpp
  activemq/test/openwire/OpenwireCmsConnectionStartStopTest.cpp
  activemq/test/openwire/OpenWireCmsSendWithAsyncCallbackTest.cpp
  activemq/test/openwire/OpenwireCmsTemplateTest.cpp
  activemq/test/openwire/OpenwireDurableTest.cpp
  activemq/test/openwire/OpenwireEnhancedConnectionTest.cpp
  activemq/test/openwire/OpenwireExpirationTest.cpp
  activemq/test/openwire/OpenwireIndividualAckTest.cpp
  activemq/test/openwire/OpenwireJmsMessageGroupsTest.cpp
  activemq/test/openwire/OpenwireJmsRecoverTest.cpp
  activemq/test/openwire/OpenwireMapMessageTest.cpp
  activemq/test/openwire/OpenwireMessageCompressionTest.cpp
  activemq/test/openwire/OpenWireMessageListenerRedeliveryTest.cpp
  activemq/test/openwire/OpenwireMessagePriorityTest.cpp
  activemq/test/openwire/OpenwireNonBlockingRedeliveryTest.cpp
  activemq/test/openwire/OpenwireOptimizedAckTest.cpp
  activemq/test/openwire/OpenwireQueueBrowserTest.cpp
  activemq/test/openwire/OpenWireRedeliveryPolicyTest.cpp
  activemq/test/openwire/OpenwireSimpleRollbackTest.cpp
  activemq/test/openwire/OpenwireSimpleTest.cpp
  activemq/test/openwire/OpenwireSlowListenerTest.cpp
  activemq/test/openwire/OpenwireTempDestinationTest.cpp
  activemq/test/openwire/OpenwireTransactionTest.cpp
  activemq/test/openwire/OpenwireVirtualTopicTest.cpp
  activemq/test/openwire/OpenwireXATransactionsTest.cpp

  # STOMP protocol tests
  activemq/test/stomp/StompAdvisoryTest.cpp
  activemq/test/stomp/StompAsyncSenderTest.cpp
  activemq/test/stomp/StompBulkMessageTest.cpp
  activemq/test/stomp/StompCmsConnectionStartStopTest.cpp
  activemq/test/stomp/StompCmsTemplateTest.cpp
  activemq/test/stomp/StompDurableTest.cpp
  activemq/test/stomp/StompExpirationTest.cpp
  activemq/test/stomp/StompJmsMessageGroupsTest.cpp
  activemq/test/stomp/StompSimpleRollbackTest.cpp
  activemq/test/stomp/StompSimpleTest.cpp
  activemq/test/stomp/StompSlowListenerTest.cpp
  activemq/test/stomp/StompTransactionTest.cpp
)

set(INTEGRATION_TEST_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

# Build a single integration test harness driven by the top-level main.cpp
list(REMOVE_ITEM INTEGRATION_TEST_SRCS ${INTEGRATION_TEST_MAIN})
add_executable(activemq-integration-test ${INTEGRATION_TEST_MAIN} ${INTEGRATION_TEST_SRCS})
target_include_directories(activemq-integration-test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries with proper dependency order for static linking
# Use LINK_GROUP with RESCAN to ensure circular dependencies between static libraries are resolved
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  target_link_libraries(activemq-integration-test PRIVATE "$<LINK_GROUP:RESCAN,activemq_cms,activemq_core,decaf>")
else()
  # For older CMake, list libraries multiple times to resolve circular dependencies
  target_link_libraries(activemq-integration-test PRIVATE activemq_cms activemq_core decaf activemq_cms)
endif()

if(TARGET CppUnit::CppUnit)
  target_link_libraries(activemq-integration-test PRIVATE CppUnit::CppUnit)
elseif(TARGET CppUnit)
  target_link_libraries(activemq-integration-test PRIVATE CppUnit)
else()
  target_link_libraries(activemq-integration-test PRIVATE CppUnit)
endif()

# Note: Integration tests are NOT registered with CTest by default because they
# require a running ActiveMQ broker. To run integration tests manually:
#
# 1. Start ActiveMQ broker (see Docker commands above)
# 2. Run: ./activemq-integration-test
# 3. Stop the broker when done
#
# To register with CTest (advanced users only), uncomment the following:
# add_test(NAME activemq-integration-tests COMMAND activemq-integration-test)
# set_tests_properties(activemq-integration-tests PROPERTIES
#   TIMEOUT 600
#   ENVIRONMENT "ACTIVEMQ_BROKER_URI=tcp://localhost:61616"
# )
