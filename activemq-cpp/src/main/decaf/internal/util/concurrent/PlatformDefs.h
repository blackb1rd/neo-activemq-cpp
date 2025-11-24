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

#ifndef _DECAF_INTERNAL_UTIL_CONCURRENT_PLATFORMDEFS_H_
#define _DECAF_INTERNAL_UTIL_CONCURRENT_PLATFORMDEFS_H_

#include <decaf/util/Config.h>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>

namespace decaf{
namespace internal{
namespace util{
namespace concurrent{

    // Forward declare the thread wrapper structure
    struct ThreadWrapper;

    // Simple wrapper for shared_mutex - tracking done via TLS
    struct RWMutexWrapper {
        std::shared_mutex mutex;
    };

    // Condition variable - using std::condition_variable (like Unix pthread_cond_t)
    struct ConditionWrapper {
        std::condition_variable cv;

        ConditionWrapper() {}
    };

    // No-op macro
    #define DECAF_CPU_RELAX() ((void)0)

    typedef void* PLATFORM_THREAD_ENTRY_ARG;

    #if defined(_WIN32) || defined(_WIN64)
        #define PLATFORM_THREAD_RETURN() return 0;
        #define PLATFORM_THREAD_CALLBACK_TYPE unsigned
        #define PLATFORM_MIN_STACK_SIZE 0x20000
        #define PLATFORM_CALLING_CONV __stdcall
    #else
        #define PLATFORM_THREAD_RETURN() return 0;
        #define PLATFORM_THREAD_CALLBACK_TYPE void*
        #define PLATFORM_MIN_STACK_SIZE 0x8000
        #define PLATFORM_CALLING_CONV
    #endif

    typedef ThreadWrapper* decaf_thread_t;
    typedef unsigned long decaf_tls_key;
    typedef ConditionWrapper* decaf_condition_t;
    typedef std::mutex* decaf_mutex_t;
    typedef RWMutexWrapper* decaf_rwmutex_t;

}}}}

#endif /* _DECAF_INTERNAL_UTIL_CONCURRENT_PLATFORMDEFS_H_ */
