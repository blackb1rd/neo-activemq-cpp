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

#include <decaf/util/concurrent/Mutex.h>

#include <decaf/internal/util/concurrent/CustomReentrantLock.h>
#include <decaf/internal/util/concurrent/Threading.h>
#include <decaf/internal/util/concurrent/ThreadingTypes.h>
#include <decaf/lang/Integer.h>
#include <decaf/lang/Thread.h>

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>

using namespace decaf;
using namespace decaf::util;
using namespace decaf::util::concurrent;
using namespace decaf::lang;
using namespace decaf::lang::exceptions;
using decaf::internal::util::concurrent::CustomReentrantLock;

////////////////////////////////////////////////////////////////////////////////
namespace decaf {
namespace util {
namespace concurrent {

    /**
     * Internal implementation using CustomReentrantLock.
     * Supports recursive locking - the same thread can lock multiple times
     * without deadlocking. Uses std::condition_variable for wait/notify.
     */
    class MutexProperties {
    private:

        MutexProperties(const MutexProperties&);
        MutexProperties& operator=(const MutexProperties&);

    public:

        MutexProperties() {
            std::string idStr = Integer::toString(++id);
            this->name.reserve(DEFAULT_NAME_PREFIX.length() + idStr.length());
            this->name.append(DEFAULT_NAME_PREFIX);
            this->name.append(idStr);
        }

        MutexProperties(const std::string& name) : name(name) {
            if (this->name.empty()) {
                std::string idStr = Integer::toString(++id);
                this->name.reserve(DEFAULT_NAME_PREFIX.length() + idStr.length());
                this->name.append(DEFAULT_NAME_PREFIX);
                this->name.append(idStr);
            }
        }

        CustomReentrantLock reentrantLock; // Recursive lock implementation
        std::condition_variable condition; // Standard condition variable
        std::string name;

        static unsigned int id;
        static std::string DEFAULT_NAME_PREFIX;
    };

    unsigned int MutexProperties::id = 0;
    std::string MutexProperties::DEFAULT_NAME_PREFIX = "Mutex-";

}}}

////////////////////////////////////////////////////////////////////////////////
Mutex::Mutex() : Synchronizable(), properties(NULL) {
    this->properties = new MutexProperties();
}

////////////////////////////////////////////////////////////////////////////////
Mutex::Mutex( const std::string& name ) : Synchronizable(), properties(NULL) {
    this->properties = new MutexProperties( name );
}

