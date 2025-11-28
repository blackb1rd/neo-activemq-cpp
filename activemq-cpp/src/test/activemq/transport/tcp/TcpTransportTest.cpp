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

#include "TcpTransportTest.h"

#include <activemq/transport/tcp/TcpTransportFactory.h>
#include <activemq/transport/tcp/TcpTransport.h>

#include <activemq/wireformat/openwire/OpenWireFormat.h>

#include <decaf/lang/Pointer.h>
#include <decaf/lang/Integer.h>
#include <decaf/net/Socket.h>
#include <decaf/net/SocketFactory.h>
#include <decaf/net/ServerSocket.h>
#include <decaf/net/SocketTimeoutException.h>
#include <decaf/io/InputStream.h>
#include <decaf/io/OutputStream.h>
#include <decaf/util/Random.h>

using namespace decaf;
using namespace decaf::lang;
using namespace decaf::net;
using namespace decaf::io;
using namespace decaf::util;
using namespace decaf::util::concurrent;
using namespace activemq;
using namespace activemq::wireformat;
using namespace activemq::wireformat::openwire;
using namespace activemq::transport;
using namespace activemq::transport::tcp;

////////////////////////////////////////////////////////////////////////////////
TcpTransportTest::TcpTransportTest() {
}

////////////////////////////////////////////////////////////////////////////////
TcpTransportTest::~TcpTransportTest() {
}

////////////////////////////////////////////////////////////////////////////////
namespace {

    class TestServer : public lang::Thread{
    private:

        bool done;
        bool error;
        Pointer<ServerSocket> server;
        Pointer<OpenWireFormat> wireFormat;
        CountDownLatch started;
        Random rand;

    public:

        TestServer() : Thread(), done(false), error(false), server(), started(1), rand() {
            server.reset(new ServerSocket(0));
            server->setSoTimeout(100); // 100ms timeout for quick shutdown response

            Properties properties;
            this->wireFormat.reset(new OpenWireFormat(properties));

            this->rand.setSeed(System::currentTimeMillis());
        }

        virtual ~TestServer() {
            stop();
        }

        int getLocalPort() {
            if (this->server.get() != NULL) {
                return server->getLocalPort();
            }

            return 0;
        }

        void waitUntilStarted() {
            this->started.await();
        }

        void waitUntilStopped() {
            // Thread should exit cleanly now that done is set and socket timeout allows periodic checks
            this->join();
        }

        void stop() {
            try {
                done = true;

                // On Windows, closing the socket doesn't always interrupt accept()
                // Unblock it by connecting to it BEFORE closing
                try {
                    int port = getLocalPort();
                    if (port > 0 && server.get() != NULL) {
                        Pointer<Socket> wakeupSocket(SocketFactory::getDefault()->createSocket());
                        try {
                            wakeupSocket->connect("127.0.0.1", port, 100);
                            wakeupSocket->close();
                        } catch (...) {}
                    }
                } catch (...) {}

                // Now close the server socket
                if (server.get() != NULL) {
                    try {
                        server->close();
                    } catch (...) {}
                }
            } catch (...) {}
        }

        virtual void run() {
            try {

                started.countDown();

                while (!done) {
                    try {
                        std::unique_ptr<Socket> socket(server->accept());
                        socket->setSoLinger(false, 0);

                        // Immediate fail sometimes.
                        if (rand.nextBoolean()) {
                            socket->close();
                            continue;
                        }

                        OutputStream* os = socket->getOutputStream();
                        DataOutputStream dataOut(os);

                        InputStream* is = socket->getInputStream();
                        DataInputStream dataIn(is);

                        // random sleep before terminate
                        TimeUnit::MILLISECONDS.sleep(rand.nextInt(20));

                        socket->close();
                    } catch (SocketTimeoutException& ste) {
                        // Timeout on accept - check done flag and continue
                        // This is expected and allows periodic checking of done flag
                        continue;
                    } catch (IOException& io) {
                        // IOException during accept usually means socket was closed
                        // Check if we're shutting down
                        if (done) {
                            break;
                        }
                        // Otherwise it's an actual error
                        error = true;
                        break;
                    }
                }

            } catch (IOException& ex) {
                // Only set error if not shutting down
                if (!done) {
                    error = true;
                }
            } catch (...) {
                if (!done) {
                    error = true;
                }
            }
        }
    };

    TestServer* server;
}

////////////////////////////////////////////////////////////////////////////////
void TcpTransportTest::setUp() {

    server = new TestServer();
    server->start();
    server->waitUntilStarted();
}

////////////////////////////////////////////////////////////////////////////////
void TcpTransportTest::tearDown() {

    try {
        if (server == NULL) {
            return;
        }

        server->stop();
        server->waitUntilStopped();
        delete server;
    } catch (...) {
        delete server;
    }
}

////////////////////////////////////////////////////////////////////////////////
void TcpTransportTest::testTransportCreateWithRadomFailures() {

    Properties properties;
    OpenWireFormat wireFormat(properties);
    TcpTransportFactory factory;

    int port = server->getLocalPort();
    URI connectUri("tcp://localhost:" + Integer::toString(port));

    Pointer<Transport> transport;

    // Test rapid creation with random connect failures.
    for (int i = 0; i < 1000; ++i) {
        try {
             transport = factory.create(connectUri);
        } catch (Exception& ex) {}

        try {
            transport->start();
        } catch (Exception& ex) {}

        try {
            transport->close();
        } catch (Exception& ex) {}
    }
}
