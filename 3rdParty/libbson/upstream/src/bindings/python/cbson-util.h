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


#ifndef CBSON_UTIL_H
#define CBSON_UTIL_H


#include <bson.h>

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#include <Python.h>


BSON_BEGIN_DECLS


void          cbson_set_use_fixed_offset (bool   use_fixed_offset);
PyObject     *cbson_date_time_from_msec  (int64_t  msec_since_epoch);
int32_t  cbson_date_time_seconds    (PyObject     *date_time);
bool   cbson_util_init            (PyObject     *module);
bool   cbson_date_time_check      (PyObject     *object);
PyObject     *cbson_fixed_offset_utc_ref (void);


BSON_END_DECLS


#endif /* CBSON_UTIL_H */
