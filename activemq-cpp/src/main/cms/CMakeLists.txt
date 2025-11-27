# CMS layer library

file(GLOB_RECURSE CMS_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cxx
)

list(FILTER CMS_SOURCES EXCLUDE REGEX ".*/test/.*|.*/tests?/.*")

# These two exception implementations are needed by activemq_core at link
# time. Build them into activemq_core instead of activemq_cms to avoid a
# circular dependency when linking shared libraries. Remove them from the
# CMS sources list so they are not compiled into activemq_cms.
list(REMOVE_ITEM CMS_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/CMSSecurityException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/MessageEOFException.cpp
)

# Also remove other CMS exception implementation files that are needed
# by activemq_core so their symbols are available where BrokerError is
# constructed.
list(REMOVE_ITEM CMS_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/CMSException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/MessageFormatException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/MessageNotReadableException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/MessageNotWriteableException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/InvalidClientIdException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/InvalidDestinationException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/InvalidSelectorException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IllegalStateException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ResourceAllocationException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TransactionInProgressException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TransactionRolledBackException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnsupportedOperationException.cpp
)

# Also remove additional CMS implementations that are referenced by
# activemq_core (so activemq_core can link without depending on
# activemq_cms which would create a circular dependency when using
# shared libraries).
list(REMOVE_ITEM CMS_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/XAException.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/MessageConsumer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/MessageProducer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Destination.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/DestinationEvent.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/MessageEnumeration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QueueBrowser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/XAResource.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Xid.cpp
)

# Remove additional CMS implementation files that should be compiled
# into activemq_core so the core gets the concrete definitions (vtables,
# destructors, typeinfo) and avoids static archive ordering problems.
list(REMOVE_ITEM CMS_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Connection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ConnectionFactory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ConnectionMetaData.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/EnhancedConnection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Startable.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Stoppable.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Closeable.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Session.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/CMSProperties.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/BytesMessage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/MapMessage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ObjectMessage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/StreamMessage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Queue.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TemporaryQueue.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TemporaryTopic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Topic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Message.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/MessageTransformer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ConnectionFactory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/DestinationSource.cpp
)

if(AMQCPP_SHARED_LIB)
  add_library(activemq_cms SHARED ${CMS_SOURCES})
else()
  add_library(activemq_cms STATIC ${CMS_SOURCES})
endif()

target_include_directories(activemq_cms
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(activemq_cms PUBLIC activemq_core decaf)

set_target_properties(activemq_cms PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

# Windows-specific settings
if(WIN32)
  if(AMQCPP_SHARED_LIB)
    set_target_properties(activemq_cms PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()
  target_link_libraries(activemq_cms PRIVATE ws2_32)
  target_compile_definitions(activemq_cms PRIVATE _WIN32_WINNT=0x0601)
endif()
