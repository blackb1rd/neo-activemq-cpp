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

#include "MockBrokerService.h"

#include <activemq/wireformat/openwire/OpenWireFormatFactory.h>
#include <activemq/wireformat/openwire/OpenWireFormat.h>
#include <activemq/wireformat/openwire/OpenWireResponseBuilder.h>
#include <activemq/commands/Command.h>
#include <activemq/commands/Response.h>
#include <activemq/commands/WireFormatInfo.h>
#include <activemq/transport/mock/MockTransport.h>

#include <decaf/net/ServerSocket.h>
#include <decaf/net/Socket.h>
#include <decaf/net/SocketTimeoutException.h>
#include <decaf/lang/Integer.h>
#include <decaf/lang/Pointer.h>
#include <decaf/util/Random.h>
#include <decaf/util/Properties.h>
#include <decaf/util/concurrent/CountDownLatch.h>
#include <decaf/io/InputStream.h>
#include <decaf/io/OutputStream.h>
#include <decaf/io/EOFException.h>

#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace activemq;
using namespace activemq::mock;
using namespace activemq::commands;
using namespace activemq::wireformat;
using namespace activemq::wireformat::openwire;
using namespace activemq::transport::mock;
using namespace decaf;
using namespace decaf::lang;
using namespace decaf::io;
using namespace decaf::util;
using namespace decaf::util::concurrent;
using namespace decaf::net;

////////////////////////////////////////////////////////////////////////////////
namespace activemq {
namespace mock {

    class TcpServer : public lang::Thread {
    private:

        std::atomic<bool> done;
        std::atomic<bool> error;
        const int configuredPort;
        Pointer<ServerSocket> server;
        Pointer<Socket> clientSocket;
        std::mutex socketMutex;
        std::mutex startedMutex;
        std::condition_variable startedCondition;
        bool serverStarted;
        Pointer<OpenWireFormat> wireFormat;
        Pointer<OpenWireResponseBuilder> responeBuilder;
        Random rand;

    public:

        TcpServer() : Thread(), done(false), error(false), configuredPort(0), server(), clientSocket(), wireFormat(),
                      responeBuilder(), rand(), serverStarted(false) {

            Properties properties;

            this->wireFormat = OpenWireFormatFactory().createWireFormat(properties).dynamicCast<OpenWireFormat>();
            this->responeBuilder.reset(new OpenWireResponseBuilder());

            this->rand.setSeed(System::currentTimeMillis());
        }

        TcpServer(int port) : Thread(), done(false), error(false), configuredPort(port), server(), clientSocket(), wireFormat(),
                              responeBuilder(), rand(), serverStarted(false) {

            Properties properties;
            this->wireFormat = OpenWireFormatFactory().createWireFormat(properties).dynamicCast<OpenWireFormat>();
            this->responeBuilder.reset(new OpenWireResponseBuilder());

            this->rand.setSeed(System::currentTimeMillis());
        }

        virtual ~TcpServer() {
            stop();
            waitUntilStopped();
        }

        int getLocalPort() {
            if (this->server.get() != NULL) {
                return server->getLocalPort();
            }

            return 0;
        }

        void waitUntilStarted() {
            std::unique_lock<std::mutex> lock(startedMutex);
            startedCondition.wait(lock, [this]{ return serverStarted || error; });
            if (error && !serverStarted) {
                throw IOException(__FILE__, __LINE__, "Mock broker failed to start");
            }
        }

        void waitUntilStopped() {
            this->join();

            // Now that thread has exited, safely close server socket
            std::lock_guard<std::mutex> lock(socketMutex);
            if (server.get() != NULL) {
                try {
                    server->close();
                } catch (...) {}
                server.reset(NULL);
            }
        }

        void stop() {
            try {
                done.store(true, std::memory_order_release);

                // Only close the client socket to unblock any pending read/write operations
                // DO NOT close server socket here - causes exception unwinding deadlock
                // The 1-second timeout on accept() ensures thread exits cleanly
                std::lock_guard<std::mutex> lock(socketMutex);

                if (clientSocket.get() != NULL) {
                    try {
                        clientSocket->close();
                    } catch (...) {}
                }
            } catch (...) {}
        }

