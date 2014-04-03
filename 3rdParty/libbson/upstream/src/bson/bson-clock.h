/*
 * Copyright 2014 MongoDB, Inc.
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


#if !defined (BSON_INSIDE) && !defined (BSON_COMPILATION)
# error "Only <bson.h> can be included directly."
#endif


#ifndef BSON_CLOCK_H
#define BSON_CLOCK_H


#include "bson-compat.h"
#include "bson-macros.h"
#include "bson-types.h"


BSON_BEGIN_DECLS


int64_t bson_get_monotonic_time (void);
int     bson_gettimeofday       (struct timeval  *tv,
                                 struct timezone *tz);


BSON_END_DECLS


#endif /* BSON_CLOCK_H */
