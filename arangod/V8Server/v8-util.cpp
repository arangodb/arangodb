////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-vocbaseprivate.h"
#include "Basics/conversions.h"
#include "VocBase/key-generator.h"
#include "V8/v8-conv.h"
#include "Utils/DocumentHelper.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the vocbase pointer from the current V8 context
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* GetContextVocBase () {
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  TRI_ASSERT_EXPENSIVE(v8g->_vocbase != nullptr);
  return static_cast<TRI_vocbase_t*>(v8g->_vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 tick id value from the internal tick id
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8TickId (TRI_voc_tick_t tick) {
  char buffer[21];
  size_t len = TRI_StringUInt64InPlace((uint64_t) tick, (char*) &buffer);

  return v8::String::New((const char*) buffer, (int) len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 revision id value from the internal revision id
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8RevisionId (TRI_voc_rid_t rid) {
  char buffer[21];
  size_t len = TRI_StringUInt64InPlace((uint64_t) rid, (char*) &buffer);

  return v8::String::New((const char*) buffer, (int) len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 document id value from the parameters
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8DocumentId (string const& collectionName,
                                    string const& key) {
  string const&& id = DocumentHelper::assembleDocumentId(collectionName, key);

  return v8::String::New(id.c_str(), (int) id.size());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an Ahuacatl error in a javascript object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> CreateErrorObjectAhuacatl (TRI_aql_error_t* error) {
  v8::HandleScope scope;

  char* message = TRI_GetErrorMessageAql(error);

  if (message != nullptr) {
    std::string str(message);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, message);

    return scope.Close(TRI_CreateErrorObject(error->_file, error->_line, TRI_GetErrorCodeAql(error), str));
  }

  return scope.Close(TRI_CreateErrorObject(error->_file, error->_line, TRI_ERROR_OUT_OF_MEMORY));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is a document identifier
////////////////////////////////////////////////////////////////////////////////

static bool ParseDocumentHandle (v8::Handle<v8::Value> const arg,
                                 string& collectionName,
                                 std::unique_ptr<char[]>& key) {
  TRI_ASSERT(collectionName.empty());

  if (! arg->IsString()) {
    return false;
  }

  // the handle must always be an ASCII string. These is no need to normalise it first
  v8::String::Utf8Value str(arg);

  if (*str == 0) {
    return false;
  }

  // collection name / document key
  size_t split;
  if (TRI_ValidateDocumentIdKeyGenerator(*str, &split)) {
    collectionName = string(*str, split);
    auto const length = str.length() - split - 1;
    auto buffer = new char[length + 1];
    memcpy(buffer, *str + split + 1, length);
    buffer[length] = '\0';
    key.reset(buffer);
    return true;
  }

  // document key only
  if (TraditionalKeyGenerator::validateKey(*str)) {
    auto const length = str.length();
    auto buffer = new char[length + 1];
    memcpy(buffer, *str, length);
    buffer[length] = '\0';
    key.reset(buffer);

    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse document or document handle from a v8 value (string | object)
////////////////////////////////////////////////////////////////////////////////

bool ExtractDocumentHandle (v8::Handle<v8::Value> const val,
                            string& collectionName,
                            std::unique_ptr<char[]>& key,
                            TRI_voc_rid_t& rid) {
  // reset the collection identifier and the revision
  collectionName = "";
  rid = 0;

  // extract the document identifier and revision from a string
  if (val->IsString()) {
    return ParseDocumentHandle(val, collectionName, key);
  }

  // extract the document identifier and revision from a document object
  if (val->IsObject()) {
    TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

    v8::Handle<v8::Object> obj = val->ToObject();

    if (obj->Has(v8g->_IdKey)) {
      v8::Handle<v8::Value> didVal = obj->Get(v8g->_IdKey);

      if (! ParseDocumentHandle(didVal, collectionName, key)) {
        return false;
      }
    }
    else if (obj->Has(v8g->_KeyKey)) {
      v8::Handle<v8::Value> didVal = obj->Get(v8g->_KeyKey);

      if (! ParseDocumentHandle(didVal, collectionName, key)) {
        return false;
      }
    }
    else {
      return false;
    }

    if (! obj->Has(v8g->_RevKey)) {
      return true;
    }

    rid = TRI_ObjectToUInt64(obj->Get(v8g->_RevKey), true);

    if (rid == 0) {
      return false;
    }

    return true;
  }

  // unknown value type. give up
  return false;
}
