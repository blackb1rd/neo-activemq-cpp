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

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/TestListener.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/Outputter.h>
#include <cppunit/XmlOutputter.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestResult.h>
#include <util/teamcity/TeamCityProgressListener.h>
#include <activemq/util/Config.h>
#include <activemq/library/ActiveMQCPP.h>
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <vector>

#ifdef __linux__
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <cxxabi.h>
#include <dirent.h>
#include <cstring>
#include <sys/syscall.h>
#endif

#ifdef __linux__
// Print stack trace with demangled symbols
void printStackTrace(int threadId = 0) {
    const int maxFrames = 2048;
    void* buffer[maxFrames];

    int numFrames = backtrace(buffer, maxFrames);
    char** symbols = backtrace_symbols(buffer, numFrames);

    if (threadId > 0) {
        std::cerr << "\n=== Stack Trace for Thread " << threadId << " ===" << std::endl;
    } else {
        std::cerr << "\n=== Stack Trace ===" << std::endl;
    }

    if (symbols == nullptr) {
        std::cerr << "Failed to get stack trace symbols" << std::endl;
        return;
    }

    for (int i = 0; i < numFrames; i++) {
        std::string frame(symbols[i]);
        std::cerr << "#" << i << " ";

        // Try to demangle C++ symbols
        size_t start = frame.find('(');
        size_t end = frame.find('+', start);

        if (start != std::string::npos && end != std::string::npos) {
            std::string mangled = frame.substr(start + 1, end - start - 1);
            int status;
            char* demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);

            if (status == 0 && demangled) {
                std::cerr << frame.substr(0, start + 1) << demangled
                         << frame.substr(end) << std::endl;
                free(demangled);
            } else {
                std::cerr << frame << std::endl;
            }
        } else {
            std::cerr << frame << std::endl;
        }
    }

    free(symbols);
    std::cerr << "===================" << std::endl;
}

// Thread-local storage to identify threads
static thread_local int threadNumber = 0;
static std::atomic<int> nextThreadNumber{0};

// Signal handler for stack trace dumping
void stackTraceSignalHandler(int sig) {
    if (sig == SIGUSR1) {
        printStackTrace(threadNumber);
    }
}

// Setup signal handler for stack trace
void setupStackTraceHandler() {
    struct sigaction sa;
    sa.sa_handler = stackTraceSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, nullptr);
}

// Enumerate all threads in the process
std::vector<pid_t> getAllThreadIds() {
    std::vector<pid_t> threadIds;
    pid_t pid = getpid();

    char taskPath[256];
    snprintf(taskPath, sizeof(taskPath), "/proc/%d/task", pid);

    DIR* dir = opendir(taskPath);
    if (!dir) {
        std::cerr << "Failed to open /proc/" << pid << "/task" << std::endl;
        return threadIds;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            // Check if the name is a number (thread ID)
            char* endptr;
            long tid = strtol(entry->d_name, &endptr, 10);
            if (*endptr == '\0' && tid > 0) {
                threadIds.push_back(static_cast<pid_t>(tid));
            }
        }
    }

    closedir(dir);
    return threadIds;
}

// Dump stack traces of all threads
void dumpAllThreadStackTraces() {
    std::cerr << "\n========================================" << std::endl;
    std::cerr << "Dumping stack traces of all threads..." << std::endl;
    std::cerr << "========================================" << std::endl;

    std::vector<pid_t> threadIds = getAllThreadIds();

    if (threadIds.empty()) {
        std::cerr << "No threads found" << std::endl;
        return;
    }

    std::cerr << "Found " << threadIds.size() << " thread(s)" << std::endl;

    for (size_t i = 0; i < threadIds.size(); i++) {
        pid_t tid = threadIds[i];
        std::cerr << "\nThread " << (i + 1) << "/" << threadIds.size()
                  << " (TID: " << tid << ")" << std::endl;

        // Send signal to thread to dump its stack
        if (syscall(SYS_tgkill, getpid(), tid, SIGUSR1) == 0) {
            // Give thread time to print stack trace
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            std::cerr << "Failed to send signal to thread " << tid << std::endl;
        }
    }

    std::cerr << "\n========================================" << std::endl;
    std::cerr << "End of stack traces" << std::endl;
    std::cerr << "========================================" << std::endl;
}
#endif

