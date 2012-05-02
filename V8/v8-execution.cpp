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

#include "v8-execution.h"

#include <fstream>
#include <locale>

#include <v8.h>

#include "V8/v8-conv.h"

using namespace std;

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

bool TRI_ExecuteRefExecutionContext (TRI_memory_zone_t* zone, TRI_js_exec_context_t context, TRI_json_t* r) {
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

      TRI_json_t* tmp = TRI_CreateNumberJson(zone, numberParameter->Value());

      if (tmp == NULL) {
        return false;
      }

      int res = TRI_PushBack2ListJson(r, tmp);

      if (res != TRI_ERROR_NO_ERROR) {
        return false;
      }
    }
    else if (parameter->IsString() ) {
      v8::Handle<v8::String>  stringParameter= parameter->ToString();
      v8::String::Utf8Value str(stringParameter);

      TRI_json_t* tmp = TRI_CreateStringCopyJson(zone, *str);

      int res = TRI_PushBack2ListJson(r, tmp);

      if (res != TRI_ERROR_NO_ERROR) {
        return false;
      }
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
/// @brief executes a result context
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_ExecuteResultContext (TRI_js_exec_context_t context) {
  js_exec_context_t* ctx;

  ctx = (js_exec_context_t*) context;
  // convert back into a handle
  v8::Persistent<v8::Function> func = ctx->_func;

  // and execute the function
  v8::Handle<v8::Value> args[] = { ctx->_arguments };
  v8::Handle<v8::Value> result = func->Call(func, 1, args);
  
  if (result.IsEmpty()) {
    return NULL;
  }

  return TRI_ObjectToJson(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
