/*
 * Copyright 2013 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef BSON_ATOMIC_H
#define BSON_ATOMIC_H


#include "bson-compat.h"
#include "bson-macros.h"


BSON_BEGIN_DECLS


#if defined(__GNUC__)
# define bson_atomic_int_add(p, v)   (__sync_add_and_fetch(p, v))
# define bson_atomic_int64_add(p, v) (__sync_add_and_fetch_8(p, v))
# define bson_memory_barrier         __sync_synchronize
#elif defined(_MSC_VER) || defined(_WIN32)
# define bson_atomic_int_add(p, v)   (InterlockedExchangeAdd((long int *)(p), v))
# define bson_atomic_int64_add(p, v) (InterlockedExchangeAdd64(p, v))
# define bson_memory_barrier         MemoryBarrier
#endif


BSON_END_DECLS


#endif /* BSON_ATOMIC_H */