class TestRunner {
private:
    CppUnit::TextUi::TestRunner* runner;
    std::string testPath;
    std::atomic<bool> wasSuccessful;
    std::atomic<bool> completed;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;

public:
    TestRunner(CppUnit::TextUi::TestRunner* r, const std::string& path)
        : runner(r), testPath(path), wasSuccessful(false), completed(false) {
    }

    void start() {
        thread = std::thread([this]() {
#ifdef __linux__
            // Assign a thread number for identification in stack traces
            threadNumber = ++nextThreadNumber;
#endif
            try {
                bool result = runner->run(testPath, false);
                wasSuccessful.store(result);
            } catch (...) {
                wasSuccessful.store(false);
            }
            completed.store(true);
            cv.notify_all();
        });
    }

    bool waitFor(long long timeoutMillis) {
        if (timeoutMillis <= 0) {
            if (thread.joinable()) {
                thread.join();
            }
            return completed.load();
        }

        std::unique_lock<std::mutex> lock(mutex);
        auto timeout = std::chrono::milliseconds(timeoutMillis);

        if (cv.wait_for(lock, timeout, [this]() { return completed.load(); })) {
            if (thread.joinable()) {
                thread.join();
            }
            return true;
        }

        return false; // Timeout occurred
    }

    bool getResult() const {
        return wasSuccessful.load();
    }

    bool isCompleted() const {
        return completed.load();
    }

    void dumpStackTrace() {
#ifdef __linux__
        dumpAllThreadStackTraces();
#else
        std::cerr << "\nStack trace dumping not supported on this platform" << std::endl;
#endif
    }

    ~TestRunner() {
        if (thread.joinable()) {
            thread.detach();
        }
    }
};

