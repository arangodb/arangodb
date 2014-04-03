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


#include <Python.h>
#include <datetime.h>

#include <bson.h>

#include "cbson-dbref.h"
#include "cbson-oid.h"
#include "cbson-util.h"


static bson_context_t *gContext;
static PyObject       *gStr_id;


static BSON_INLINE void
cbson_loads_set_item (PyObject   *obj,
                      const char *key,
                      PyObject   *value)
{
   PyObject *keyobj;

   if (PyDict_Check(obj)) {
      if (*key == '_' && !strcmp(key, "_id")) {
         keyobj = gStr_id;
         Py_INCREF(keyobj);
      } else if (!(keyobj = PyUnicode_DecodeUTF8(key, strlen(key), "strict"))) {
         keyobj = PyString_FromString(key);
      }
      if (keyobj) {
         PyDict_SetItem(obj, keyobj, value);
         Py_DECREF(keyobj);
      }
   } else {
      PyList_Append(obj, value);
   }
}


static PyObject *
cbson_regex_new (const char *regex,
                 const char *options)
{
   uint32_t flags = 0;
   uint32_t i;
   PyObject *module;
   PyObject *compile;
   PyObject *strobj;
   PyObject *value = NULL;

   /*
    * NOTE: Typically, you would want to cache the module from
    *       PyImport_ImportModule(). However, it is really not typical to
    *       have regexes stored so loading it each time is not a common
    *       case.
    */

   if (!(module = PyImport_ImportModule("re"))) {
      return NULL;
   }

   if (!(compile = PyObject_GetAttrString(module, "compile"))) {
      Py_DECREF(module);
      return NULL;
   }

   for (i = 0; options[i]; i++) {
      switch (options[i]) {
      case 'i':
         flags |= 2;
         break;
      case 'l':
         flags |= 4;
         break;
      case 'm':
         flags |= 8;
         break;
      case 's':
         flags |= 16;
         break;
      case 'u':
         flags |= 32;
         break;
      case 'x':
         flags |= 64;
         break;
      default:
         break;
      }
   }

   if ((strobj = PyUnicode_FromString(regex))) {
      value = PyObject_CallFunction(compile, (char *)"Oi", strobj, flags);
      Py_DECREF(strobj);
   }

   Py_DECREF(compile);
   Py_DECREF(module);

   return value;
}


static bool
cbson_loads_visit_utf8 (const bson_iter_t *iter,
                        const char        *key,
                        size_t             v_utf8_len,
                        const char        *v_utf8,
                        void              *data)
{
   PyObject **ret = data;
   PyObject *value;

   if (!(value = PyUnicode_DecodeUTF8(v_utf8, v_utf8_len, "strict"))) {
      Py_DECREF(*ret);
      *ret = NULL;
      return true;
   }

   cbson_loads_set_item(*ret, key, value);
   Py_DECREF(value);
   return false;
}


static bool
cbson_loads_visit_int32 (const bson_iter_t *iter,
                         const char        *key,
                         int32_t       v_int32,
                         void              *data)
{
   PyObject **ret = data;
   PyObject *value;

#if PY_MAJOR_VERSION >= 3
   value = PyLong_FromLong(v_int32);
#else
   value = PyInt_FromLong(v_int32);
#endif

   if (value) {
      cbson_loads_set_item(*ret, key, value);
      Py_DECREF(value);
   }

   return false;
}


static bool
cbson_loads_visit_int64 (const bson_iter_t *iter,
                         const char        *key,
                         int64_t       v_int64,
                         void              *data)
{
   PyObject **ret = data;
   PyObject *value;

   if ((value = PyLong_FromLongLong(v_int64))) {
      cbson_loads_set_item(*ret, key, value);
      Py_DECREF(value);
   }

   return false;
}


static bool
cbson_loads_visit_bool (const bson_iter_t *iter,
                        const char        *key,
                        bool        v_bool,
                        void              *data)
{
   PyObject **ret = data;
   cbson_loads_set_item(*ret, key, v_bool ? Py_True : Py_False);
   return false;
}


static bool
cbson_loads_visit_double (const bson_iter_t *iter,
                          const char        *key,
                          double             v_double,
                          void              *data)
{
   PyObject **ret = data;
   PyObject *value;

   if ((value = PyFloat_FromDouble(v_double))) {
      cbson_loads_set_item(*ret, key, value);
      Py_DECREF(value);
   }

   return false;
}


