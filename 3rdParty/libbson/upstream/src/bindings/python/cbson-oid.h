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


#ifndef CBSON_OID_H
#define CBSON_OID_H


#include <bson.h>

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#include <Python.h>


BSON_BEGIN_DECLS


#define cbson_oid_check(o) (Py_TYPE(o) == cbson_oid_get_type(NULL))


typedef struct
{
   PyObject_HEAD
   bson_oid_t oid;
} cbson_oid_t;


PyTypeObject *cbson_oid_get_type        (bson_context_t   *context);
PyObject     *cbson_oid_new             (const bson_oid_t *oid);
PyObject     *cbson_invalid_id_get_type (void);


BSON_END_DECLS


#endif /* CBSON_OID_H */
