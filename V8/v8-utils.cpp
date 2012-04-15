////////////////////////////////////////////////////////////////////////////////
/// @brief V8 utility functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-utils.h"

#include <fstream>
#include <locale>

#include <Basics/Dictionary.h>
#include <Basics/StringUtils.h>
#include <BasicsC/conversions.h>
#include <BasicsC/csv.h>
#include <BasicsC/files.h>
#include <BasicsC/logging.h>
#include <BasicsC/process-utils.h>
#include <BasicsC/string-buffer.h>
#include <BasicsC/strings.h>

#include "VocBase/query-base.h"

#include "V8/v8-conv.h"

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                 EXECUTION CONTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief execution context
////////////////////////////////////////////////////////////////////////////////

typedef struct js_exec_context_s {
  v8::Persistent<v8::Function> _func;
  v8::Persistent<v8::Object> _arguments;
}
js_exec_context_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new execution context
////////////////////////////////////////////////////////////////////////////////

TRI_js_exec_context_t TRI_CreateExecutionContext (char const* script) {
  js_exec_context_t* ctx;

  // execute script inside the context
  v8::Handle<v8::Script> compiled = v8::Script::Compile(v8::String::New(script), 
                                                        v8::String::New("--script--"));
    
  // compilation failed, print errors that happened during compilation
  if (compiled.IsEmpty()) {
    return 0;
  }
    
  // compute the function
  v8::Handle<v8::Value> val = compiled->Run();
    
  if (val.IsEmpty()) {
    return 0;
  }

  ctx = new js_exec_context_t;

  ctx->_func = v8::Persistent<v8::Function>::New(v8::Handle<v8::Function>::Cast(val));
  ctx->_arguments = v8::Persistent<v8::Object>::New(v8::Object::New());

  // return the handle
  return (TRI_js_exec_context_t) ctx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees an new execution context
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeExecutionContext (TRI_js_exec_context_t context) {
  js_exec_context_t* ctx;

  ctx = (js_exec_context_t*) context;

  ctx->_func.Dispose();
  ctx->_func.Clear();

  ctx->_arguments.Dispose();
  ctx->_arguments.Clear();

  delete ctx;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an json array
////////////////////////////////////////////////////////////////////////////////

bool TRI_DefineJsonArrayExecutionContext (TRI_js_exec_context_t context,
                                          TRI_json_t* json) {
  js_exec_context_t* ctx;
  v8::Handle<v8::Value> result;

  ctx = (js_exec_context_t*) context;

  assert(json->_type == TRI_JSON_ARRAY);

  size_t n = json->_value._objects._length;

  for (size_t i = 0;  i < n;  i += 2) {
    TRI_json_t* key = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);

    if (key->_type != TRI_JSON_STRING) {
      continue;
    }

    TRI_json_t* j = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i + 1);
    v8::Handle<v8::Value> val = TRI_ObjectJson(j);

    ctx->_arguments->Set(v8::String::New(key->_value._string.data), val);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines documents in a join/where - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

bool TRI_DefineWhereExecutionContextX (TRI_js_exec_context_t context,
                                      const TRI_select_join_t* join,
                                      const size_t level,
                                      const bool isJoin) {
  js_exec_context_t* ctx;
  TRI_doc_mptr_t* document;
  
  ctx = (js_exec_context_t*) context;

  for (size_t i = 0; i <= level; i++) {
    TRI_join_part_t* part = (TRI_join_part_t*) join->_parts._buffer[i];

    if (part->_type != JOIN_TYPE_LIST || (isJoin && (level == i))) {
      // part is a single-document container
      document = (TRI_doc_mptr_t*) part->_singleDocument;
      if (!document) {
        ctx->_arguments->Set(v8::String::New(part->_alias), v8::Null());
      } 
      else {
        v8::Handle<v8::Value> result;
        bool ok = TRI_ObjectDocumentPointer(part->_collection->_collection, document, &result);
        if (!ok) {
          return false;
        }
        ctx->_arguments->Set(v8::String::New(part->_alias), result);
      }

      if (part->_extraData._size) {
        // make extra values available
        ctx->_arguments->Set(v8::String::New(part->_extraData._alias), v8::Number::New(*((double*) part->_extraData._singleValue)));
      }
    }
    else {
      // part is a multi-document container
      v8::Handle<v8::Array> array = v8::Array::New();
      size_t pos = 0;
      for (size_t n = 0; n < part->_listDocuments._length; n++) {
        document = (TRI_doc_mptr_t*) part->_listDocuments._buffer[n];
        if (document) {
          v8::Handle<v8::Value> result;
          bool ok = TRI_ObjectDocumentPointer(part->_collection->_collection, document, &result);
          if (!ok) {
            return false;
          }
          array->Set(pos++, result);
        }
      }
      ctx->_arguments->Set(v8::String::New(part->_alias), array);
      
      if (part->_extraData._size) {
        // make extra values available
        v8::Handle<v8::Array> array = v8::Array::New();
        uint32_t pos = 0;
        for (size_t n = 0; n < part->_extraData._listValues._length; n++) {
          double* data = (double*) part->_extraData._listValues._buffer[n];
          if (data) {
            v8::Handle<v8::Value> result;
            array->Set(pos++, v8::Number::New(*data));
          }
        }
        ctx->_arguments->Set(v8::String::New(part->_extraData._alias), array);
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines documents in a join/where
////////////////////////////////////////////////////////////////////////////////

bool TRI_DefineWhereExecutionContext (TRI_query_instance_t* const instance,
                                      TRI_js_exec_context_t context,
                                      const size_t level,
                                      const bool isJoin) {
  js_exec_context_t* ctx;
  TRI_doc_mptr_t* document;
  
  ctx = (js_exec_context_t*) context;

  for (size_t i = 0; i <= level; i++) {
    TRI_join_part_t* part = (TRI_join_part_t*) instance->_join._buffer[i];

    assert(part);

    if ((isJoin && !part->_mustMaterialize._join) ||
        (!isJoin && !part->_mustMaterialize._where)) {
      // no need to materialize query part
      continue;
    }

    if (part->_type != JOIN_TYPE_LIST || (isJoin && (level == i))) {
      // part is a single-document container
      document = (TRI_doc_mptr_t*) part->_singleDocument;
      if (!document) {
        ctx->_arguments->Set(v8::String::New(part->_alias), v8::Null());
      } 
      else {
        v8::Handle<v8::Value> result;
        bool ok = TRI_ObjectDocumentPointer(part->_collection->_collection, document, &result);
        if (!ok) {
          return false;
        }
        ctx->_arguments->Set(v8::String::New(part->_alias), result);
      }

      if (part->_extraData._size) {
        // make extra values available
        ctx->_arguments->Set(v8::String::New(part->_extraData._alias), v8::Number::New(*((double*) part->_extraData._singleValue)));
      }
    }
    else {
      // part is a multi-document container
      v8::Handle<v8::Array> array = v8::Array::New();
      uint32_t pos = 0;
      for (size_t n = 0; n < part->_listDocuments._length; n++) {
        document = (TRI_doc_mptr_t*) part->_listDocuments._buffer[n];
        if (document) {
          v8::Handle<v8::Value> result;
          bool ok = TRI_ObjectDocumentPointer(part->_collection->_collection, document, &result);
          if (!ok) {
            return false;
          }
          array->Set(pos++, result);
        }
      }
      ctx->_arguments->Set(v8::String::New(part->_alias), array);
      
      if (part->_extraData._size) {
        // make extra values available
        v8::Handle<v8::Array> array = v8::Array::New();
        size_t pos = 0;
        for (size_t n = 0; n < part->_extraData._listValues._length; n++) {
          double* data = (double*) part->_extraData._listValues._buffer[n];
          if (data) {
            v8::Handle<v8::Value> result;
            array->Set(pos++, v8::Number::New(*data));
          }
        }
        ctx->_arguments->Set(v8::String::New(part->_extraData._alias), array);
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a document composed of multiple elements
////////////////////////////////////////////////////////////////////////////////

bool TRI_DefineSelectExecutionContext (TRI_js_exec_context_t context,
                                       TRI_rc_result_t* resultState) {
  js_exec_context_t* ctx;
  TRI_sr_documents_t document;
  TRI_sr_documents_t* docPtr;
  TRI_select_size_t* numPtr;
  TRI_select_size_t num;
  TRI_select_result_t* result;

  assert(resultState); 
  numPtr = (TRI_select_size_t*) resultState->_dataPtr;
  if (!numPtr) {
    return false;
  }
  result = resultState->_selectResult;

  ctx = (js_exec_context_t*) context;

  for (size_t i = 0; i < result->_dataParts->_length; i++) {
    TRI_select_datapart_t* part = (TRI_select_datapart_t*) result->_dataParts->_buffer[i];

    num = *numPtr++;
    docPtr = (TRI_sr_documents_t*) numPtr;

    if (part->_type == RESULT_PART_DOCUMENT_SINGLE) {
      document = (TRI_sr_documents_t) *docPtr++;
      if (!document) {
        ctx->_arguments->Set(v8::String::New(part->_alias), v8::Null());
      } 
      else {
        v8::Handle<v8::Value> result;
        TRI_doc_mptr_t masterPointer;
        TRI_MarkerMasterPointer(document, &masterPointer);
        bool ok = TRI_ObjectDocumentPointer(part->_collection, &masterPointer, &result);
        if (!ok) {
          return false;
        }
        ctx->_arguments->Set(v8::String::New(part->_alias), result);
      }
    }
    else if (part->_type == RESULT_PART_DOCUMENT_MULTI) {
      // part is a multi-document container
      v8::Handle<v8::Array> array = v8::Array::New();
      size_t pos = 0;
      for (size_t i = 0; i < num; i++) {
        document = (TRI_sr_documents_t) *docPtr++;
        if (document) {
          v8::Handle<v8::Value> result;
          TRI_doc_mptr_t masterPointer;
          TRI_MarkerMasterPointer(document, &masterPointer);
          bool ok = TRI_ObjectDocumentPointer(part->_collection, &masterPointer, &result);
          if (!ok) {
            return false;
          }
          array->Set(pos++, result);
        }
      }
      ctx->_arguments->Set(v8::String::New(part->_alias), array);
    }
    else if (part->_type == RESULT_PART_VALUE_SINGLE) {
      void* value = (void*) docPtr;
      ctx->_arguments->Set(v8::String::New(part->_alias), v8::Number::New(*(double*) value));
      docPtr = (TRI_sr_documents_t*) ((uint8_t*) docPtr + part->_extraDataSize);
    }
    else if (part->_type == RESULT_PART_VALUE_MULTI) {
      v8::Handle<v8::Array> array = v8::Array::New();
      size_t pos = 0;
      for (size_t i = 0; i < num; i++) {
        void* value = (void*) docPtr;
        array->Set(pos++, v8::Number::New(*(double*) value));
        docPtr = (TRI_sr_documents_t*) ((uint8_t*) docPtr + part->_extraDataSize);
      }
      ctx->_arguments->Set(v8::String::New(part->_alias), array);
    }
    
    numPtr = (TRI_select_size_t*) docPtr;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a v8 object from a document list
////////////////////////////////////////////////////////////////////////////////

static bool MakeObject (TRI_select_result_t* result, TRI_sr_documents_t* docPtr, void* storage) {
  TRI_sr_documents_t document;
  TRI_select_size_t* numPtr;
  TRI_select_size_t num;

  v8::Handle<v8::Object> obj = v8::Object::New();

  numPtr = (TRI_select_size_t*) docPtr;
  
  for (size_t i = 0; i < result->_dataParts->_length; i++) {
    TRI_select_datapart_t* part = 
      (TRI_select_datapart_t*) result->_dataParts->_buffer[i];

    assert(part);

    if (!part->_mustMaterialize._order) {
      // no need to materialize
      continue;
    }

    num = *numPtr++;
    docPtr = (TRI_sr_documents_t*) numPtr;

    if (part->_type == RESULT_PART_DOCUMENT_SINGLE) {
      document = (TRI_sr_documents_t) *docPtr++;
      if (!document) {
        obj->Set(v8::String::New(part->_alias), v8::Null());
      } 
      else {
        v8::Handle<v8::Value> result;
        TRI_doc_mptr_t masterPointer;
        TRI_MarkerMasterPointer(document, &masterPointer);
        bool ok = TRI_ObjectDocumentPointer(part->_collection, &masterPointer, &result);
        if (!ok) {
          return false;
        }
        obj->Set(v8::String::New(part->_alias), result);
      }
    }
    else if (part->_type == RESULT_PART_DOCUMENT_MULTI) {
      // part is a multi-document container
      v8::Handle<v8::Array> array = v8::Array::New();
      uint32_t pos = 0;
      for (size_t i = 0; i < num; i++) {
        document = (TRI_sr_documents_t) *docPtr++;
        if (document) {
          v8::Handle<v8::Value> result;
          TRI_doc_mptr_t masterPointer;
          TRI_MarkerMasterPointer(document, &masterPointer);
          bool ok = TRI_ObjectDocumentPointer(part->_collection, &masterPointer, &result);
          if (!ok) {
            return false;
          }
          array->Set(pos++, result);
        }
      }
      obj->Set(v8::String::New(part->_alias), array);
    }
    else if (part->_type == RESULT_PART_VALUE_SINGLE) {
      void* value = (void*) docPtr;
      obj->Set(v8::String::New(part->_alias), v8::Number::New(*(double*) value));
      docPtr = (TRI_sr_documents_t*) ((uint8_t*) docPtr + part->_extraDataSize);
    }
    else if (part->_type == RESULT_PART_VALUE_MULTI) {
      v8::Handle<v8::Array> array = v8::Array::New();
      uint32_t pos = 0;
      for (size_t i = 0; i < num; i++) {
        void* value = (void*) docPtr;
        array->Set(pos++, v8::Number::New(*(double*) value));
        docPtr = (TRI_sr_documents_t*) ((uint8_t*) docPtr + part->_extraDataSize);
      }
      obj->Set(v8::String::New(part->_alias), array);
    }
    
    numPtr = (TRI_select_size_t*) docPtr;
  }

  * (v8::Handle<v8::Object>*) storage = obj;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines an execution context with two documents for comparisons
////////////////////////////////////////////////////////////////////////////////

bool TRI_DefineCompareExecutionContext (TRI_js_exec_context_t context,
                                        TRI_select_result_t* result,
                                        TRI_sr_documents_t* left,
                                        TRI_sr_documents_t* right) {
  js_exec_context_t* ctx;
  
  ctx = (js_exec_context_t*) context;
  assert(ctx);
  v8::Handle<v8::Object> leftValue;
  v8::Handle<v8::Object> rightValue;

  MakeObject(result, left, &leftValue);
  MakeObject(result, right, &rightValue);

  ctx->_arguments->Set(v8::String::New("l"), leftValue);
  ctx->_arguments->Set(v8::String::New("r"), rightValue);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an execution context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteExecutionContext (TRI_js_exec_context_t context, void* storage) {
  js_exec_context_t* ctx;

  ctx = (js_exec_context_t*) context;
  // convert back into a handle
  v8::Persistent<v8::Function> func = ctx->_func;

  // and execute the function
  v8::Handle<v8::Value> args[] = { ctx->_arguments };
  v8::Handle<v8::Value> result = func->Call(func, 1, args);
  
  if (result.IsEmpty()) {
    return false;
  }

  * (v8::Handle<v8::Value>*) storage = result;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an execution context for a condition - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteConditionExecutionContextX (TRI_js_exec_context_t context, bool* r) {
  js_exec_context_t* ctx;

  ctx = (js_exec_context_t*) context;

  // convert back into a handle
  v8::Persistent<v8::Function> func = ctx->_func;

  // and execute the function
  v8::Handle<v8::Value> args[] = { ctx->_arguments };
  v8::Handle<v8::Value> result = func->Call(func, 1, args);
  
  if (result.IsEmpty()) {
    return false;
  }

  *r = TRI_ObjectToBoolean(result);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an execution context for a condition
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteConditionExecutionContext (TRI_query_instance_t* const instance, 
                                           TRI_js_exec_context_t context, 
                                           bool* r) {
  js_exec_context_t* ctx;

  ctx = (js_exec_context_t*) context;

  // convert back into a handle
  v8::Persistent<v8::Function> func = ctx->_func;

  // and execute the function
  v8::Handle<v8::Value> args[] = { ctx->_arguments };
  v8::Handle<v8::Value> result = func->Call(func, 1, args);
  
  if (result.IsEmpty()) {
    return false;
  }

  *r = TRI_ObjectToBoolean(result);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an execution context for ref access
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteRefExecutionContext (TRI_js_exec_context_t context, TRI_json_t* r) {
  js_exec_context_t* ctx;

  ctx = (js_exec_context_t*) context;

  // convert back into a handle
  v8::Persistent<v8::Function> func = ctx->_func;

  // and execute the function
  v8::Handle<v8::Value> args[] = { ctx->_arguments };
  v8::Handle<v8::Value> result = func->Call(func, 1, args);
  
  if (result.IsEmpty() || !result->IsArray()) {
    return false;
  }
    
  v8::Handle<v8::Object> obj = result->ToObject(); 

  uint32_t position = 0;
  while (true) {
    if (!obj->Has(position)) {
      break;
    }
    v8::Handle<v8::Value> parameter = obj->Get(position++);
    if (parameter->IsNumber()) {
      v8::Handle<v8::Number> numberParameter = parameter->ToNumber();
      TRI_PushBack2ListJson(r, TRI_CreateNumberJson(numberParameter->Value()));
    }
    else if (parameter->IsString() ) {
      v8::Handle<v8::String>  stringParameter= parameter->ToString();
      v8::String::Utf8Value str(stringParameter);
      TRI_PushBack2ListJson(r, TRI_CreateStringCopyJson(*str));
    }
    else {
      continue;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an execution context for order by
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteOrderExecutionContext (TRI_js_exec_context_t context, int* r) {
  v8::TryCatch tryCatch;
  js_exec_context_t* ctx;

  ctx = (js_exec_context_t*) context;

  // convert back into a handle
  v8::Persistent<v8::Function> func = ctx->_func;

  // and execute the function
  v8::Handle<v8::Value> args[] = { ctx->_arguments };
  v8::Handle<v8::Value> result = func->Call(func, 1, args);

  if (result.IsEmpty()) {
    return false;
  }

  *r = (int) TRI_ObjectToDouble(result);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   WEAK DICTIONARY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_WEAK_DIRECTORY_TYPE = 1000;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief dictionary / key pair
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  void* _dictionary;
  char* _key;
}
wd_key_pair_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief weak dictionary callback
////////////////////////////////////////////////////////////////////////////////

static void WeakDictionaryCallback (v8::Persistent<v8::Value> object, void* parameter) {
  typedef Dictionary< v8::Persistent<v8::Value>* > WD;

  WD* dictionary;
  char* key;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  dictionary = (WD*) ((wd_key_pair_t*) parameter)->_dictionary;
  key = ((wd_key_pair_t*) parameter)->_key;

  LOG_FATAL("weak-callback for dictionary called");

  // dispose and clear the persistent handle
  WD::KeyValue const* kv = dictionary->lookup(key);

  if (kv != 0) {
    const_cast<WD::KeyValue*>(kv)->_value->Dispose();
    const_cast<WD::KeyValue*>(kv)->_value->Clear();

    delete const_cast<WD::KeyValue*>(kv)->_value;

    dictionary->erase(key);
  }

  TRI_FreeString(key);
  TRI_Free(parameter);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invocation callback
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> WeakDictionaryInvocationCallback (v8::Arguments const& args) {
  typedef Dictionary< v8::Persistent<v8::Value>* > WD;
  static uint64_t MIN_SIZE = 100;

  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> self = args.Holder();

  if (self->InternalFieldCount() <= 1) {
    return v8::ThrowException(v8::String::New("corrupted weak dictionary"));
  }

  WD* dictionary = new WD(MIN_SIZE);
  v8::Handle<v8::Value> external = v8::Persistent<v8::Value>::New(v8::External::New(dictionary));

  self->SetInternalField(SLOT_CLASS, external);
  self->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_WEAK_DIRECTORY_TYPE));

  return self;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets an entry
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetWeakDictionary (v8::Local<v8::String> name,
                                                   const v8::AccessorInfo& info) {
  typedef Dictionary< v8::Persistent<v8::Value>* > WD;

  v8::HandleScope scope;

  // sanity check
  v8::Handle<v8::Object> self = info.Holder();

  // get the dictionary
  WD* dictionary = TRI_UnwrapClass<WD >(self, WRP_WEAK_DIRECTORY_TYPE);

  if (dictionary == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted weak dictionary")));
  }

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);
  
  if (key == "") {
    return scope.Close(v8::Undefined());
  }  

  // check the dictionary
  WD::KeyValue const* kv = dictionary->lookup(key.c_str());

  if (kv == 0) {
    return scope.Close(v8::Undefined());
  }

  return scope.Close(*kv->_value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets an entry
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapSetWeakDictionary (v8::Local<v8::String> name,
                                                   v8::Local<v8::Value> value,
                                                   const v8::AccessorInfo& info) {
  typedef Dictionary< v8::Persistent<v8::Value>* > WD;

  v8::HandleScope scope;

  // sanity check
  v8::Handle<v8::Object> self = info.Holder();

  // get the dictionary
  WD* dictionary = TRI_UnwrapClass<WD >(self, WRP_WEAK_DIRECTORY_TYPE);

  if (dictionary == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted weak dictionary")));
  }

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);
  
  if (key == "") {
    return scope.Close(v8::Undefined());
  }  

  char* ckey = TRI_DuplicateString(key.c_str());

  // create a new weak persistent
  v8::Persistent<v8::Value>* persistent = new v8::Persistent<v8::Value>();
  *persistent = v8::Persistent<v8::Value>::New(value);

  // enter a value into the dictionary
  WD::KeyValue const* kv = dictionary->lookup(ckey);

  if (kv != 0) {
    kv->_value->Dispose();
    kv->_value->Clear();

    delete kv->_value;

    const_cast<WD::KeyValue*>(kv)->_value = persistent;
  }
  else {
    dictionary->insert(ckey, persistent);
  }

  wd_key_pair_t* p = new wd_key_pair_t;
  p->_dictionary = dictionary;
  p->_key = ckey;

  persistent->MakeWeak(p, WeakDictionaryCallback);

  return scope.Close(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           GENERAL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief reads/execute a file into/in the current context
////////////////////////////////////////////////////////////////////////////////

static bool LoadJavaScriptFile (v8::Handle<v8::Context> context,
                                char const* filename,
                                bool execute) {
  v8::HandleScope handleScope;

  char* content = TRI_SlurpFile(filename);

  if (content == 0) {
    LOG_TRACE("cannot loaded java script file '%s': %s", filename, TRI_last_error());
    return false;
  }

  if (execute) {
    char* contentWrapper = TRI_Concatenate5String("(function() { ",
                                                  content,
                                                  "/* end-of-file '",
                                                  filename,
                                                  "' */ })()");

    TRI_FreeString(content);

    content = contentWrapper;
  }

  v8::Handle<v8::String> name = v8::String::New(filename);
  v8::Handle<v8::String> source = v8::String::New(content);

  TRI_FreeString(content);

  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);

  // compilation failed, print errors that happened during compilation
  if (script.IsEmpty()) {
    return false;
  }

  // execute script
  v8::Handle<v8::Value> result = script->Run();

  if (result.IsEmpty()) {
    return false;
  }

  LOG_TRACE("loaded java script file: '%s'", filename);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads all files from a directory into the current context
////////////////////////////////////////////////////////////////////////////////

static bool LoadJavaScriptDirectory (v8::Handle<v8::Context> context, char const* path, bool execute) {
  v8::HandleScope scope;
  TRI_vector_string_t files;
  bool result;
  regex_t re;
  size_t i;

  LOG_TRACE("loading java script directory: '%s'", path);

  files = TRI_FilesDirectory(path);

  regcomp(&re, "^(.*)\\.js$", REG_ICASE | REG_EXTENDED);

  result = true;

  for (i = 0;  i < files._length;  ++i) {
    v8::TryCatch tryCatch;
    bool ok;
    char const* filename;
    char* full;

    filename = files._buffer[i];

    if (! regexec(&re, filename, 0, 0, 0) == 0) {
      continue;
    }

    full = TRI_Concatenate2File(path, filename);
    if (!full) {
      continue;
    }
    ok = LoadJavaScriptFile(context, full, execute);
    TRI_FreeString(full);

    result = result && ok;

    if (! ok) {
      TRI_LogV8Exception(&tryCatch);
    }
  }

  TRI_DestroyVectorString(&files);
  regfree(&re);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a script
///
/// @FUN{internal.execute(@FA{script}, @FA{sandbox}, @FA{filename})}
///
/// Executes the @FA{script} with the @FA{sandbox} as context. Global variables
/// assigned inside the @FA{script}, will be visible in the @FA{sandbox} object
/// after execution. The @FA{filename} is used for displaying error
/// messages.
///
/// If @FA{sandbox} is undefined, then @FN{execute} uses the current context.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Execute (v8::Arguments const& argv) {
  v8::HandleScope scope;
  size_t i;

  // extract arguments
  if (argv.Length() != 3) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: execute(<script>, <sandbox>, <filename>)")));
  }

  v8::Handle<v8::Value> source = argv[0];
  v8::Handle<v8::Value> sandboxValue = argv[1];
  v8::Handle<v8::Value> filename = argv[2];

  if (! source->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("<script> must be a string")));
  }

  bool useSandbox = sandboxValue->IsObject();
  v8::Handle<v8::Object> sandbox;
  v8::Handle<v8::Context> context;

  if (useSandbox) {
    sandbox = sandboxValue->ToObject();

    // create new context
    context = v8::Context::New();
    context->Enter();

    // copy sandbox into context
    v8::Handle<v8::Array> keys = sandbox->GetPropertyNames();

    for (i = 0; i < keys->Length(); i++) {
      v8::Handle<v8::String> key = keys->Get(v8::Integer::New(i))->ToString();
      v8::Handle<v8::Value> value = sandbox->Get(key);

      if (TRI_IsTraceLogging(__FILE__)) {
        v8::String::Utf8Value keyName(key);

        if (*keyName != 0) {
          LOG_TRACE("copying key '%s' from sandbox to context", *keyName);
        }
      }

      if (value == sandbox) {
        value = context->Global();
      }

      context->Global()->Set(key, value);
    }
  }

  // execute script inside the context
  v8::Handle<v8::Script> script = v8::Script::Compile(source->ToString(), filename);

  // compilation failed, print errors that happened during compilation
  if (script.IsEmpty()) {
    if (useSandbox) {
      context->DetachGlobal();
      context->Exit();
    }

    return scope.Close(v8::Undefined());
  }

  // compilation succeeded, run the script
  v8::Handle<v8::Value> result = script->Run();

  if (result.IsEmpty()) {
    if (useSandbox) {
      context->DetachGlobal();
      context->Exit();
    }

    return scope.Close(v8::Undefined());
  }

  // copy result back into the sandbox
  if (useSandbox) {
    v8::Handle<v8::Array> keys = context->Global()->GetPropertyNames();

    for (i = 0; i < keys->Length(); i++) {
      v8::Handle<v8::String> key = keys->Get(v8::Integer::New(i))->ToString();
      v8::Handle<v8::Value> value = context->Global()->Get(key);

      if (TRI_IsTraceLogging(__FILE__)) {
        v8::String::Utf8Value keyName(key);

        if (*keyName != 0) {
          LOG_TRACE("copying key '%s' from context to sandbox", *keyName);
        }
      }

      if (value == context->Global()) {
        value = sandbox;
      }

      sandbox->Set(key, value);
    }

    context->DetachGlobal();
    context->Exit();
  }

  if (useSandbox) {
    return scope.Close(v8::True());
  }
  else {
    return scope.Close(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file and executes it
///
/// @FUN{internal.load(@FA{filename})}
///
/// Reads in a files and executes the contents in the current context.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Load (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: load(<filename>)")));
  }

  v8::String::Utf8Value name(argv[0]);

  if (*name == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be a string")));
  }

  char* content = TRI_SlurpFile(*name);

  if (content == 0) {
    return scope.Close(v8::ThrowException(v8::String::New(TRI_last_error())));
  }

  TRI_ExecuteStringVocBase(v8::Context::GetCurrent(), v8::String::New(content), argv[0], false);
  TRI_FreeString(content);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message
///
/// @FUN{internal.log(@FA{level}, @FA{message})}
///
/// Logs the @FA{message} at the given log @FA{level}.
///
/// Valid log-level are:
///
/// - fatal
/// - error
/// - warning
/// - info
/// - debug
/// - trace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Log (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: log(<level>, <message>)")));
  }

  v8::String::Utf8Value level(argv[0]);

  if (*level == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<level> must be a string")));
  }

  v8::String::Utf8Value message(argv[1]);

  if (*message == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<message> must be a string")));
  }

  if (TRI_CaseEqualString(*level, "fatal")) {
    LOG_FATAL("%s", *message);
  }
  else if (TRI_CaseEqualString(*level, "error")) {
    LOG_ERROR("%s", *message);
  }
  else if (TRI_CaseEqualString(*level, "warning")) {
    LOG_WARNING("%s", *message);
  }
  else if (TRI_CaseEqualString(*level, "info")) {
    LOG_INFO("%s", *message);
  }
  else if (TRI_CaseEqualString(*level, "debug")) {
    LOG_DEBUG("%s", *message);
  }
  else if (TRI_CaseEqualString(*level, "trace")) {
    LOG_TRACE("%s", *message);
  }
  else {
    LOG_ERROR("(unknown log level '%s') %s", *level, *message);
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the log-level
///
/// @FUN{internal.logLevel()}
///
/// Returns the current log-level as string.
///
/// @verbinclude fluent37
///
/// @FUN{internal.logLevel(@FA{level})}
///
/// Changes the current log-level. Valid log-level are:
///
/// - fatal
/// - error
/// - warning
/// - info
/// - debug
/// - trace
///
/// @verbinclude fluent38
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LogLevel (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (1 <= argv.Length()) {
    v8::String::Utf8Value str(argv[0]);

    TRI_SetLogLevelLogging(*str);
  }

  return scope.Close(v8::String::New(TRI_LogLevelLogging()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs the arguments
///
/// @FUN{internal.output(@FA{string1}, @FA{string2}, @FA{string3}, ...)}
///
/// Outputs the arguments to standard output.
///
/// @verbinclude fluent39
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Output (v8::Arguments const& argv) {
  for (int i = 0; i < argv.Length(); i++) {
    v8::HandleScope scope;

    // extract the next argument
    v8::Handle<v8::Value> val = argv[i];

    // convert it into a string
    v8::String::Utf8Value utf8(val);

    if (*utf8 == 0) {
      continue;
    }

    // print the argument
    char const* ptr = *utf8;
    size_t len = utf8.length();

    while (0 < len) {
      ssize_t n = write(1, ptr, len);

      if (n < 0) {
        return v8::Undefined();
      }

      len -= n;
      ptr += n;
    }
  }

  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads in a file
///
/// @FUN{internal.read(@FA{filename})}
///
/// Reads in a files and returns the content as string.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Read (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: read(<filename>)")));
  }

  v8::String::Utf8Value name(argv[0]);

  if (*name == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be a string")));
  }

  char* content = TRI_SlurpFile(*name);

  if (content == 0) {
    return scope.Close(v8::ThrowException(v8::String::New(TRI_last_error())));
  }

  v8::Handle<v8::String> result = v8::String::New(content);

  TRI_FreeString(content);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current process information
///
/// @FUN{internal.processStat()}
///
/// Returns information about the current process:
///
/// - minorPageFaults: The number of minor faults the process has made
///   which have not required loading a memory page from disk.
///   
/// - majorPageFaults: The number of major faults the process has made
///   which have required loading a memory page from disk.
///
/// - userTime: Amount of time that this process has been scheduled in
///   user mode, measured in clock ticks.
///
/// - systemTime: Amount of time that this process has been scheduled
///   in kernel mode, measured in clock ticks.
///
/// - numberThreads: Number of threads in this process.
///
/// - residentSize: Resident Set Size: number of pages the process has
///   in real memory.  This is just the pages which count toward text,
///   data, or stack space.  This does not include pages which have
///   not been demand-loaded in, or which are swapped out.
///
/// - virtualSize: Virtual memory size in bytes.
///
/// @verbinclude system1
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ProcessStat (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> result = v8::Object::New();

  TRI_process_info_t info = TRI_ProcessInfoSelf();

  result->Set(v8::String::New("minorPageFaults"), v8::Number::New((double) info._minorPageFaults));
  result->Set(v8::String::New("majorPageFaults"), v8::Number::New((double) info._majorPageFaults));
  result->Set(v8::String::New("userTime"), v8::Number::New((double) info._userTime / (double) info._scClkTck));
  result->Set(v8::String::New("systemTime"), v8::Number::New((double) info._systemTime / (double) info._scClkTck));
  result->Set(v8::String::New("numberThreads"), v8::Number::New((double) info._numberThreads));
  result->Set(v8::String::New("residentSize"), v8::Number::New((double) info._residentSize));
  result->Set(v8::String::New("virtualSize"), v8::Number::New((double) info._virtualSize));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief formats the arguments
///
/// @FUN{internal.sprintf(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to the format string @FA{format}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SPrintF (v8::Arguments const& argv) {
  v8::HandleScope scope;

  size_t len = argv.Length();

  if (len == 0) {
    return scope.Close(v8::String::New(""));
  }

  v8::String::Utf8Value format(argv[0]);

  if (*format == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<format> must be a string")));
  }

  string result;

  size_t p = 1;

  for (char const* ptr = *format;  *ptr;  ++ptr) {
    if (*ptr == '%') {
      ++ptr;

      switch (*ptr) {
        case '%':
          result += '%';
          break;

        case 'd':
        case 'f':
        case 'i': {
          if (len <= p) {
            return scope.Close(v8::ThrowException(v8::String::New("not enough arguments")));
          }

          bool e;
          double f = TRI_ObjectToDouble(argv[p], e);

          if (e) {
            string msg = StringUtils::itoa(p) + ".th argument must be a number";
            return scope.Close(v8::ThrowException(v8::String::New(msg.c_str())));
          }

          char b[1024];

          if (*ptr == 'f') {
            snprintf(b, sizeof(b), "%f", f);
          }
          else {
            snprintf(b, sizeof(b), "%ld", (long) f);
          }

          ++p;

          result += b;

          break;
        }

        case 'o':
        case 's': {
          if (len <= p) {
            return scope.Close(v8::ThrowException(v8::String::New("not enough arguments")));
          }

          v8::String::Utf8Value text(argv[p]);

          if (*text == 0) {
            string msg = StringUtils::itoa(p) + ".th argument must be a string";
            return scope.Close(v8::ThrowException(v8::String::New(msg.c_str())));
          }

          ++p;

          result += *text;

          break;
        }

        default: {
          string msg = "found illegal format directive '" + string(1, *ptr) + "'";
          return scope.Close(v8::ThrowException(v8::String::New(msg.c_str())));
        }
      }
    }
    else {
      result += *ptr;
    }
  }

  for (size_t i = p;  i < len;  ++i) {
    v8::String::Utf8Value text(argv[i]);

    if (*text == 0) {
      string msg = StringUtils::itoa(i) + ".th argument must be a string";
      return scope.Close(v8::ThrowException(v8::String::New(msg.c_str())));
    }

    result += " ";
    result += *text;
  }

  return scope.Close(v8::String::New(result.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current time
///
/// @FUN{internal.time()}
///
/// Returns the current time in seconds.
///
/// @verbinclude fluent36
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Time (v8::Arguments const& argv) {
  v8::HandleScope scope;

  return scope.Close(v8::Number::New(TRI_microtime()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a file of any type or directory exists
///
/// @FUN{fs.exists(@FA{filename})}
///
/// Returns true if a file (of any type) or a directory exists at a given
/// path. If the file is a broken symbolic link, returns false.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Exists (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("exists: execute(<filename>)")));
  }

  v8::Handle<v8::Value> filename = argv[0];

  if (! filename->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be a string")));
  }

  v8::String::Utf8Value name(filename);

  if (*name == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be an UTF8 string")));
  }

  return scope.Close(TRI_ExistsFile(*name) ? v8::True() : v8::False());;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds attributes to array
////////////////////////////////////////////////////////////////////////////////

void TRI_AugmentObject (v8::Handle<v8::Value> value, TRI_json_t const* json) {
  v8::HandleScope scope;

  if (! value->IsObject()) {
    return;
  }

  if (json->_type != TRI_JSON_ARRAY) {
    return;
  }

  v8::Handle<v8::Object> object = value->ToObject();

  size_t n = json->_value._objects._length;

  for (size_t i = 0;  i < n;  i += 2) {
    TRI_json_t* key = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);

    if (key->_type != TRI_JSON_STRING) {
      continue;
    }

    TRI_json_t* j = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i + 1);
    v8::Handle<v8::Value> val = TRI_ObjectJson(j);

    object->Set(v8::String::New(key->_value._string.data), val);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reports an exception
////////////////////////////////////////////////////////////////////////////////

string TRI_StringifyV8Exception (v8::TryCatch* tryCatch) {
  v8::HandleScope handle_scope;

  v8::String::Utf8Value exception(tryCatch->Exception());
  const char* exceptionString = *exception;
  v8::Handle<v8::Message> message = tryCatch->Message();
  string result;

  // V8 didn't provide any extra information about this error; just print the exception.
  if (message.IsEmpty()) {
    if (exceptionString == 0) {
      result = "JavaScript exception\n";
    }
    else {
      result = "JavaScript exception: " + string(exceptionString) + "\n";
    }
  }
  else {
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    const char* filenameString = *filename;
    int linenum = message->GetLineNumber();
    int start = message->GetStartColumn() + 1;
    int end = message->GetEndColumn();

    if (filenameString == 0) {
      if (exceptionString == 0) {
        result = "JavaScript exception\n";
      }
      else {
        result = "JavaScript exception: " + string(exceptionString) + "\n";
      }
    }
    else {
      if (exceptionString == 0) {
        result = "JavaScript exception in file '" + string(filenameString) + "' at "
               + StringUtils::itoa(linenum) + "," + StringUtils::itoa(start) + "\n";
      }
      else {
        result = "JavaScript exception in file '" + string(filenameString) + "' at "
               + StringUtils::itoa(linenum) + "," + StringUtils::itoa(start)
               + ": " + exceptionString + "\n";
      }
    }

    v8::String::Utf8Value sourceline(message->GetSourceLine());

    if (*sourceline) {
      string l = *sourceline;

      result += "!" + l + "\n";
      
      if (1 < start) {
        l = string(start - 1, ' ');
      }
      else {
        l = "";
      }

      l += string((size_t)(end - start + 1), '^');

      result += "!" + l + "\n";
    }

    v8::String::Utf8Value stacktrace(tryCatch->StackTrace());

    if (*stacktrace && stacktrace.length() > 0) {
      result += "stacktrace: " + string(*stacktrace) + "\n";
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an exception and stacktrace
////////////////////////////////////////////////////////////////////////////////

void TRI_LogV8Exception (v8::TryCatch* tryCatch) {
  v8::HandleScope handle_scope;

  v8::String::Utf8Value exception(tryCatch->Exception());
  const char* exceptionString = *exception;
  v8::Handle<v8::Message> message = tryCatch->Message();

  // V8 didn't provide any extra information about this error; just print the exception.
  if (message.IsEmpty()) {
    if (exceptionString == 0) {
      LOG_ERROR("JavaScript exception");
    }
    else {
      LOG_ERROR("JavaScript exception: %s", exceptionString);
    }
  }
  else {
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    const char* filenameString = *filename;
    int linenum = message->GetLineNumber();
    int start = message->GetStartColumn() + 1;
    int end = message->GetEndColumn();

    if (filenameString == 0) {
      if (exceptionString == 0) {
        LOG_ERROR("JavaScript exception");
      }
      else {
        LOG_ERROR("JavaScript exception: %s", exceptionString);
      }
    }
    else {
      if (exceptionString == 0) {
        LOG_ERROR("JavaScript exception in file '%s' at %d,%d", filenameString, linenum, start);
      }
      else {
        LOG_ERROR("JavaScript exception in file '%s' at %d,%d: %s", filenameString, linenum, start, exceptionString);
      }
    }

    v8::String::Utf8Value sourceline(message->GetSourceLine());

    if (*sourceline) {
      string l = *sourceline;

      LOG_ERROR("!%s", l.c_str());
      
      if (1 < start) {
        l = string(start - 1, ' ');
      }
      else {
        l = "";
      }

      l += string((size_t)(end - start + 1), '^');

      LOG_ERROR("!%s", l.c_str());
    }

    v8::String::Utf8Value stacktrace(tryCatch->StackTrace());

    if (*stacktrace && stacktrace.length() > 0) {
      LOG_ERROR("stacktrace: %s", *stacktrace);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file into the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadJavaScriptFile (v8::Handle<v8::Context> context, char const* filename) {
  return LoadJavaScriptFile(context, filename, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads all files from a directory into the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadJavaScriptDirectory (v8::Handle<v8::Context> context, char const* path) {
  return LoadJavaScriptDirectory(context, path, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a file in the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteJavaScriptFile (v8::Handle<v8::Context> context, char const* filename) {
  return LoadJavaScriptFile(context, filename, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes all files from a directory in the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteJavaScriptDirectory (v8::Handle<v8::Context> context, char const* path) {
  return LoadJavaScriptDirectory(context, path, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a string within a V8 context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteStringVocBase (v8::Handle<v8::Context> context,
                               v8::Handle<v8::String> source,
                               v8::Handle<v8::Value> name,
                               bool printResult) {
  v8::HandleScope scope;

  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);

  // compilation failed, print errors that happened during compilation
  if (script.IsEmpty()) {
    return false;
  }

  // compilation succeeded, run the script
  else {
    v8::Handle<v8::Value> result = script->Run();

    if (result.IsEmpty()) {
      return false;
    }
    else {

      // if all went well and the result wasn't undefined then print the returned value
      if (printResult && ! result->IsUndefined()) {
        v8::TryCatch tryCatch;

        v8::Handle<v8::String> printFuncName = v8::String::New("print");
        v8::Handle<v8::Function> print = v8::Handle<v8::Function>::Cast(context->Global()->Get(printFuncName));

        v8::Handle<v8::Value> args[] = { result };
        print->Call(print, 1, args);

        if (tryCatch.HasCaught()) {
          TRI_LogV8Exception(&tryCatch);
        }
      }

      return true;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 utils functions inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Utils (v8::Handle<v8::Context> context, string const& path) {
  v8::HandleScope scope;

  v8::Handle<v8::FunctionTemplate> ft;
  v8::Handle<v8::ObjectTemplate> rt;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  if (v8g == 0) {
    v8g = new TRI_v8_global_t;
    isolate->SetData(v8g);
  }

  // .............................................................................
  // create the Dictionary constructor
  // .............................................................................

  ft = v8::FunctionTemplate::New(WeakDictionaryInvocationCallback);
  ft->SetClassName(v8::String::New("WeakDictionary"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);    

  rt->SetNamedPropertyHandler(MapGetWeakDictionary,        // NamedPropertyGetter
                              MapSetWeakDictionary,        // NamedPropertySetter
                              0, //PropertyQueryWeakDictionary, // NamedPropertyQuery,
                              0,                           // NamedPropertyDeleter deleter = 0,
                              0 //KeysOfWeakDictionary         // NamedPropertyEnumerator,
                                                           // Handle<Value> data = Handle<Value>());
                              );

  v8g->DictionaryTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("WeakDictionary"), ft->GetFunction());
                         
  // .............................................................................
  // create the global functions
  // .............................................................................

  context->Global()->Set(v8::String::New("SYS_EXECUTE"),
                         v8::FunctionTemplate::New(JS_Execute)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("SYS_LOAD"),
                         v8::FunctionTemplate::New(JS_Load)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("SYS_LOG"),
                         v8::FunctionTemplate::New(JS_Log)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("SYS_LOG_LEVEL"),
                         v8::FunctionTemplate::New(JS_LogLevel)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("SYS_OUTPUT"),
                         v8::FunctionTemplate::New(JS_Output)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("SYS_READ"),
                         v8::FunctionTemplate::New(JS_Read)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("SYS_PROCESS_STAT"),
                         v8::FunctionTemplate::New(JS_ProcessStat)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("SYS_SPRINTF"),
                         v8::FunctionTemplate::New(JS_SPrintF)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("SYS_TIME"),
                         v8::FunctionTemplate::New(JS_Time)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("FS_EXISTS"),
                         v8::FunctionTemplate::New(JS_Exists)->GetFunction(),
                         v8::ReadOnly);

  // .............................................................................
  // create the global variables
  // .............................................................................

  vector<string> paths = StringUtils::split(path, ";:");

  v8::Handle<v8::Array> modulesPaths = v8::Array::New();

  for (uint32_t i = 0;  i < (uint32_t) paths.size();  ++i) {
    modulesPaths->Set(i, v8::String::New(paths[i].c_str()));
  }

  context->Global()->Set(v8::String::New("MODULES_PATH"), modulesPaths);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