static bool
cbson_loads_visit_oid (const bson_iter_t *iter,
                       const char        *key,
                       const bson_oid_t  *oid,
                       void              *data)
{
   PyObject **ret = data;
   PyObject *value;

   if ((value = cbson_oid_new(oid))) {
      cbson_loads_set_item(*ret, key, value);
      Py_DECREF(value);
   }

   return false;
}


static bool
cbson_loads_visit_undefined (const bson_iter_t *iter,
                             const char        *key,
                             void              *data)
{
   PyObject **ret = data;
   cbson_loads_set_item(*ret, key, Py_None);
   return false;
}


static bool
cbson_loads_visit_null (const bson_iter_t *iter,
                        const char        *key,
                        void              *data)
{
   PyObject **ret = data;
   cbson_loads_set_item(*ret, key, Py_None);
   return false;
}


static bool
cbson_loads_visit_date_time (const bson_iter_t *iter,
                             const char        *key,
                             int64_t       msec_since_epoch,
                             void              *data)
{
   PyObject **ret = data;
   PyObject *date_time;

   if ((date_time = cbson_date_time_from_msec(msec_since_epoch))) {
      cbson_loads_set_item(*ret, key, date_time);
      Py_DECREF(date_time);
   }

   return false;
}


static bool
cbson_loads_visit_regex (const bson_iter_t *iter,
                         const char        *key,
                         const char        *regex,
                         const char        *options,
                         void              *data)
{
   PyObject **ret = data;
   PyObject *re;

   if ((re = cbson_regex_new(regex, options))) {
      cbson_loads_set_item(*ret, key, re);
      Py_DECREF(re);
   }

   return false;
}


static bool
cbson_loads_visit_dbpointer (const bson_iter_t *iter,
                             const char        *key,
                             size_t             v_collection_len,
                             const char        *v_collection,
                             const bson_oid_t  *v_oid,
                             void              *data)
{
   PyObject **ret = data;
   PyObject *dbref;

   if ((dbref = cbson_dbref_new(v_collection,
                                v_collection_len,
                                NULL,
                                0,
                                v_oid))) {
      cbson_loads_set_item(*ret, key, dbref);
      Py_DECREF(dbref);
   }

   return false;
}


static bool
cbson_loads_visit_document (const bson_iter_t *iter,
                            const char        *key,
                            const bson_t      *v_document,
                            void              *data);


static bool
cbson_loads_visit_array (const bson_iter_t *iter,
                         const char        *key,
                         const bson_t      *v_array,
                         void              *data);


static void
cbson_loads_visit_corrupt (const bson_iter_t *iter,
                           void              *data)
{
   PyObject **ret = data;
   Py_DECREF(*ret);
   *ret = NULL;
}


static const bson_visitor_t gLoadsVisitors = {
   .visit_corrupt = cbson_loads_visit_corrupt,
   .visit_double = cbson_loads_visit_double,
   .visit_utf8 = cbson_loads_visit_utf8,
   .visit_document = cbson_loads_visit_document,
   .visit_array = cbson_loads_visit_array,
   .visit_undefined = cbson_loads_visit_undefined,
   .visit_oid = cbson_loads_visit_oid,
   .visit_bool = cbson_loads_visit_bool,
   .visit_date_time = cbson_loads_visit_date_time,
   .visit_null = cbson_loads_visit_null,
   .visit_regex = cbson_loads_visit_regex,
   .visit_dbpointer = cbson_loads_visit_dbpointer,
   .visit_int32 = cbson_loads_visit_int32,
   .visit_int64 = cbson_loads_visit_int64,
};


static bool
cbson_loads_visit_document (const bson_iter_t *iter,
                            const char        *key,
                            const bson_t      *v_document,
                            void              *data)
{
   bson_iter_t child;
   PyObject **ret = data;
   PyObject *obj;

   bson_return_val_if_fail(iter, true);
   bson_return_val_if_fail(key, true);
   bson_return_val_if_fail(v_document, true);

   if (bson_iter_init(&child, v_document)) {
      obj = PyDict_New();
      if (!bson_iter_visit_all(&child, &gLoadsVisitors, &obj)) {
         cbson_loads_set_item(*ret, key, obj);
      }
      Py_XDECREF(obj);
   }

   return false;
}


