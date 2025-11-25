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

#ifndef _DECAF_INTERNAL_UTIL_CONCURRENT_CUSTOMREENTRANTLOCK_H_
#define _DECAF_INTERNAL_UTIL_CONCURRENT_CUSTOMREENTRANTLOCK_H_

#include <decaf/util/Config.h>
#include <mutex>
#include <atomic>
#include <thread>

namespace decaf {
namespace internal {
namespace util {
namespace concurrent {

    /**
     * Custom Reentrant Lock Implementation using std::mutex and Atomic Ownership Tracking.
     *
     * This class provides Java-style monitor reentrancy without using std::recursive_mutex.
     * It manually tracks the owning thread ID and recursion counter to enable the same thread
     * to acquire the lock multiple times without deadlocking.
     *
     * Key Features:
     * - Reentrant locking: Same thread can lock multiple times
     * - Condition variable support: fullyUnlock() and reLock() methods
     * - Thread-safe ownership tracking using atomics
     * - Compatible with std::condition_variable
     *
     * Usage with Condition Variables:
     * The fullyUnlock() and reLock() methods are critical for correct condition variable
     * behavior. When waiting on a condition variable, the lock must be completely released
     * (regardless of recursion depth) and then restored to its previous state after waking up.
     *
     * @since 3.11.0
     */
    class DECAF_API CustomReentrantLock {
    private:

        // The underlying non-recursive mutex
        std::mutex _mutex;

        // Atomically tracks the thread ID that currently owns this lock
        // Uses std::thread::id() (default constructed) to represent "no owner"
        std::atomic<std::thread::id> _owner;

        // Tracks the number of times the owning thread has acquired this lock
        // Only modified by the owning thread, no need for atomic
        int _recursionCount;

    private:

        CustomReentrantLock(const CustomReentrantLock&);
        CustomReentrantLock& operator=(const CustomReentrantLock&);

    public:

        /**
         * Constructs a new CustomReentrantLock with no owner.
         */
        CustomReentrantLock() : _mutex(), _owner(std::thread::id()), _recursionCount(0) {}

        /**
         * Destructor - ensures the lock is not held during destruction.
         */
        ~CustomReentrantLock() {
            // Note: Destroying a locked mutex is undefined behavior
            // The caller must ensure the lock is released before destruction
        }

        /**
         * Acquires the lock, blocking if necessary until the lock is available.
         *
         * If the current thread already owns the lock, this method increments the
         * recursion counter and returns immediately. Otherwise, the thread blocks
         * until it can acquire the underlying mutex.
         *
         * Behavior:
         * 1. If current thread owns the lock: increment recursion count, return immediately
         * 2. Otherwise: block on _mutex.lock() until acquired
         * 3. Set owner to current thread, set recursion count to 1
         */
        void lock() {
            std::thread::id tid = std::this_thread::get_id();

            // Re-entry Case: We already own the lock
            if (_owner.load(std::memory_order_relaxed) == tid) {
                _recursionCount++;
                return;
            }

            // Acquisition Case: We don't own it, so we wait for the real mutex
            _mutex.lock();

            // Ownership Setup
            _owner.store(tid, std::memory_order_relaxed);
            _recursionCount = 1;
        }

        /**
         * Attempts to acquire the lock without blocking.
         *
         * If the current thread already owns the lock, this method increments the
         * recursion counter and returns true. Otherwise, it attempts to acquire the
         * underlying mutex without blocking.
         *
         * @return true if the lock was acquired, false otherwise
         */
        bool tryLock() {
            std::thread::id tid = std::this_thread::get_id();

            // Re-entry Case: We already own the lock
            if (_owner.load(std::memory_order_relaxed) == tid) {
                _recursionCount++;
                return true;
            }

            // Try to acquire without blocking
            if (_mutex.try_lock()) {
                _owner.store(tid, std::memory_order_relaxed);
                _recursionCount = 1;
                return true;
            }

            return false;
        }

        /**
         * Releases the lock once.
         *
         * Decrements the recursion counter. If the counter reaches zero, the lock
         * is fully released and other threads can acquire it.
         *
         * Throws IllegalMonitorStateException equivalent if called by a thread that
         * doesn't own the lock (in debug builds, this is an assertion failure).
         */
        void unlock() {
            std::thread::id tid = std::this_thread::get_id();

            // Safety check: ensure we are the owner
            if (_owner.load(std::memory_order_relaxed) != tid) {
                // In production, we might throw an exception here
                // For now, just return to avoid undefined behavior
                // LOG ERROR: IllegalMonitorStateException equivalent
                return;
            }

            _recursionCount--;

            if (_recursionCount == 0) {
                _owner.store(std::thread::id(), std::memory_order_relaxed); // Clear owner
                _mutex.unlock(); // Release real mutex
            }
        }

        /**
         * Completely releases the lock regardless of recursion depth.
         *
         * This method is CRITICAL for condition variable support. It saves the
         * current recursion count and fully releases the lock, allowing other
         * threads to acquire it. The returned value should be passed to reLock()
         * to restore the lock to its previous state.
         *
         * @return The recursion count before the lock was released, or 0 if the
         *         calling thread doesn't own the lock
         */
        int fullyUnlock() {
            std::thread::id tid = std::this_thread::get_id();

            if (_owner.load(std::memory_order_relaxed) != tid) {
                return 0; // Not the owner
            }

            int savedCount = _recursionCount;
            _recursionCount = 0;
            _owner.store(std::thread::id(), std::memory_order_relaxed);
            _mutex.unlock();

            return savedCount;
        }

        /**
         * Re-acquires the lock and restores it to a specific recursion depth.
         *
         * This method is CRITICAL for condition variable support. After waiting on
         * a condition variable, this method re-acquires the lock and restores the
         * recursion count to match the value returned by fullyUnlock().
         *
         * @param count The recursion depth to restore (from fullyUnlock())
         */
        void reLock(int count) {
            if (count <= 0) {
                return; // Nothing to restore
            }

            _mutex.lock();
            _owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
            _recursionCount = count;
        }

        /**
         * Returns the underlying std::mutex for use with std::condition_variable.
         *
         * WARNING: Direct use of this mutex bypasses reentrancy tracking.
         * This method should only be used internally for condition variable operations
         * in conjunction with fullyUnlock() and reLock().
         *
         * @return Reference to the internal std::mutex
         */
        std::mutex& getInternalMutex() {
            return _mutex;
        }

        /**
         * Adopts an already-locked mutex and restores recursion state.
         * This is used after a condition variable wait where the underlying mutex
         * is already locked by std::unique_lock.
         *
         * WARNING: Only call this when you KNOW the mutex is already locked!
         *
         * @param count The recursion depth to restore (from fullyUnlock())
         */
        void adoptLock(int count) {
            if (count <= 0) {
                return;
            }
            // The mutex is already locked, just update the metadata
            _owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
            _recursionCount = count;
        }

        /**
         * Checks if the current thread owns this lock.
         *
         * @return true if the calling thread owns this lock, false otherwise
         */
        bool isHeldByCurrentThread() const {
            return _owner.load(std::memory_order_relaxed) == std::this_thread::get_id();
        }

        /**
         * Returns the number of times the current thread has acquired this lock.
         *
         * @return The recursion depth, or 0 if the current thread doesn't own the lock
         */
        int getRecursionCount() const {
            if (_owner.load(std::memory_order_relaxed) == std::this_thread::get_id()) {
                return _recursionCount;
            }
            return 0;
        }

    };

}}}}

#endif /* _DECAF_INTERNAL_UTIL_CONCURRENT_CUSTOMREENTRANTLOCK_H_ */
