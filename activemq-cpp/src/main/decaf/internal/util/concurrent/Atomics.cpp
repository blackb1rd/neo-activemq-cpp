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

#include <decaf/internal/util/concurrent/Atomics.h>
#include <atomic>

using namespace decaf::internal;
using namespace decaf::internal::util;
using namespace decaf::internal::util::concurrent;

////////////////////////////////////////////////////////////////////////////////
void Atomics::initialize() {
}

////////////////////////////////////////////////////////////////////////////////
void Atomics::shutdown() {
}

////////////////////////////////////////////////////////////////////////////////
bool Atomics::compareAndSet32(volatile int* target, int expect, int update) {
    // Use std::atomic for cross-platform support (works on GCC, Clang, MSVC)
    std::atomic<int>* atomic_target = reinterpret_cast<std::atomic<int>*>(const_cast<int*>(target));
    return atomic_target->compare_exchange_strong(expect, update, std::memory_order_seq_cst, std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
bool Atomics::compareAndSet(volatile void** target, void* expect, void* update) {
    std::atomic<void*>* atomic_target = reinterpret_cast<std::atomic<void*>*>(const_cast<void**>(target));
    return atomic_target->compare_exchange_strong(expect, update, std::memory_order_seq_cst, std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
int Atomics::getAndSet(volatile int* target, int newValue) {
    std::atomic<int>* atomic_target = reinterpret_cast<std::atomic<int>*>(const_cast<int*>(target));
    return atomic_target->exchange(newValue, std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
void* Atomics::getAndSet(volatile void** target, void* newValue) {
    std::atomic<void*>* atomic_target = reinterpret_cast<std::atomic<void*>*>(const_cast<void**>(target));
    return atomic_target->exchange(newValue, std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
int Atomics::getAndIncrement(volatile int* target) {
    std::atomic<int>* atomic_target = reinterpret_cast<std::atomic<int>*>(const_cast<int*>(target));
    return atomic_target->fetch_add(1, std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
int Atomics::getAndDecrement(volatile int* target) {
    std::atomic<int>* atomic_target = reinterpret_cast<std::atomic<int>*>(const_cast<int*>(target));
    return atomic_target->fetch_sub(1, std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
int Atomics::getAndAdd(volatile int* target, int delta) {
    std::atomic<int>* atomic_target = reinterpret_cast<std::atomic<int>*>(const_cast<int*>(target));
    return atomic_target->fetch_add(delta, std::memory_order_seq_cst);
}

////////////////////////////////////////////////////////////////////////////////
int Atomics::addAndGet(volatile int* target, int delta) {
    std::atomic<int>* atomic_target = reinterpret_cast<std::atomic<int>*>(const_cast<int*>(target));
    return atomic_target->fetch_add(delta, std::memory_order_seq_cst) + delta;
}

////////////////////////////////////////////////////////////////////////////////
int Atomics::incrementAndGet(volatile int* target) {
    std::atomic<int>* atomic_target = reinterpret_cast<std::atomic<int>*>(const_cast<int*>(target));
    return atomic_target->fetch_add(1, std::memory_order_seq_cst) + 1;
}

////////////////////////////////////////////////////////////////////////////////
int Atomics::decrementAndGet(volatile int* target) {
    std::atomic<int>* atomic_target = reinterpret_cast<std::atomic<int>*>(const_cast<int*>(target));
    return atomic_target->fetch_sub(1, std::memory_order_seq_cst) - 1;
}