static bool
cbson_loads_visit_array (const bson_iter_t *iter,
                         const char        *key,
                         const bson_t      *v_array,
                         void              *data)
{
   bson_iter_t child;
   PyObject **ret = data;
   PyObject *obj;

   bson_return_val_if_fail(iter, true);
   bson_return_val_if_fail(key, true);
   bson_return_val_if_fail(v_array, true);

   if (bson_iter_init(&child, v_array)) {
      obj = PyList_New(0);
      if (!bson_iter_visit_all(&child, &gLoadsVisitors, &obj)) {
         cbson_loads_set_item(*ret, key, obj);
      }
      Py_XDECREF(obj);
   }

   return false;
}


static PyObject *
cbson_loads (PyObject *self,
             PyObject *args)
{
   const uint8_t *buffer;
   uint32_t buffer_length;
   bson_reader_t *reader;
   bson_iter_t iter;
   const bson_t *b;
   bool eof = false;
   PyObject *ret = NULL;
   PyObject *dict;

   if (!PyArg_ParseTuple(args, "s#", &buffer, &buffer_length)) {
      return NULL;
   }

   ret = PyList_New(0);

   reader = bson_reader_new_from_data(buffer, buffer_length);

   if (!(b = bson_reader_read(reader, &eof))) {
      PyErr_SetString(PyExc_ValueError, "Failed to parse buffer.");
      goto failure;
   }

   do {
      if (!bson_iter_init(&iter, b)) {
         bson_reader_destroy(reader);
         goto failure;
      }
      dict = PyDict_New();
      bson_iter_visit_all(&iter, &gLoadsVisitors, &dict);
      if (dict) {
         PyList_Append(ret, dict);
         Py_DECREF(dict);
      }
   } while ((b = bson_reader_read(reader, &eof)));

   bson_reader_destroy(reader);

   if (!eof) {
      PyErr_SetString(PyExc_ValueError, "Buffer contained invalid BSON.");
      goto failure;
   }

   return ret;

failure:
   Py_XDECREF(ret);
   return NULL;
}


static PyObject *
cbson_as_json (PyObject *self,
               PyObject *args)
{
   const uint8_t *buffer;
   uint32_t buffer_length;
   bson_reader_t *reader;
   const bson_t *b;
   PyObject *ret = NULL;
   size_t len = 0;
   char *str;

   if (!PyArg_ParseTuple(args, "s#", &buffer, &buffer_length)) {
      return NULL;
   }

   reader = bson_reader_new_from_data(buffer, buffer_length);
   b = bson_reader_read(reader, NULL);
   bson_reader_destroy(reader);

   if (b) {
      str = bson_as_json(b, &len);
      ret = PyUnicode_DecodeUTF8(str, len, "strict");
      bson_free(str);
   } else {
      PyErr_SetString(PyExc_ValueError, "Failed to parse BSON document.");
   }

   return ret;
}


static PyObject *
cbson_from_json (PyObject *self,
                 PyObject *args)
{
   bson_t b;
   const uint8_t *buffer;
   bson_error_t error;
   PyObject *ret = NULL;
#ifdef PY_SSIZE_T_CLEAN
   ssize_t buffer_length;
#else
   int buffer_length;
#endif

   if (!PyArg_ParseTuple (args, "s#", &buffer, &buffer_length)) {
      return NULL;
   }

   if (!bson_init_from_json (&b, (char *)buffer, buffer_length, &error)) {
      PyErr_SetString (PyExc_ValueError, "Failed to parse JSON document.");
      return NULL;
   }

   ret = PyString_FromStringAndSize ((char *)bson_get_data (&b), b.len);

   bson_destroy (&b);

   return ret;
}


