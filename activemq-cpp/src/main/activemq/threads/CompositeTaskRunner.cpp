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

#include "CompositeTaskRunner.h"

#include <memory>
#include <atomic>

#include <activemq/exceptions/ActiveMQException.h>

using namespace std;
using namespace activemq;
using namespace activemq::threads;
using namespace activemq::exceptions;
using namespace decaf;
using namespace decaf::util;
using namespace decaf::lang;
using namespace decaf::lang::exceptions;

////////////////////////////////////////////////////////////////////////////////
namespace activemq {
namespace threads {

    enum class TaskRunnerState : int {
        RUNNING = 0,
        STOPPING = 1,
        STOPPED = 2
    };

    class CompositeTaskRunnerImpl {
    private:

        CompositeTaskRunnerImpl(const CompositeTaskRunnerImpl&);
        CompositeTaskRunnerImpl& operator= (const CompositeTaskRunnerImpl&);

    public:

        decaf::util::LinkedList<CompositeTask*> tasks;
        mutable decaf::util::concurrent::Mutex mutex;

        decaf::lang::Pointer<decaf::lang::Thread> thread;

        std::atomic<TaskRunnerState> state;
        std::atomic<bool> pending;

    public:

        CompositeTaskRunnerImpl() : tasks(),
                                    mutex(),
                                    thread(),
                                    state(TaskRunnerState::RUNNING),
                                    pending(false) {
        }

    };

}}

////////////////////////////////////////////////////////////////////////////////
CompositeTaskRunner::CompositeTaskRunner() : impl(new CompositeTaskRunnerImpl) {
    this->impl->thread.reset(new Thread(this, "ActiveMQ CompositeTaskRunner Thread"));
}

////////////////////////////////////////////////////////////////////////////////
CompositeTaskRunner::~CompositeTaskRunner() {
    try {
        shutdown();
        impl->thread->join();
        impl->thread.reset(NULL);
    }
    AMQ_CATCHALL_NOTHROW()

    try {
        delete this->impl;
    }
    AMQ_CATCHALL_NOTHROW()
}

////////////////////////////////////////////////////////////////////////////////
void CompositeTaskRunner::start() {

    synchronized(&impl->mutex) {
        if (impl->state.load(std::memory_order_acquire) == TaskRunnerState::RUNNING &&
            !this->impl->thread->isAlive()) {
            this->impl->thread->start();
            this->wakeup();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
bool CompositeTaskRunner::isStarted() const {

    bool result = false;

    synchronized(&impl->mutex) {
        if (this->impl->thread != NULL && this->impl->thread->isAlive()) {
            result = true;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
void CompositeTaskRunner::shutdown(long long timeout) {

    // Phase 1: Signal shutdown
    TaskRunnerState expected = TaskRunnerState::RUNNING;
    if (!impl->state.compare_exchange_strong(expected, TaskRunnerState::STOPPING)) {
        // Already stopping or stopped
        return;
    }

    // Memory barrier to ensure state change is visible
    std::atomic_thread_fence(std::memory_order_release);

    // Wake up the thread
    synchronized(&impl->mutex) {
        impl->pending.store(true, std::memory_order_release);
        impl->mutex.notifyAll();
    }

    // Phase 2: Wait for thread to terminate
    // (no need to wait if shutdown is called from thread that is shutting down)
    if (Thread::currentThread() != this->impl->thread.get()) {
        TaskRunnerState currentState = impl->state.load(std::memory_order_acquire);
        if (currentState != TaskRunnerState::STOPPED) {
            this->impl->thread->join(timeout);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void CompositeTaskRunner::shutdown() {

    // Phase 1: Signal shutdown
    TaskRunnerState expected = TaskRunnerState::RUNNING;
    if (!impl->state.compare_exchange_strong(expected, TaskRunnerState::STOPPING)) {
        // Already stopping or stopped
        return;
    }

    // Memory barrier to ensure state change is visible
    std::atomic_thread_fence(std::memory_order_release);

    // Wake up the thread
    synchronized(&impl->mutex) {
        impl->pending.store(true, std::memory_order_release);
        impl->mutex.notifyAll();
    }

    // Phase 2: Wait for thread to terminate
    // (no need to wait if shutdown is called from thread that is shutting down)
    if (Thread::currentThread() != this->impl->thread.get()) {
        TaskRunnerState currentState = impl->state.load(std::memory_order_acquire);
        if (currentState != TaskRunnerState::STOPPED) {
            impl->thread->join();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void CompositeTaskRunner::wakeup() {

    // Check if we're shutting down
    if (impl->state.load(std::memory_order_acquire) != TaskRunnerState::RUNNING) {
        return;
    }

    synchronized(&impl->mutex) {
        impl->pending.store(true, std::memory_order_release);
        impl->mutex.notifyAll();
    }
}

////////////////////////////////////////////////////////////////////////////////
void CompositeTaskRunner::run() {

    try {

        while (true) {
            // Check state with memory barrier
            if (impl->state.load(std::memory_order_acquire) != TaskRunnerState::RUNNING) {
                break;
            }

            synchronized(&impl->mutex) {
                impl->pending.store(false, std::memory_order_release);
            }

            if (!this->iterate()) {
                // wait to be notified.
                synchronized(&impl->mutex) {
                    // Double-check state before waiting
                    if (impl->state.load(std::memory_order_acquire) != TaskRunnerState::RUNNING) {
                        break;
                    }

                    // Use timed wait to periodically check state
                    while (!impl->pending.load(std::memory_order_acquire) &&
                           impl->state.load(std::memory_order_acquire) == TaskRunnerState::RUNNING) {
                        impl->mutex.wait(100); // 100ms timeout
                    }
                }
            }
        }
    }
    AMQ_CATCHALL_NOTHROW()

    // Mark as stopped with memory barrier
    impl->state.store(TaskRunnerState::STOPPED, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // Notify any waiting threads
    synchronized(&impl->mutex) {
        impl->mutex.notifyAll();
    }
}

////////////////////////////////////////////////////////////////////////////////
void CompositeTaskRunner::addTask(CompositeTask* task) {

    if (task != NULL) {
        synchronized(&impl->tasks) {
            impl->tasks.add(task);
            wakeup();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void CompositeTaskRunner::removeTask(CompositeTask* task) {

    if (task != NULL) {
        synchronized(&impl->tasks) {
            impl->tasks.remove(task);
            wakeup();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
bool CompositeTaskRunner::iterate() {

    synchronized(&impl->tasks) {

        for (int i = 0; i < impl->tasks.size(); ++i) {
            CompositeTask* task = impl->tasks.pop();

            if (task != NULL) {
                if (task->isPending()) {
                    task->iterate();
                    impl->tasks.addLast(task);

                    // Always return true, so that we check again for any of
                    // the other tasks that might now be pending.
                    return true;
                } else {
                    impl->tasks.addLast(task);
                }
            }
        }
    }

    return false;
}
