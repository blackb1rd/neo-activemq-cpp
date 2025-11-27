/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FailoverTransportTest.h"

#include <activemq/transport/failover/FailoverTransportFactory.h>
#include <activemq/transport/failover/FailoverTransport.h>
#include <activemq/transport/mock/MockTransport.h>
#include <activemq/exceptions/ActiveMQException.h>
#include <activemq/commands/ActiveMQMessage.h>
#include <activemq/commands/ConnectionControl.h>
#include <activemq/mock/MockBrokerService.h>
#include <decaf/util/concurrent/CountDownLatch.h>
#include <decaf/util/concurrent/Mutex.h>
#include <decaf/lang/Pointer.h>
#include <decaf/lang/Thread.h>
#include <decaf/util/UUID.h>

#include <random>
#include <chrono>

using namespace activemq;
using namespace activemq::mock;
using namespace activemq::commands;
using namespace activemq::transport;
using namespace activemq::transport::failover;
using namespace activemq::transport::mock;
using namespace activemq::exceptions;
using namespace decaf::io;
using namespace decaf::lang;
using namespace decaf::util;
using namespace decaf::util::concurrent;

////////////////////////////////////////////////////////////////////////////////
FailoverTransportTest::FailoverTransportTest() {
}

////////////////////////////////////////////////////////////////////////////////
FailoverTransportTest::~FailoverTransportTest() {
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testTransportCreate() {

    std::string uri = "failover://(mock://localhost:61616)?randomize=false";

    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    Thread::sleep(1000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testTransportCreateWithBackups() {

    std::string uri = "failover://(mock://localhost:61616,mock://localhost:61618)?randomize=false&backup=true";

    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);
    CPPUNIT_ASSERT(failover->isBackup() == true);

    transport->start();

    Thread::sleep(1000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
class FailToConnectListener : public DefaultTransportListener {
public:

    bool caughtException;

    FailToConnectListener() : caughtException(false) {}

    virtual void onException(const decaf::lang::Exception& ex AMQCPP_UNUSED) {
        caughtException = true;
    }
};

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testTransportCreateFailOnCreate() {

    std::string uri =
            "failover://(mock://localhost:61616?failOnCreate=true)?useExponentialBackOff=false&maxReconnectAttempts=3&initialReconnectDelay=100";

    FailToConnectListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->getMaxReconnectAttempts() == 3);

    transport->start();

    Thread::sleep(1000);

    CPPUNIT_ASSERT(listener.caughtException == true);
    CPPUNIT_ASSERT(failover->isConnected() == false);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testTransportCreateFailOnCreateSendMessage() {

    std::string uri =
            "failover://(mock://localhost:61616?failOnCreate=true)?useExponentialBackOff=false&maxReconnectAttempts=3&initialReconnectDelay=100";

    Pointer<ActiveMQMessage> message(new ActiveMQMessage());

    FailToConnectListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->getMaxReconnectAttempts() == 3);

    transport->start();

    CPPUNIT_ASSERT_THROW_MESSAGE("Should Throw a IOException", transport->oneway(message), IOException);

    CPPUNIT_ASSERT(listener.caughtException == true);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFailingBackupCreation() {

    std::string uri = "failover://(mock://localhost:61616,"
            "mock://localhost:61618?failOnCreate=true)?randomize=false&backup=true";

    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);
    CPPUNIT_ASSERT(failover->isBackup() == true);

    transport->start();

    Thread::sleep(2000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
class MessageCountingListener : public DefaultTransportListener {
public:

    int numMessages;

    MessageCountingListener() : numMessages(0) {}

    virtual void onCommand(const Pointer<Command> command AMQCPP_UNUSED) {
        numMessages++;
    }
};

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testSendOnewayMessage() {

    std::string uri = "failover://(mock://localhost:61616)?randomize=false";

    const int numMessages = 1000;
    Pointer<ActiveMQMessage> message(new ActiveMQMessage());

    MessageCountingListener messageCounter;
    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    Thread::sleep(1000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    MockTransport* mock = NULL;
    while (mock == NULL) {
        mock = dynamic_cast<MockTransport*>(transport->narrow(typeid(MockTransport)));
    }
    mock->setOutgoingListener(&messageCounter);

    for (int i = 0; i < numMessages; ++i) {
        transport->oneway(message);
    }

    Thread::sleep(2000);

    CPPUNIT_ASSERT(messageCounter.numMessages = numMessages);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testSendRequestMessage() {

    std::string uri = "failover://(mock://localhost:61616)?randomize=false";

    Pointer<ActiveMQMessage> message(new ActiveMQMessage());

    MessageCountingListener messageCounter;
    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    Thread::sleep(1000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    MockTransport* mock = NULL;
    while (mock == NULL) {
        mock = dynamic_cast<MockTransport*>(transport->narrow(typeid(MockTransport)));
    }
    mock->setOutgoingListener(&messageCounter);

    transport->request(message);
    transport->request(message);
    transport->request(message);
    transport->request(message);
    Thread::sleep(1000);

    CPPUNIT_ASSERT(messageCounter.numMessages = 4);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testSendOnewayMessageFail() {

    std::string uri = "failover://(mock://localhost:61616?failOnSendMessage=true,"
            "mock://localhost:61618)?randomize=false";

    Pointer<ActiveMQMessage> message(new ActiveMQMessage());

    MessageCountingListener messageCounter;
    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    Thread::sleep(1000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    MockTransport* mock = NULL;
    while (mock == NULL) {
        mock = dynamic_cast<MockTransport*>(transport->narrow(typeid(MockTransport)));
    }
    mock->setOutgoingListener(&messageCounter);

    transport->oneway(message);
    transport->oneway(message);
    transport->oneway(message);
    transport->oneway(message);
    Thread::sleep(1000);

    CPPUNIT_ASSERT(messageCounter.numMessages = 4);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testSendRequestMessageFail() {

    std::string uri = "failover://(mock://localhost:61616?failOnSendMessage=true,"
            "mock://localhost:61618)?randomize=false";

    Pointer<ActiveMQMessage> message(new ActiveMQMessage());

    MessageCountingListener messageCounter;
    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    Thread::sleep(1000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    MockTransport* mock = NULL;
    while (mock == NULL) {
        mock = dynamic_cast<MockTransport*>(transport->narrow(typeid(MockTransport)));
    }
    mock->setOutgoingListener(&messageCounter);

    transport->request(message);
    transport->request(message);
    transport->request(message);
    transport->request(message);
    Thread::sleep(1000);

    CPPUNIT_ASSERT(messageCounter.numMessages = 4);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testWithOpewireCommands() {

    std::string uri = "failover://(mock://localhost:61616)?randomize=false";

    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    Thread::sleep(1000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    Pointer<ConnectionInfo> connection = createConnection();
    transport->request(connection);
    Pointer<SessionInfo> session1 = createSession(connection);
    transport->request(session1);
    Pointer<SessionInfo> session2 = createSession(connection);
    transport->request(session2);
    Pointer<ConsumerInfo> consumer1 = createConsumer(session1);
    transport->request(consumer1);
    Pointer<ConsumerInfo> consumer2 = createConsumer(session1);
    transport->request(consumer2);
    Pointer<ConsumerInfo> consumer3 = createConsumer(session2);
    transport->request(consumer3);

    Pointer<ProducerInfo> producer1 = createProducer(session2);
    transport->request(producer1);

    // Remove the Producers
    this->disposeOf(producer1, transport);

    // Remove the Consumers
    this->disposeOf(consumer1, transport);
    this->disposeOf(consumer2, transport);
    this->disposeOf(consumer3, transport);

    // Remove the Session instances.
    this->disposeOf(session1, transport);
    this->disposeOf(session2, transport);

    // Indicate that we are done.
    Pointer<ShutdownInfo> shutdown(new ShutdownInfo());
    transport->oneway(shutdown);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
Pointer<ConnectionInfo> FailoverTransportTest::createConnection() {

    Pointer<ConnectionId> id(new ConnectionId());
    id->setValue(UUID::randomUUID().toString());

    Pointer<ConnectionInfo> info(new ConnectionInfo());
    info->setClientId(UUID::randomUUID().toString());
    info->setConnectionId(id);

    return info;
}

////////////////////////////////////////////////////////////////////////////////
Pointer<SessionInfo> FailoverTransportTest::createSession(const Pointer<ConnectionInfo>& parent) {

    static int idx = 1;

    Pointer<SessionId> id(new SessionId());
    id->setConnectionId(parent->getConnectionId()->getValue());
    id->setValue(idx++);

    Pointer<SessionInfo> info(new SessionInfo());
    info->setSessionId(id);

    return info;
}

////////////////////////////////////////////////////////////////////////////////
Pointer<ConsumerInfo> FailoverTransportTest::createConsumer(const Pointer<SessionInfo>& parent) {

    static int idx = 1;

    Pointer<ConsumerId> id(new ConsumerId());
    id->setConnectionId(parent->getSessionId()->getConnectionId());
    id->setSessionId(parent->getSessionId()->getValue());
    id->setValue(idx++);

    Pointer<ConsumerInfo> info(new ConsumerInfo());
    info->setConsumerId(id);

    return info;
}

////////////////////////////////////////////////////////////////////////////////
Pointer<ProducerInfo> FailoverTransportTest::createProducer(const Pointer<SessionInfo>& parent) {

    static int idx = 1;

    Pointer<ProducerId> id(new ProducerId());
    id->setConnectionId(parent->getSessionId()->getConnectionId());
    id->setSessionId(parent->getSessionId()->getValue());
    id->setValue(idx++);

    Pointer<ProducerInfo> info(new ProducerInfo());
    info->setProducerId(id);

    return info;
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::disposeOf(const Pointer<SessionInfo>& session, Pointer<Transport>& transport) {

    Pointer<RemoveInfo> command(new RemoveInfo());
    command->setObjectId(session->getSessionId());
    transport->oneway(command);
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::disposeOf(const Pointer<ConsumerInfo>& consumer, Pointer<Transport>& transport) {

    Pointer<RemoveInfo> command(new RemoveInfo());
    command->setObjectId(consumer->getConsumerId());
    transport->oneway(command);
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::disposeOf(const Pointer<ProducerInfo>& producer, Pointer<Transport>& transport) {

    Pointer<RemoveInfo> command(new RemoveInfo());
    command->setObjectId(producer->getProducerId());
    transport->oneway(command);
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testTransportHandlesConnectionControl() {

    std::string uri = "failover://(mock://localhost:61618?failOnCreate=true,mock://localhost:61616)?randomize=false";

    std::string reconnectStr = "mock://localhost:61613?name=Reconnect";

    Pointer<ConnectionControl> control(new ConnectionControl());
    control->setReconnectTo(reconnectStr);
    control->setRebalanceConnection(true);

    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    failover->setUpdateURIsSupported(true);

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    Thread::sleep(3000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    MockTransport* mock = NULL;
    while (mock == NULL) {
        Thread::sleep(100);
        mock = dynamic_cast<MockTransport*>(transport->narrow(typeid(MockTransport)));
    }

    LinkedList<URI> removals;
    removals.add(URI("mock://localhost:61616"));

    mock->fireCommand(control);
    Thread::sleep(2000);
    failover->removeURI(true, removals);

    Thread::sleep(20000);

    mock = NULL;
    while (mock == NULL) {
        Thread::sleep(100);
        mock = dynamic_cast<MockTransport*>(transport->narrow(typeid(MockTransport)));
    }

    CPPUNIT_ASSERT_EQUAL(std::string("Reconnect"), mock->getName());
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testPriorityBackupConfig() {

    std::string uri = "failover://(mock://localhost:61616,"
                      "mock://localhost:61618)?randomize=false&priorityBackup=true";

    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);
    CPPUNIT_ASSERT(failover->isPriorityBackup() == true);

    transport->start();

    Thread::sleep(1000);
    CPPUNIT_ASSERT(failover->isConnected() == true);
    CPPUNIT_ASSERT(failover->isConnectedToPriority() == true);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testUriOptionsApplied() {

    std::string uri = "failover://(mock://localhost:61616,mock://localhost:61618)?"
            "randomize=true&"
            "priorityBackup=true&"
            "initialReconnectDelay=222&"
            "useExponentialBackOff=false&"
            "maxReconnectAttempts=27&"
            "startupMaxReconnectAttempts=44&"
            "backup=true&"
            "trackMessages=false&"
            "maxCacheSize=16543217&"
            "timeout=500&"
            "updateURIsSupported=false&"
            "maxReconnectDelay=55555&"
            "priorityURIs=mock://localhost:61617,mock://localhost:61619";

    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == true);
    CPPUNIT_ASSERT(failover->isPriorityBackup() == true);
    CPPUNIT_ASSERT(failover->isUseExponentialBackOff() == false);
    CPPUNIT_ASSERT(failover->getInitialReconnectDelay() == 222);
    CPPUNIT_ASSERT(failover->getMaxReconnectAttempts() == 27);
    CPPUNIT_ASSERT(failover->getStartupMaxReconnectAttempts() == 44);
    CPPUNIT_ASSERT(failover->isBackup() == true);
    CPPUNIT_ASSERT(failover->isTrackMessages() == false);
    CPPUNIT_ASSERT(failover->getMaxCacheSize() == 16543217);
    CPPUNIT_ASSERT(failover->isUpdateURIsSupported() == false);
    CPPUNIT_ASSERT(failover->getMaxReconnectDelay() == 55555);

    const List<URI>& priorityUris = failover->getPriorityURIs();
    CPPUNIT_ASSERT(priorityUris.size() == 2);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testConnectedToMockBroker() {

    MockBrokerService broker1(61626);
    MockBrokerService broker2(61628);

    broker1.start();
    broker1.waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626,"
            "tcp://localhost:61628)";

    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);

    transport->start();

    int count = 0;
    while (!failover->isConnected() && count++ < 20) {
        Thread::sleep(200);
    }
    CPPUNIT_ASSERT(failover->isConnected() == true);
    CPPUNIT_ASSERT(failover->isConnectedToPriority() == false);

    transport->close();

    broker1.stop();
    broker1.waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testMaxReconnectsZeroAttemptsOneConnect() {

    std::string uri = "failover://(mock://localhost:61616)?maxReconnectAttempts=0";

    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);

    transport->start();

    Thread::sleep(1000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testMaxReconnectsHonorsConfiguration() {

    // max reconnect attempts of two means one connection attempt followed by
    // two retries.

    std::string uri = "failover://(mock://localhost:61616?failOnCreate=true,"
                                  "mock://localhost:61617?failOnCreate=true)"
                                  "?randomize=false&maxReconnectAttempts=2";

    Pointer<WireFormatInfo> info(new WireFormatInfo());

    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    CPPUNIT_ASSERT_THROW_MESSAGE("Send should have failed after max connect attempts of two",
            transport->oneway(info), Exception);

    CPPUNIT_ASSERT(failover->isConnected() == false);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testStartupMaxReconnectsHonorsConfiguration() {

    // max reconnect attempts of two means one connection attempt followed by
    // two retries.

    std::string uri = "failover://(mock://localhost:61616?failOnCreate=true,"
                                  "mock://localhost:61617?failOnCreate=true)"
                                  "?randomize=false&startupMaxReconnectAttempts=2";

    Pointer<WireFormatInfo> info(new WireFormatInfo());

    DefaultTransportListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    CPPUNIT_ASSERT_THROW_MESSAGE("Send should have failed after max connect attempts of two",
            transport->oneway(info), Exception);

    CPPUNIT_ASSERT(failover->isConnected() == false);

    transport->close();
}

////////////////////////////////////////////////////////////////////////////////
namespace {

    class PriorityBackupListener : public DefaultTransportListener {
    private:

        Pointer<CountDownLatch> interruptedLatch;
        Pointer<CountDownLatch> resumedLatch;

        Mutex resetMutex;

    public:

        PriorityBackupListener() : interruptedLatch(new CountDownLatch(1)),
                                   resumedLatch(new CountDownLatch(1)),
                                   resetMutex() {
        }

        virtual ~PriorityBackupListener() {}

        virtual void transportInterrupted() {
            Pointer<CountDownLatch> latch;
            synchronized(&resetMutex) {
                latch = interruptedLatch;
            }
            if (latch != NULL) {
                latch->countDown();
            }
        }

        virtual void transportResumed() {
            Pointer<CountDownLatch> latch;
            synchronized(&resetMutex) {
                latch = resumedLatch;
            }
            if (latch != NULL) {
                latch->countDown();
            }
        }

        void reset() {
            synchronized(&resetMutex) {
                interruptedLatch.reset(new CountDownLatch(1));
                resumedLatch.reset(new CountDownLatch(1));
            }
        }

        bool awaitInterruption() {
            Pointer<CountDownLatch> latch;
            synchronized(&resetMutex) {
                latch = interruptedLatch;
            }
            return latch->await(60000);
        }

        bool awaitResumed() {
            Pointer<CountDownLatch> latch;
            synchronized(&resetMutex) {
                latch = resumedLatch;
            }
            return latch->await(60000);
        }
    };
}

// ============================================================================
// Failover Tests WITHOUT Randomization (randomize=false)
// ============================================================================

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFailoverNoRandomizeBothOnline() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Both brokers online
    broker1->start();
    broker1->waitUntilStarted();

    broker2->start();
    broker2->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?randomize=false";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    CPPUNIT_ASSERT_MESSAGE("Failed to connect initially", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    // Stop broker1, should failover to broker2
    broker1->stop();
    broker1->waitUntilStopped();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted", listener.awaitInterruption());
    CPPUNIT_ASSERT_MESSAGE("Failed to reconnect", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
    broker1->stop();
    broker1->waitUntilStopped();
    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFailoverNoRandomizeBroker1OnlyOnline() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Only broker1 online
    broker1->start();
    broker1->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?randomize=false";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    CPPUNIT_ASSERT_MESSAGE("Failed to connect to broker1", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    // Stop broker1, should lose connection (broker2 still offline)
    broker1->stop();
    broker1->waitUntilStopped();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted", listener.awaitInterruption());

    // Start broker2, should reconnect
    broker2->start();
    broker2->waitUntilStarted();

    CPPUNIT_ASSERT_MESSAGE("Failed to reconnect to broker2", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
    broker1->stop();
    broker1->waitUntilStopped();
    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFailoverNoRandomizeBroker2OnlyOnline() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Only broker2 online
    broker2->start();
    broker2->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?randomize=false";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    CPPUNIT_ASSERT_MESSAGE("Failed to connect to broker2", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    // Start broker1
    broker1->start();
    broker1->waitUntilStarted();
    Thread::sleep(500);

    // Stop broker2, should failover to broker1
    broker2->stop();
    broker2->waitUntilStopped();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted", listener.awaitInterruption());
    CPPUNIT_ASSERT_MESSAGE("Failed to reconnect to broker1", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
    broker1->stop();
    broker1->waitUntilStopped();
    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFailoverNoRandomizeBothOfflineBroker1ComesOnline() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Both brokers offline initially
    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?randomize=false&startupMaxReconnectAttempts=10&initialReconnectDelay=50&useExponentialBackOff=false";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    // Give it a moment to attempt connection
    Thread::sleep(500);

    // Start broker1
    broker1->start();
    broker1->waitUntilStarted();

    CPPUNIT_ASSERT_MESSAGE("Failed to connect to broker1", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
    broker1->stop();
    broker1->waitUntilStopped();
    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFailoverNoRandomizeBothOfflineBroker2ComesOnline() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Both brokers offline initially
    // Use longer reconnect delay and more attempts to ensure broker has time to start
    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?randomize=false&startupMaxReconnectAttempts=50&initialReconnectDelay=50&maxReconnectDelay=50&useExponentialBackOff=false";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);

    transport->start();

    // Give it a moment to attempt connection, then start broker2
    Thread::sleep(300);

    // Start broker2
    broker2->start();
    broker2->waitUntilStarted();

    CPPUNIT_ASSERT_MESSAGE("Failed to connect to broker2", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
    broker1->stop();
    broker1->waitUntilStopped();
    broker2->stop();
    broker2->waitUntilStopped();
}

// ============================================================================
// Failover Tests WITH Randomization (randomize=true, default)
// ============================================================================

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFailoverWithRandomizeBothOnline() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Both brokers online
    broker1->start();
    broker1->waitUntilStarted();

    broker2->start();
    broker2->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);

    transport->start();

    CPPUNIT_ASSERT_MESSAGE("Failed to connect initially", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    // Stop one broker (could be connected to either due to randomization)
    broker1->stop();
    broker1->waitUntilStopped();

    // Should either stay connected (if on broker2) or failover to broker2
    Thread::sleep(1000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
    broker1->stop();
    broker1->waitUntilStopped();
    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFailoverWithRandomizeBroker1OnlyOnline() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Only broker1 online
    broker1->start();
    broker1->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);

    transport->start();

    CPPUNIT_ASSERT_MESSAGE("Failed to connect to broker1", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    // Stop broker1
    broker1->stop();
    broker1->waitUntilStopped();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted", listener.awaitInterruption());

    // Start broker2
    broker2->start();
    broker2->waitUntilStarted();

    CPPUNIT_ASSERT_MESSAGE("Failed to reconnect to broker2", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
    broker1->stop();
    broker1->waitUntilStopped();
    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFailoverWithRandomizeBroker2OnlyOnline() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Only broker2 online
    broker2->start();
    broker2->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);

    transport->start();

    CPPUNIT_ASSERT_MESSAGE("Failed to connect to broker2", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    // Start broker1
    broker1->start();
    broker1->waitUntilStarted();
    Thread::sleep(500);

    // Stop broker2
    broker2->stop();
    broker2->waitUntilStopped();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted", listener.awaitInterruption());
    CPPUNIT_ASSERT_MESSAGE("Failed to reconnect to broker1", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
    broker1->stop();
    broker1->waitUntilStopped();
    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFailoverWithRandomizeBothOfflineBroker1ComesOnline() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Both brokers offline initially
    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?startupMaxReconnectAttempts=50&initialReconnectDelay=50&useExponentialBackOff=false";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);

    transport->start();

    // Give it a moment to attempt connection (but not exhaust all attempts)
    Thread::sleep(500);

    // Start broker1
    broker1->start();
    broker1->waitUntilStarted();

    CPPUNIT_ASSERT_MESSAGE("Failed to connect to broker1", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
    broker1->stop();
    broker1->waitUntilStopped();
    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFailoverWithRandomizeBothOfflineBroker2ComesOnline() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Both brokers offline initially
    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?startupMaxReconnectAttempts=50&initialReconnectDelay=50&useExponentialBackOff=false";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);

    transport->start();

    // Give it a moment to attempt connection
    Thread::sleep(500);

    // Start broker2
    broker2->start();
    broker2->waitUntilStarted();

    CPPUNIT_ASSERT_MESSAGE("Failed to connect to broker2", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();
    broker1->stop();
    broker1->waitUntilStopped();
    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testConnectedToPriorityOnFirstTryThenFailover() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    broker1->start();
    broker1->waitUntilStarted();

    broker2->start();
    broker2->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?randomize=false&priorityBackup=true";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);

    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);
    CPPUNIT_ASSERT(failover->isPriorityBackup() == true);

    transport->start();

    CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", listener.awaitResumed());
    listener.reset();

    CPPUNIT_ASSERT(failover->isConnected() == true);
    CPPUNIT_ASSERT(failover->isConnectedToPriority() == true);

    broker1->stop();
    broker1->waitUntilStopped();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted in time", listener.awaitInterruption());
    CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", listener.awaitResumed());
    listener.reset();

    CPPUNIT_ASSERT(failover->isConnected() == true);
    CPPUNIT_ASSERT(failover->isConnectedToPriority() == false);

    transport->close();

    broker1->stop();
    broker1->waitUntilStopped();

    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testConnectsToPriorityOnceStarted() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    broker2->start();
    broker2->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626?transport.useInactivityMonitor=false,"
                                  "tcp://localhost:61628?transport.useInactivityMonitor=false)?randomize=false&priorityBackup=true";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);

    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);
    CPPUNIT_ASSERT(failover->isPriorityBackup() == true);

    transport->start();

    CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", listener.awaitResumed());
    listener.reset();

    CPPUNIT_ASSERT(failover->isConnected() == true);
    CPPUNIT_ASSERT(failover->isConnectedToPriority() == false);

    broker1->start();
    broker1->waitUntilStarted();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted in time", listener.awaitInterruption());

    CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", listener.awaitResumed());
    listener.reset();

    CPPUNIT_ASSERT(failover->isConnected() == true);
    CPPUNIT_ASSERT(failover->isConnectedToPriority() == true);

    transport->close();

    broker1->stop();
    broker1->waitUntilStopped();

    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testConnectsToPriorityAfterInitialBackupFails() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61627));
    Pointer<MockBrokerService> broker3(new MockBrokerService(61628));

    broker2->start();
    broker2->waitUntilStarted();

    broker3->start();
    broker3->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626?transport.useInactivityMonitor=false,"
                                  "tcp://localhost:61627?transport.useInactivityMonitor=false,"
                                  "tcp://localhost:61628?transport.useInactivityMonitor=false)?randomize=false&priorityBackup=true";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);
    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);
    CPPUNIT_ASSERT(failover->isPriorityBackup() == true);

    transport->start();

    CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", listener.awaitResumed());
    listener.reset();

    CPPUNIT_ASSERT(failover->isConnected() == true);
    CPPUNIT_ASSERT(failover->isConnectedToPriority() == false);

    Thread::sleep(100);

    broker1->start();
    broker1->waitUntilStarted();

    broker2->stop();
    broker2->waitUntilStopped();

    for (int i = 0; i < 2; ++i) {

        CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted in time", listener.awaitInterruption());
        CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", listener.awaitResumed());
        listener.reset();

        URI connectedURI = URI(transport->getRemoteAddress());

        if (connectedURI.getPort() == broker1->getPort()) {
            break;
        }
    }

    CPPUNIT_ASSERT(failover->isConnected() == true);
    CPPUNIT_ASSERT(failover->isConnectedToPriority() == true);

    transport->close();

    broker1->stop();
    broker1->waitUntilStopped();

    broker2->stop();
    broker2->waitUntilStopped();

    broker3->stop();
    broker3->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testPriorityBackupRapidSwitchingOnRestore() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    broker1->start();
    broker1->waitUntilStarted();

    broker2->start();
    broker2->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?randomize=false&priorityBackup=true";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);

    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);
    CPPUNIT_ASSERT(failover->isRandomize() == false);
    CPPUNIT_ASSERT(failover->isPriorityBackup() == true);

    transport->start();

    CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", listener.awaitResumed());
    listener.reset();

    CPPUNIT_ASSERT(failover->isConnected() == true);
    CPPUNIT_ASSERT(failover->isConnectedToPriority() == true);

    broker1->stop();
    broker1->waitUntilStopped();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted in time", listener.awaitInterruption());
    CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", listener.awaitResumed());
    listener.reset();

    CPPUNIT_ASSERT(failover->isConnected() == true);
    CPPUNIT_ASSERT(failover->isConnectedToPriority() == false);

    broker1->start();
    broker1->waitUntilStarted();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted in time", listener.awaitInterruption());
    CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", listener.awaitResumed());
    listener.reset();

    CPPUNIT_ASSERT(failover->isConnected() == true);
    CPPUNIT_ASSERT(failover->isConnectedToPriority() == true);

    transport->close();

    broker1->stop();
    broker1->waitUntilStopped();

    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testSimpleBrokerRestart() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Start both brokers
    broker1->start();
    broker1->waitUntilStarted();

    broker2->start();
    broker2->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?maxReconnectDelay=1000";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);

    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);

    transport->start();

    CPPUNIT_ASSERT_MESSAGE("Failed to connect initially", listener.awaitResumed());

    // Small delay to ensure connection state is fully updated after resume callback
    Thread::sleep(100);

    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    // Stop broker1, should failover to broker2
    broker1->stop();
    broker1->waitUntilStopped();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted", listener.awaitInterruption());
    CPPUNIT_ASSERT_MESSAGE("Failed to reconnect", listener.awaitResumed());

    // Small delay to ensure connection state is fully updated after resume callback
    Thread::sleep(100);

    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    // Restart broker1
    broker1->start();
    broker1->waitUntilStarted();

    // Allow time for backup connections (2 seconds is usually sufficient)
    Thread::sleep(2000);

    // Stop broker2 - should failover to broker1 (if backup ready) or reconnect
    broker2->stop();
    broker2->waitUntilStopped();

    // Wait for interruption - should happen when broker2 goes down
    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted after broker2 stop", listener.awaitInterruption());
    // Wait for resume - should happen when reconnected to broker1
    CPPUNIT_ASSERT_MESSAGE("Failed to reconnect to broker1", listener.awaitResumed());

    // Small delay to ensure connection state is fully updated
    Thread::sleep(100);

    listener.reset();
    CPPUNIT_ASSERT_MESSAGE("Should be connected after failover", failover->isConnected() == true);

    // Final connection check
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();

    broker1->stop();
    broker1->waitUntilStopped();

    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testBrokerRestartWithProperSync() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Start both brokers
    broker1->start();
    broker1->waitUntilStarted();

    broker2->start();
    broker2->waitUntilStarted();

    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?maxReconnectDelay=1000";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);

    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);

    transport->start();

    // Wait for initial connection using listener, not sleep
    CPPUNIT_ASSERT_MESSAGE("Failed to connect initially", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    // Stop broker1, should failover to broker2
    broker1->stop();
    broker1->waitUntilStopped();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted", listener.awaitInterruption());
    CPPUNIT_ASSERT_MESSAGE("Failed to reconnect", listener.awaitResumed());
    listener.reset();
    CPPUNIT_ASSERT(failover->isConnected() == true);

    // Restart broker1 - wait for transport to be ready
    broker1->start();
    broker1->waitUntilStarted();

    // Give backup connections time to establish (this is asynchronous in FailoverTransport)
    // We can't eliminate this completely without modifying FailoverTransport to notify
    // when backup connections are ready, but 3 seconds should be sufficient
    Thread::sleep(3000);

    // Stop broker2, should reconnect to broker1
    broker2->stop();
    broker2->waitUntilStopped();

    CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted after broker2 stop", listener.awaitInterruption());
    CPPUNIT_ASSERT_MESSAGE("Failed to reconnect to broker1", listener.awaitResumed());
    listener.reset();

    // Now we're guaranteed to be connected before checking
    CPPUNIT_ASSERT_MESSAGE("Should be connected to broker1", failover->isConnected() == true);

    // Restart broker2
    broker2->start();
    broker2->waitUntilStarted();

    // Give backup connections time to re-establish
    Thread::sleep(3000);

    // Final check - we're still connected
    CPPUNIT_ASSERT_MESSAGE("Should still be connected", failover->isConnected() == true);

    transport->close();

    broker1->stop();
    broker1->waitUntilStopped();

    broker2->stop();
    broker2->waitUntilStopped();
}

////////////////////////////////////////////////////////////////////////////////
void FailoverTransportTest::testFuzzyBrokerAvailability() {

    Pointer<MockBrokerService> broker1(new MockBrokerService(61626));
    Pointer<MockBrokerService> broker2(new MockBrokerService(61628));

    // Start with both brokers online
    broker1->start();
    broker1->waitUntilStarted();

    broker2->start();
    broker2->waitUntilStarted();

    // Give brokers extra time to fully initialize
    Thread::sleep(500);

    std::string uri = "failover://(tcp://localhost:61626,"
                                  "tcp://localhost:61628)?initialReconnectDelay=100&maxReconnectDelay=500&useExponentialBackOff=false";

    PriorityBackupListener listener;
    FailoverTransportFactory factory;

    Pointer<Transport> transport(factory.create(uri));
    CPPUNIT_ASSERT(transport != NULL);

    transport->setTransportListener(&listener);

    FailoverTransport* failover =
        dynamic_cast<FailoverTransport*>(transport->narrow(typeid(FailoverTransport)));

    CPPUNIT_ASSERT(failover != NULL);

    transport->start();

    // Wait for initial connection with extended timeout
    int retries = 0;
    while (!listener.awaitResumed() && retries++ < 3) {
        Thread::sleep(1000);
    }
    CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", failover->isConnected());
    listener.reset();

    CPPUNIT_ASSERT(failover->isConnected() == true);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> actionDist(0, 3);
    std::uniform_int_distribution<> delayDist(100, 500);

    bool broker1Running = true;
    bool broker2Running = true;

    // Perform 10 random broker start/stop cycles
    for (int i = 0; i < 10; ++i) {

        // Random action: 0=stop broker1, 1=stop broker2, 2=start broker1, 3=start broker2
        int action = actionDist(gen);

        // Ensure at least one broker stays online
        bool canStopBroker1 = broker1Running && broker2Running;
        bool canStopBroker2 = broker1Running && broker2Running;

        switch (action) {
            case 0: // Try to stop broker1
                if (canStopBroker1) {
                    broker1->stop();
                    broker1->waitUntilStopped();
                    broker1Running = false;

                    if (broker2Running) {
                        CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted in time", listener.awaitInterruption());
                        CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", listener.awaitResumed());
                        listener.reset();
                    }
                }
                break;

            case 1: // Try to stop broker2
                if (canStopBroker2) {
                    broker2->stop();
                    broker2->waitUntilStopped();
                    broker2Running = false;

                    if (broker1Running) {
                        CPPUNIT_ASSERT_MESSAGE("Failed to get interrupted in time", listener.awaitInterruption());
                        CPPUNIT_ASSERT_MESSAGE("Failed to get reconnected in time", listener.awaitResumed());
                        listener.reset();
                    }
                }
                break;

            case 2: // Try to start broker1
                if (!broker1Running) {
                    broker1->start();
                    broker1->waitUntilStarted();
                    broker1Running = true;

                    // Give time for potential reconnection
                    Thread::sleep(500);
                }
                break;

            case 3: // Try to start broker2
                if (!broker2Running) {
                    broker2->start();
                    broker2->waitUntilStarted();
                    broker2Running = true;

                    // Give time for potential reconnection
                    Thread::sleep(500);
                }
                break;
        }

        // Small delay between actions
        Thread::sleep(100 + delayDist(gen));

        // Verify connection state - should be connected if at least one broker is running
        if (broker1Running || broker2Running) {
            // Allow some time for reconnection if needed
            int retries = 0;
            while (!failover->isConnected() && retries < 50) {
                Thread::sleep(100);
                retries++;
            }
            CPPUNIT_ASSERT_MESSAGE("Should be connected when at least one broker is online",
                                   failover->isConnected());
        }
    }

    // Ensure both brokers are running at the end for clean shutdown
    if (!broker1Running) {
        broker1->start();
        broker1->waitUntilStarted();
        broker1Running = true;
    }
    if (!broker2Running) {
        broker2->start();
        broker2->waitUntilStarted();
        broker2Running = true;
    }

    Thread::sleep(1000);
    CPPUNIT_ASSERT(failover->isConnected() == true);

    transport->close();

    broker1->stop();
    broker1->waitUntilStopped();

    broker2->stop();
    broker2->waitUntilStopped();
}