static PyObject *
cbson_dumps (PyObject *self,
             PyObject *args)
{
   const char *keystr;
   PyObject *doc;
   PyObject *key;
   PyObject *ret;
   PyObject *value;
   Py_ssize_t pos = 0;
   bson_t *b;
   size_t keylen;

   if (!PyArg_ParseTuple(args, "O", &doc)) {
      return NULL;
   }

   if (!PyDict_Check(doc)) {
      PyErr_SetString(PyExc_TypeError, "doc must be a dict.");
      return NULL;
   }

   b = bson_new();

   while (PyDict_Next(doc, &pos, &key, &value)) {
      /*
       * TODO: Key validation. Make sure no NULL is present. Ensure valid UTF-8.
       */

      if (PyString_Check(key)) {
         keystr = PyString_AS_STRING(key);
         keylen = PyString_GET_SIZE(key);
      } else if (PyUnicode_Check(key)) {
         /*
          * TODO: Convert to UTF-8.
          */
         keystr = (const char *)PyUnicode_AS_UNICODE(key);
         keylen = PyUnicode_GET_SIZE(key);
      } else {
         PyErr_SetString(PyExc_TypeError, "key must be a string.");
         bson_destroy(b);
         return NULL;
      }

      if (value == Py_None) {
         if (!bson_append_null(b, keystr, keylen)) {
            goto failure;
         }
      } else if (PyString_Check(value)) {
         /*
          * TODO: Validate UTF-8.
          */
         if (!bson_append_utf8(b, keystr, keylen,
                               PyString_AS_STRING(value),
                               PyString_GET_SIZE(value))) {
            goto failure;
         }
      } else if (PyUnicode_Check(value)) {
         /*
          * TODO: Convert and validate UTF-8.
          */
         if (!bson_append_utf8(b, keystr, keylen,
                               (const char *)PyUnicode_AS_UNICODE(value),
                               PyUnicode_GET_SIZE(value))) {
            goto failure;
         }
      } else if (PyDateTime_Check(value)) {
         /*
          * TODO: Convert to msec since epoch.
          */
      } else if (PyBool_Check(value)) {
         if (!bson_append_bool(b, keystr, keylen, (value == Py_True))) {
            goto failure;
         }
      } else if (PyLong_Check(value)) {
         if (!bson_append_int64(b, keystr, keylen, PyLong_AsLong(value))) {
            goto failure;
         }
      } else if (PyInt_Check(value)) {
         if (!bson_append_int32(b, keystr, keylen, PyInt_AsLong(value))) {
            goto failure;
         }
      } else if (PyFloat_Check(value)) {
         if (!bson_append_double(b, keystr, keylen, PyFloat_AsDouble(value))) {
            goto failure;
         }
      } else if (cbson_oid_check(value)) {
         if (!bson_append_oid(b, keystr, keylen, &((cbson_oid_t *)value)->oid)) {
            goto failure;
         }
      /* } else if (CHECK FOR REGEX) { */
      /* } else if (CHECK FOR BINARY) { */
      } else {
         goto failure;
      }
   }

   ret = PyString_FromStringAndSize((const char *)bson_get_data(b), b->len);
   bson_destroy(b);

   return ret;

failure:
   PyErr_SetString(PyExc_TypeError, "Cannot encode type.");
   bson_destroy(b);
   return NULL;
}


static PyMethodDef cbson_methods[] = {
   { "dumps", cbson_dumps, METH_VARARGS, "Encode a document to BSON." },
   { "loads", cbson_loads, METH_VARARGS, "Decode a document from BSON." },
   { "as_json", cbson_as_json, METH_VARARGS, "Encode a BSON document as MongoDB Extended JSON." },
   { "from_json", cbson_from_json, METH_VARARGS, "Decode a BSON document from MongoDB Extended JSON." },
   { NULL }
};


PyMODINIT_FUNC
initcbson (void)
{
   bson_context_flags_t flags;
   PyObject *module;
   PyObject *version;

   /*
    * Initialize our module with class methods.
    */
   if (!(module = Py_InitModule("cbson", cbson_methods))) {
      return;
   }

   /*
    * Initialize our context for certain library functions.
    */
   flags = 0;
   flags |= BSON_CONTEXT_DISABLE_PID_CACHE;
#if defined(__linux__)
   flags |= BSON_CONTEXT_USE_TASK_ID;
#endif
   gContext = bson_context_new(flags);

   /*
    * Register the library version as __version__.
    */
   version = PyString_FromString(BSON_VERSION_S);
   PyModule_AddObject(module, "__version__", version);

   /*
    * Initialize utilities (datetime).
    */
   if (!cbson_util_init(module)) {
      Py_DECREF(module);
      return;
   }

   gStr_id = PyString_FromStringAndSize("_id", 3);

   /*
    * Register cbson types.
    */
   PyModule_AddObject(module, "ObjectId", (PyObject *)cbson_oid_get_type(gContext));
   PyModule_AddObject(module, "InvalidId", cbson_invalid_id_get_type());
   PyModule_AddObject(module, "DBRef", (PyObject *)cbson_dbref_get_type());

   PyDateTime_IMPORT;
}
