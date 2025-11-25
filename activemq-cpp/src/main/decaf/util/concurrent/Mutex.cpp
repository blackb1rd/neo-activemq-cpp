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
#include <decaf/lang/Integer.h>
#include <decaf/lang/Thread.h>

#include <mutex>
#include <condition_variable>
#include <chrono>

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
        // Ensure mutex is not locked during destruction
        // std::mutex destructor handles cleanup automatically
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

    // Save and fully release the lock (handles recursion properly)
    int savedRecursionCount = this->properties->reentrantLock.fullyUnlock();

    // Create a unique_lock for the internal mutex (it will lock it)
    std::unique_lock<std::mutex> lock(this->properties->reentrantLock.getInternalMutex());

    // Wait on the condition variable (releases lock and reacquires on wake)
    if (millisecs == 0 && nanos == 0) {
        this->properties->condition.wait(lock);
    } else {
        auto duration = std::chrono::milliseconds(millisecs) + std::chrono::nanoseconds(nanos);
        this->properties->condition.wait_for(lock, duration);
    }

    // Release the unique_lock without unlocking (we'll restore recursion state)
    lock.release();

    // Restore the lock to its previous recursion state (mutex is already locked)
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
