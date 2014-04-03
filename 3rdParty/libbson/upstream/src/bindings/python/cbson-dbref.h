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


#ifndef CBSON_DBREF_H
#define CBSON_DBREF_H


#include <bson.h>

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#include <Python.h>


BSON_BEGIN_DECLS


typedef struct
{
   PyObject_HEAD

   char   *collection;
   size_t  collection_len;

   char   *database;
   size_t  database_len;

   bson_oid_t oid;

   PyObject *extra;

} cbson_dbref_t;


PyTypeObject *cbson_dbref_get_type (void);
PyObject     *cbson_dbref_new      (const char       *collection,
                                    size_t            collection_len,
                                    const char       *database,
                                    size_t            database_len,
                                    const bson_oid_t *oid);


BSON_END_DECLS


#endif /* CBSON_DBREF_H */