int main( int argc, char **argv ) {

#ifdef __linux__
    setupStackTraceHandler();
#endif

    activemq::library::ActiveMQCPP::initializeLibrary();

    bool wasSuccessful = false;
    int iterations = 1;
    std::ofstream outputFile;
    bool useXMLOutputter = false;
    std::string testPath = "";
    long long timeoutSeconds = 0; // 0 means no timeout
    std::unique_ptr<CppUnit::TestListener> listener( new CppUnit::BriefTestProgressListener );

    if( argc > 1 ) {
        for( int i = 1; i < argc; ++i ) {
            const std::string arg( argv[i] );
            if( arg == "-runs" ) {
                if( ( i + 1 ) >= argc ) {
                    std::cout << "-runs requires a value for the iteration count" << std::endl;
                    return -1;
                }
                try {
                    iterations = std::stoi( argv[++i] );
                } catch( std::exception& ex ) {
                    std::cout << "Invalid iteration count specified on command line: "
                              << argv[i] << std::endl;
                    return -1;
                }
            } else if( arg == "-teamcity" ) {
                listener.reset( new test::util::teamcity::TeamCityProgressListener() );
            } else if( arg == "-quiet" ) {
                listener.reset( NULL );
            } else if( arg == "-xml" ) {
                if( ( i + 1 ) >= argc ) {
                    std::cout << "-xml requires a filename to be specified" << std::endl;
                    return -1;
                }

                std::ofstream outputFile( argv[++i] );
                useXMLOutputter = true;
            } else if( arg == "-test" ) {
                if( ( i + 1 ) >= argc ) {
                    std::cout << "-test requires a test name or path to be specified" << std::endl;
                    return -1;
                }
                testPath = argv[++i];
            } else if( arg == "-timeout" ) {
                if( ( i + 1 ) >= argc ) {
                    std::cout << "-timeout requires a timeout value in seconds" << std::endl;
                    return -1;
                }
                try {
                    timeoutSeconds = std::stoll( argv[++i] );
                    if( timeoutSeconds < 0 ) {
                        std::cout << "Timeout value must be positive" << std::endl;
                        return -1;
                    }
                } catch( std::exception& ex ) {
                    std::cout << "Invalid timeout value specified on command line: "
                              << argv[i] << std::endl;
                    return -1;
                }
            } else if( arg == "-help" || arg == "--help" || arg == "-h" ) {
                std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
                std::cout << "Options:" << std::endl;
                std::cout << "  -runs <count>      Run tests multiple times" << std::endl;
                std::cout << "  -test <name>       Run specific test or test suite" << std::endl;
                std::cout << "                     Examples: -test decaf::lang::MathTest" << std::endl;
                std::cout << "                               -test decaf::lang::MathTest::test_absD" << std::endl;
                std::cout << "  -timeout <sec>     Set test timeout in seconds (0 = no timeout)" << std::endl;
                std::cout << "  -teamcity          Use TeamCity progress listener" << std::endl;
                std::cout << "  -quiet             Suppress test progress output" << std::endl;
                std::cout << "  -xml <file>        Output results in XML format" << std::endl;
                std::cout << "  -help, --help, -h  Show this help message" << std::endl;
                activemq::library::ActiveMQCPP::shutdownLibrary();
                return 0;
            }
        }
    }

    for( int i = 0; i < iterations; ++i ) {

        CppUnit::TextUi::TestRunner runner;
        CppUnit::TestFactoryRegistry &registry = CppUnit::TestFactoryRegistry::getRegistry();
        runner.addTest( registry.makeTest() );

        // Shows a message as each test starts
        if( listener.get() != NULL ) {
            runner.eventManager().addListener( listener.get() );
        }

        // Specify XML output and inform the test runner of this format.  The TestRunner
        // will delete the passed XmlOutputter for us.
        if( useXMLOutputter ) {
            runner.setOutputter( new CppUnit::XmlOutputter( &runner.result(), outputFile ) );
        } else {
            // Use CompilerOutputter for better error formatting with stack traces
            runner.setOutputter( new CppUnit::CompilerOutputter( &runner.result(), std::cerr ) );
        }

        if( timeoutSeconds > 0 ) {
            TestRunner testRunner(&runner, testPath);
            testRunner.start();

            bool completed = testRunner.waitFor(timeoutSeconds * 1000);

            if( !completed ) {
                std::cerr << std::endl << "ERROR: Test execution timed out after "
                          << timeoutSeconds << " seconds" << std::endl;

                // Dump stack traces of all threads before terminating
                testRunner.dumpStackTrace();

                std::cerr << "Forcibly terminating process due to timeout..." << std::endl;
                if( useXMLOutputter ) {
                    outputFile.close();
                }
                // Force exit since we can't safely stop the test thread
                std::_Exit(-1);
            }

            wasSuccessful = testRunner.getResult();

            // If tests failed (but didn't timeout), dump stack traces
            if( !wasSuccessful ) {
                std::cerr << std::endl << "ERROR: Test execution failed" << std::endl;
                testRunner.dumpStackTrace();
            }
        } else {
            wasSuccessful = runner.run( testPath, false );

            // If tests failed, dump stack traces
            if( !wasSuccessful ) {
                std::cerr << std::endl << "ERROR: Test execution failed" << std::endl;
#ifdef __linux__
                dumpAllThreadStackTraces();
#else
                std::cerr << "\nStack trace dumping not supported on this platform" << std::endl;
#endif
            }
        }

        if( useXMLOutputter ) {
            outputFile.close();
        }
    }

    activemq::library::ActiveMQCPP::shutdownLibrary();

    return !wasSuccessful;
}

