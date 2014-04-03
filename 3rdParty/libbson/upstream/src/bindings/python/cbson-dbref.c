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


#include <string.h>

#include "cbson-dbref.h"
#include "cbson-oid.h"


#define cbson_dbref_check(op) (Py_TYPE(op) == &cbson_dbref_type)


static PyObject *cbson_dbref_get_collection (PyObject *obj,
                                             void     *data);
static PyObject *cbson_dbref_get_database   (PyObject *obj,
                                             void     *data);
static PyObject *cbson_dbref_get_id         (PyObject *obj,
                                             void     *data);
static PyObject *cbson_dbref_tp_repr        (PyObject *obj);


static PyTypeObject cbson_dbref_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "cbson.DBRef",             /*tp_name*/
    sizeof(cbson_dbref_t),     /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,    /*tp_compare*/
    cbson_dbref_tp_repr,       /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,       /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "A BSON DBRef.",           /*tp_doc*/
};


static PyGetSetDef cbson_dbref_getset[] = {
   { (char *)"collection", cbson_dbref_get_collection, NULL,
     (char *)"Get the name of this DBRef's collection as unicode." },
   { (char *)"database", cbson_dbref_get_database, NULL,
     (char *)"Get the name of this DBRef's database.\n\n"
             "Returns None if this DBRef doesn't specify a database." },
   { (char *)"id", cbson_dbref_get_id, NULL,
     (char *)"Get this DBRef's _id." },
   { NULL }
};


static PyMethodDef cbson_dbref_methods[] = {
   { NULL }
};


static PyObject *
cbson_dbref_get_collection (PyObject *object,
                            void     *data)
{
   cbson_dbref_t *dbref = (cbson_dbref_t *)object;
   PyObject *ret;

   if (dbref->collection) {
      ret = PyUnicode_FromStringAndSize(dbref->collection,
                                        dbref->collection_len);
   } else {
      ret = Py_None;
      Py_INCREF(ret);
   }

   return ret;
}


static PyObject *
cbson_dbref_get_database (PyObject *object,
                          void     *data)
{
   cbson_dbref_t *dbref = (cbson_dbref_t *)object;
   PyObject *ret;

   if (dbref->database) {
      ret = PyUnicode_FromStringAndSize(dbref->database,
                                        dbref->database_len);
   } else {
      ret = Py_None;
      Py_INCREF(ret);
   }

   return ret;
}


static PyObject *
cbson_dbref_get_id (PyObject *object,
                    void     *data)
{
   cbson_dbref_t *dbref = (cbson_dbref_t *)object;
   return cbson_oid_new(&dbref->oid);
}


static PyObject *
cbson_dbref_tp_repr (PyObject *obj)
{
   PyObject *collection = NULL;
   PyObject *id = NULL;
   PyObject *format = NULL;
   PyObject *args = NULL;
   PyObject *repr = NULL;

   if ((collection = cbson_dbref_get_collection(obj, NULL)) &&
       (id = cbson_dbref_get_id(obj, NULL)) &&
       (format = PyString_FromString("DBRef(%r, %r)")) &&
       (args = PyTuple_Pack(2, collection, id))) {
      repr = PyUnicode_Format(format, args);
   }

   Py_XDECREF(collection);
   Py_XDECREF(id);
   Py_XDECREF(format);
   Py_XDECREF(args);

   return repr;
}


static PyObject *
cbson_dbref_tp_new (PyTypeObject *self,
                    PyObject     *args,
                    PyObject     *kwargs)
{
   const char *col;
   PyObject *oid;
   PyObject *ret;
   int col_len;

   if (!PyArg_ParseTuple(args, "s#O", &col, &col_len, &oid)) {
      return NULL;
   }

   assert(col);
   assert(col_len >= 0);
   assert(oid);

   if (!cbson_oid_check(oid)) {
      PyErr_SetString(PyExc_TypeError, "oid must be a cbson.ObjectId");
      return NULL;
   }

   /*
    * TODO: Extra, Database, etc.
    */

   ret = cbson_dbref_new(col, col_len, NULL, 0, &((cbson_oid_t *)oid)->oid);

   return ret;
}


PyObject *
cbson_dbref_new (const char       *collection,
                 size_t            collection_len,
                 const char       *database,
                 size_t            database_len,
                 const bson_oid_t *oid)
{
   cbson_dbref_t *dbref;
   PyObject *ret;

   bson_return_val_if_fail(collection || !collection_len, NULL);
   bson_return_val_if_fail(database || !database_len, NULL);
   bson_return_val_if_fail(oid, NULL);

   if (!(ret = PyType_GenericNew(&cbson_dbref_type, NULL, NULL))) {
      return NULL;
   }

   dbref = (cbson_dbref_t *)ret;

   if (collection && collection_len) {
      dbref->collection = bson_malloc0(collection_len + 1);
      memcpy(dbref->collection, collection, collection_len);
      dbref->collection[collection_len] = '\0';
      dbref->collection_len = collection_len;
   } else {
      dbref->collection = NULL;
      dbref->collection_len = 0;
   }

   if (database && database_len) {
      dbref->database = bson_malloc0(database_len + 1);
      memcpy(dbref->database, database, database_len);
      dbref->database[database_len] = '\0';
   } else {
      dbref->database = NULL;
      dbref->database_len = 0;
   }

   bson_oid_copy(oid, &dbref->oid);

   return ret;
}


PyTypeObject *
cbson_dbref_get_type (void)
{
   static bool initialized;

   if (!initialized) {
      cbson_dbref_type.tp_new = cbson_dbref_tp_new;
      cbson_dbref_type.tp_getset = cbson_dbref_getset;
      cbson_dbref_type.tp_methods = cbson_dbref_methods;
      if (PyType_Ready(&cbson_dbref_type) < 0) {
         return NULL;
      }
      initialized = true;
   }

   return &cbson_dbref_type;
}
