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

#include "DedicatedTaskRunner.h"

#include <activemq/exceptions/ActiveMQException.h>

using namespace activemq;
using namespace activemq::threads;
using namespace activemq::exceptions;
using namespace decaf;
using namespace decaf::lang;
using namespace decaf::lang::exceptions;

////////////////////////////////////////////////////////////////////////////////
DedicatedTaskRunner::DedicatedTaskRunner(Task* task) :
    mutex(), thread(), state(DedicatedTaskRunnerState::RUNNING), pending(false), task(task) {

    if (this->task == NULL) {
        throw NullPointerException(__FILE__, __LINE__, "Task passed was null");
    }

    this->thread.reset(new Thread(this, "ActiveMQ Dedicated Task Runner"));
}

////////////////////////////////////////////////////////////////////////////////
DedicatedTaskRunner::~DedicatedTaskRunner() {
    try {
        this->shutdown();
    }
    AMQ_CATCHALL_NOTHROW()
}

////////////////////////////////////////////////////////////////////////////////
void DedicatedTaskRunner::start() {

    synchronized(&mutex) {
        if (state.load(std::memory_order_acquire) == DedicatedTaskRunnerState::RUNNING &&
            !this->thread->isAlive()) {
            this->thread->start();
            this->wakeup();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
bool DedicatedTaskRunner::isStarted() const {

    bool result = false;

    synchronized(&mutex) {
        if (this->thread != NULL) {
            result = true;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
void DedicatedTaskRunner::shutdown(long long timeout) {

    synchronized(&mutex) {

        if (this->thread == NULL) {
            return;
        }

        // Phase 1: Signal shutdown
        DedicatedTaskRunnerState expected = DedicatedTaskRunnerState::RUNNING;
        if (!state.compare_exchange_strong(expected, DedicatedTaskRunnerState::STOPPING)) {
            // Already stopping or stopped
            return;
        }

        // Memory barrier
        std::atomic_thread_fence(std::memory_order_release);

        pending.store(true, std::memory_order_release);
        mutex.notifyAll();
    }

    // Phase 2: Wait till the thread stops
    if (Thread::currentThread() != this->thread.get()) {
        DedicatedTaskRunnerState currentState = state.load(std::memory_order_acquire);
        if (currentState != DedicatedTaskRunnerState::STOPPED) {
            this->thread->join(timeout);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void DedicatedTaskRunner::shutdown() {

    synchronized(&mutex) {

        if (this->thread == NULL) {
            return;
        }

        // Phase 1: Signal shutdown
        DedicatedTaskRunnerState expected = DedicatedTaskRunnerState::RUNNING;
        if (!state.compare_exchange_strong(expected, DedicatedTaskRunnerState::STOPPING)) {
            // Already stopping or stopped
            return;
        }

        // Memory barrier
        std::atomic_thread_fence(std::memory_order_release);

        pending.store(true, std::memory_order_release);
        mutex.notifyAll();
    }

    // Phase 2: Wait till the thread stops
    if (Thread::currentThread() != this->thread.get()) {
        DedicatedTaskRunnerState currentState = state.load(std::memory_order_acquire);
        if (currentState != DedicatedTaskRunnerState::STOPPED) {
            this->thread->join();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void DedicatedTaskRunner::wakeup() {

    if (state.load(std::memory_order_acquire) != DedicatedTaskRunnerState::RUNNING) {
        return;
    }

    synchronized(&mutex) {
        pending.store(true, std::memory_order_release);
        mutex.notifyAll();
    }
}

////////////////////////////////////////////////////////////////////////////////
void DedicatedTaskRunner::run() {

    try {

        while (true) {

            // Check state with memory barrier
            if (state.load(std::memory_order_acquire) != DedicatedTaskRunnerState::RUNNING) {
                break;
            }

            synchronized(&mutex) {
                pending.store(false, std::memory_order_release);
            }

            if (!this->task->iterate()) {

                // wait to be notified.
                synchronized(&mutex) {
                    // Double-check state before waiting
                    if (state.load(std::memory_order_acquire) != DedicatedTaskRunnerState::RUNNING) {
                        break;
                    }

                    // Use timed wait to periodically check state
                    while (!pending.load(std::memory_order_acquire) &&
                           state.load(std::memory_order_acquire) == DedicatedTaskRunnerState::RUNNING) {
                        mutex.wait(100); // 100ms timeout
                    }
                }
            }

        }
    }
    AMQ_CATCHALL_NOTHROW()

    // Mark as stopped with memory barrier
    state.store(DedicatedTaskRunnerState::STOPPED, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // Notify any waiting threads
    synchronized(&mutex) {
        mutex.notifyAll();
    }
}
