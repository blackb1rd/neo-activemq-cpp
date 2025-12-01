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

#ifndef _ACTIVEMQ_TRANSPORT_TCP_SSLTRANSPORTTEST_H_
#define _ACTIVEMQ_TRANSPORT_TCP_SSLTRANSPORTTEST_H_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <activemq/util/Config.h>

namespace activemq {
namespace transport {
namespace tcp {

    /**
     * Test suite for SSL Transport functionality including connection handling,
     * SSL handshake, timeout scenarios, and error conditions.
     */
    class SslTransportTest : public CppUnit::TestFixture {

        CPPUNIT_TEST_SUITE( SslTransportTest );
        CPPUNIT_TEST( testSslTransportCreate );
        CPPUNIT_TEST( testSslHandshakeAfterConnect );
        CPPUNIT_TEST( testSslConnectionWithServerName );
        CPPUNIT_TEST( testSslConnectionFailureHandling );
        CPPUNIT_TEST( testSslTransportWithProperties );
        CPPUNIT_TEST_SUITE_END();

    public:

        SslTransportTest();
        virtual ~SslTransportTest();

        virtual void setUp();
        virtual void tearDown();

        /**
         * Tests basic SSL transport creation and configuration.
         */
        void testSslTransportCreate();

        /**
         * Tests that SSL handshake is performed immediately after TCP connection.
         * This is the main fix for the timeout issue.
         */
        void testSslHandshakeAfterConnect();

        /**
         * Tests SSL connection timeout scenarios to ensure proper error handling.
         */
        void testSslConnectionTimeout();

        /**
         * Tests SSL connection with SNI (Server Name Indication) configuration.
         */
        void testSslConnectionWithServerName();

        /**
         * Tests that SSL connection failures are properly handled and reported.
         */
        void testSslConnectionFailureHandling();

        /**
         * Tests SSL transport configuration using URI properties.
         */
        void testSslTransportWithProperties();

    };

}}}

#endif /* _ACTIVEMQ_TRANSPORT_TCP_SSLTRANSPORTTEST_H_ */
