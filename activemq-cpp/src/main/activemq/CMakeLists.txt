# activemq core library

file(GLOB_RECURSE ACTIVEMQ_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cxx
)

list(FILTER ACTIVEMQ_SOURCES EXCLUDE REGEX ".*/test/.*|.*/tests?/.*")

# Include specific CMS exception implementations in activemq_core so the
# exception symbols are available without creating a circular link
# dependency between activemq_core and activemq_cms.
list(APPEND ACTIVEMQ_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/CMSSecurityException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/MessageEOFException.cpp
)

# Also include other CMS exception implementations that BrokerError and
# other activemq_core sources construct, so their symbols are available
# when linking activemq_core.
list(APPEND ACTIVEMQ_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/CMSException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/MessageFormatException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/MessageNotReadableException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/MessageNotWriteableException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/InvalidClientIdException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/InvalidDestinationException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/InvalidSelectorException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/IllegalStateException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/ResourceAllocationException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/TransactionInProgressException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/TransactionRolledBackException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/UnsupportedOperationException.cpp
)

# Also compile in additional CMS implementations that activemq_core
# needs directly so those symbols are available at link-time.
list(APPEND ACTIVEMQ_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/XAException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/MessageConsumer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/MessageProducer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/Destination.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/DestinationEvent.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/MessageEnumeration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/QueueBrowser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/XAResource.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/Xid.cpp
)

# Also compile additional CMS implementation files into activemq_core so
# activemq_core object files have the concrete implementations they rely on
# (destructors, vtables, typeinfo). This prevents unresolved-symbol and
# static-archive ordering problems when building/linking on Linux.
list(APPEND ACTIVEMQ_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/Connection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/ConnectionFactory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/ConnectionMetaData.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/EnhancedConnection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/Startable.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/Stoppable.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/Closeable.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/Session.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/CMSProperties.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/BytesMessage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/MapMessage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/ObjectMessage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/StreamMessage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/Queue.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/TemporaryQueue.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/TemporaryTopic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/Topic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/Message.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/MessageTransformer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../cms/DestinationSource.cpp
)

if(AMQCPP_SHARED_LIB)
  add_library(activemq_core SHARED ${ACTIVEMQ_SOURCES})
else()
  add_library(activemq_core STATIC ${ACTIVEMQ_SOURCES})
endif()

target_include_directories(activemq_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(activemq_core PUBLIC decaf)

set_target_properties(activemq_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

# Windows-specific settings
if(WIN32)
  if(AMQCPP_SHARED_LIB)
    set_target_properties(activemq_core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()
  target_link_libraries(activemq_core PRIVATE ws2_32)
  target_compile_definitions(activemq_core PRIVATE _WIN32_WINNT=0x0601)
endif()