        virtual void run() {
            try {
                MockTransport mock(this->wireFormat, this->responeBuilder);

                // Create and bind the server socket once
                try {
                    server.reset(new ServerSocket(configuredPort));
                    server->setReuseAddress(true);
                    server->setSoTimeout(1000); // 1 second timeout on accept
                } catch (IOException& e) {
                    // Failed to create/bind server socket - notify and exit
                    error.store(true, std::memory_order_release);
                    {
                        std::lock_guard<std::mutex> lock(startedMutex);
                        serverStarted = false;
                    }
                    startedCondition.notify_all();
                    return;
                }

                // Signal that server is now listening on the port
                {
                    std::lock_guard<std::mutex> lock(startedMutex);
                    serverStarted = true;
                }
                startedCondition.notify_all();

                while (!done.load(std::memory_order_acquire)) {
                    Socket* socketPtr = NULL;
                    try {
                        // Check done flag before blocking accept call to avoid
                        // exception unwinding issues during shutdown
                        if (done.load(std::memory_order_acquire)) {
                            break;
                        }

                        socketPtr = server->accept();

                        // Check done again after accept to avoid processing during shutdown
                        if (done.load(std::memory_order_acquire)) {
                            if (socketPtr != NULL) {
                                try {
                                    delete socketPtr;
                                } catch (...) {}
                            }
                            break;
                        }

                        std::lock_guard<std::mutex> lock(socketMutex);
                        clientSocket.reset(socketPtr);
                    } catch (SocketTimeoutException& ste) {
                        // Timeout on accept - check done flag and continue
                        // This is expected and allows periodic checking of done flag
                        continue;
                    } catch (...) {
                        // Accept failed or server closed
                        // Clean up socketPtr if allocated
                        if (socketPtr != NULL) {
                            try {
                                delete socketPtr;
                            } catch (...) {}
                        }
                        if (done.load(std::memory_order_acquire)) {
                            break;
                        }
                        continue;
                    }

                    clientSocket->setSoLinger(false, 0);
                    // Set socket timeout to allow thread to check done flag periodically
                    clientSocket->setSoTimeout(1000); // 1 second timeout

                    Pointer<WireFormatInfo> preferred = wireFormat->getPreferedWireFormatInfo();

                    try {
                        {
                            OutputStream* os = clientSocket->getOutputStream();
                            DataOutputStream dataOut(os);

                            InputStream* is = clientSocket->getInputStream();
                            DataInputStream dataIn(is);

                            wireFormat->marshal(preferred, &mock, &dataOut);
                            dataOut.flush();

                            while (!done.load(std::memory_order_acquire)) {
                                try {
                                    Pointer<Command> command = wireFormat->unmarshal(&mock, &dataIn);
                                    Pointer<Response> response = responeBuilder->buildResponse(command);

                                    if (response != NULL) {
                                        wireFormat->marshal(response, &mock, &dataOut);
                                    }
                                } catch (SocketTimeoutException& ste) {
                                    // Timeout allows us to check done flag
                                    continue;
                                } catch (IOException& ioe) {
                                    // Socket closed or connection error
                                    break;
                                }
                            }
                            // DataInputStream and DataOutputStream will be destroyed here,
                            // before the socket is closed
                        }
                    } catch (IOException& ioe) {
                        // Failed during handshake or stream operations
                        // Clean up and wait for next connection
                    } catch (...) {
                        // Unexpected error
                    }

                    // Clean up the client socket
                    {
                        std::lock_guard<std::mutex> lock(socketMutex);
                        if (clientSocket.get() != NULL) {
                            try {
                                clientSocket->close();
                            } catch (...) {}
                            clientSocket.reset(NULL);
                        }
                    }
                }
            } catch (IOException& ex) {
                error.store(true, std::memory_order_release);
            } catch (Exception& ex) {
                error.store(true, std::memory_order_release);
            } catch (...) {
                error.store(true, std::memory_order_release);
            }
        }
    };

    class MockBrokerServiceImpl {
    private:

        MockBrokerServiceImpl(const MockBrokerServiceImpl&);
        MockBrokerServiceImpl& operator= (const MockBrokerServiceImpl&);

    public:

        Pointer<TcpServer> server;
        int configuredPort;

    public:

        MockBrokerServiceImpl() : server(), configuredPort(0) {
        }

        MockBrokerServiceImpl(int port) : server(), configuredPort(port) {
        }

    };

}}

////////////////////////////////////////////////////////////////////////////////
MockBrokerService::MockBrokerService() : impl(new MockBrokerServiceImpl()) {

}

////////////////////////////////////////////////////////////////////////////////
MockBrokerService::MockBrokerService(int port) : impl(new MockBrokerServiceImpl(port)) {

}

////////////////////////////////////////////////////////////////////////////////
MockBrokerService::~MockBrokerService() {
    try {
        stop();
    }
    AMQ_CATCHALL_NOTHROW()

    try {
        delete impl;
    }
    AMQ_CATCHALL_NOTHROW()
}

////////////////////////////////////////////////////////////////////////////////
void MockBrokerService::start() {
    // Create a new server thread each time to support restart
    if (impl->server.get() != NULL) {
        // Make sure previous server is stopped
        impl->server->stop();
        impl->server->waitUntilStopped();
    }

    if (impl->configuredPort != 0) {
        impl->server.reset(new TcpServer(impl->configuredPort));
    } else {
        impl->server.reset(new TcpServer());
    }
    impl->server->start();
}

////////////////////////////////////////////////////////////////////////////////
void MockBrokerService::stop() {
    if (impl->server.get() != NULL) {
        impl->server->stop();
    }
}

////////////////////////////////////////////////////////////////////////////////
void MockBrokerService::waitUntilStarted() {
    if (impl->server.get() != NULL) {
        impl->server->waitUntilStarted();
    }
}

////////////////////////////////////////////////////////////////////////////////
void MockBrokerService::waitUntilStopped() {
    if (impl->server.get() != NULL) {
        impl->server->waitUntilStopped();
    }
}

////////////////////////////////////////////////////////////////////////////////
int MockBrokerService::getPort() const {
    if (impl->server.get() != NULL) {
        return impl->server->getLocalPort();
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
std::string MockBrokerService::getConnectString() const {
    int port = getPort();
    return std::string("tcp://localhost:") + Integer::toString(port);
}