////////////////////////////////////////////////////////////////////////////////
Mutex::~Mutex() {
    try {
        // CRITICAL: Destroying a locked std::mutex is undefined behavior and causes crashes.
        // Ensure proper cleanup before destruction.

        // If the current thread owns the lock, unlock it completely
        while (this->properties->reentrantLock.isHeldByCurrentThread()) {
            this->properties->reentrantLock.unlock();
        }

        // If another thread holds the lock, we have a race condition during shutdown.
        // On Windows, thread stack unwinding can continue briefly after join() returns.
        // Wait a short time to allow Lock RAII destructors to complete their unlock() calls.
        if (this->properties->reentrantLock.isLocked()) {
            for (int i = 0; i < 50 && this->properties->reentrantLock.isLocked(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    } catch (...) {
        // Suppress all exceptions in destructor to prevent std::terminate()
    }

    delete this->properties;
}

////////////////////////////////////////////////////////////////////////////////
std::string Mutex::getName() const {
    return this->properties->name;
}

////////////////////////////////////////////////////////////////////////////////
std::string Mutex::toString() const {
    return this->properties->name;
}

////////////////////////////////////////////////////////////////////////////////
bool Mutex::isLocked() const {
    return this->properties->reentrantLock.isHeldByCurrentThread();
}

////////////////////////////////////////////////////////////////////////////////
void Mutex::lock() {
    this->properties->reentrantLock.lock();
}

////////////////////////////////////////////////////////////////////////////////
void Mutex::unlock() {
    this->properties->reentrantLock.unlock();
}

////////////////////////////////////////////////////////////////////////////////
bool Mutex::tryLock() {
    return this->properties->reentrantLock.tryLock();
}
////////////////////////////////////////////////////////////////////////////////
void Mutex::wait() {
    wait(0, 0);
}

////////////////////////////////////////////////////////////////////////////////
void Mutex::wait(long long millisecs) {
    wait(millisecs, 0);
}

////////////////////////////////////////////////////////////////////////////////
void Mutex::wait( long long millisecs, int nanos ) {

    if (millisecs < 0) {
        throw IllegalArgumentException(__FILE__, __LINE__, "Milliseconds value cannot be negative.");
    }

    if (nanos < 0 || nanos > 999999) {
        throw IllegalArgumentException(__FILE__, __LINE__, "Nanoseconds value must be in the range [0..999999].");
    }

    // Check if the current thread has been interrupted before waiting
    if (Thread::interrupted()) {
        throw InterruptedException(__FILE__, __LINE__, "Thread interrupted before wait");
    }

    // Get current thread handle to manage state
    Thread* currentThread = Thread::currentThread();
    Thread::State savedState = currentThread->getState();

    // Save the recursion count
    int savedRecursionCount = this->properties->reentrantLock.getRecursionCount();

    // Clear CustomReentrantLock metadata WITHOUT unlocking the internal mutex
    // This allows other threads to acquire the lock via CustomReentrantLock
    // while we wait on the condition variable
    this->properties->reentrantLock.clearMetadata();

    // Create unique_lock adopting the already-locked internal mutex
    std::unique_lock<std::mutex> lock(this->properties->reentrantLock.getInternalMutex(), std::adopt_lock);

    // Set thread state before waiting
    decaf::internal::util::concurrent::ThreadHandle* handle = decaf::internal::util::concurrent::Threading::getCurrentThreadHandle();
    if (millisecs == 0 && nanos == 0) {
        handle->state.store(Thread::WAITING, std::memory_order_release);
    } else {
        handle->state.store(Thread::TIMED_WAITING, std::memory_order_release);
    }

    // Wait on condition variable (this unlocks and relocks the mutex)
    // We need to wait in a loop to check for interruption periodically
    if (millisecs == 0 && nanos == 0) {
        // For indefinite wait, use short polling to check for interruption
        while (true) {
            auto timeout = std::chrono::milliseconds(100);
            auto result = this->properties->condition.wait_for(lock, timeout);

            // Check if thread was interrupted
            if (Thread::interrupted()) {
                // Restore thread state
                handle->state.store(savedState, std::memory_order_release);
                // Release the unique_lock without unlocking the mutex
                lock.release();
                // Restore the CustomReentrantLock state (mutex is already locked)
                this->properties->reentrantLock.adoptLock(savedRecursionCount);
                throw InterruptedException(__FILE__, __LINE__, "Thread interrupted during wait");
            }

            // If we were notified (not timed out), exit the wait loop
            if (result == std::cv_status::no_timeout) {
                break;
            }
            // Otherwise, continue waiting (timeout occurred, loop again to check interruption)
        }
    } else {
        // For timed wait, just wait for the specified duration
        auto duration = std::chrono::milliseconds(millisecs) + std::chrono::nanoseconds(nanos);
        this->properties->condition.wait_for(lock, duration);

        // Check if thread was interrupted after the wait
        if (Thread::interrupted()) {
            // Restore thread state
            handle->state.store(savedState, std::memory_order_release);
            // Release the unique_lock without unlocking the mutex
            lock.release();
            // Restore the CustomReentrantLock state (mutex is already locked)
            this->properties->reentrantLock.adoptLock(savedRecursionCount);
            throw InterruptedException(__FILE__, __LINE__, "Thread interrupted during wait");
        }
    }

    // Restore thread state after waiting
    handle->state.store(savedState, std::memory_order_release);

    // Release the unique_lock without unlocking the mutex
    lock.release();

    // Restore the CustomReentrantLock state (mutex is already locked)
    this->properties->reentrantLock.adoptLock(savedRecursionCount);
}

////////////////////////////////////////////////////////////////////////////////
void Mutex::notify() {
    this->properties->condition.notify_one();
}

////////////////////////////////////////////////////////////////////////////////
void Mutex::notifyAll() {
    this->properties->condition.notify_all();
}
