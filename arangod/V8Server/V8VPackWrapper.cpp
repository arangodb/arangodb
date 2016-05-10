////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "V8VPackWrapper.h"
#include "Basics/conversions.h"
#include "Logger/Logger.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Transaction.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "VocBase/datafile.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/document-collection.h"
#include "VocBase/KeyGenerator.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for vpack
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
/// - SLOT_DITCH
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_VPACK_TYPE = 8;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "ditch"
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_DITCH = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief return offset of VPack from a marker
////////////////////////////////////////////////////////////////////////////////

static inline VPackSlice VPackFromMarker(TRI_df_marker_t const* marker) {
  uint8_t const* ptr = reinterpret_cast<uint8_t const*>(marker) + DatafileHelper::VPackOffset(marker->getType());
  return VPackSlice(ptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add the _id attribute to an object
////////////////////////////////////////////////////////////////////////////////
  
static void AddCollectionId(v8::Isolate* isolate, v8::Handle<v8::Object> self,
                            arangodb::Transaction* trx, TRI_df_marker_t const* marker) {
  char buffer[TRI_COL_NAME_LENGTH + TRI_VOC_KEY_MAX_LENGTH + 2];

  VPackSlice slice = VPackFromMarker(marker);
 
  // extract cid from marker 
  VPackSlice id = slice.get(TRI_VOC_ATTRIBUTE_ID);
  uint64_t cid = DatafileHelper::ReadNumber<uint64_t>(id.begin() + 1, sizeof(uint64_t));

  VPackValueLength keyLength;
  char const* key = slice.get(TRI_VOC_ATTRIBUTE_KEY).getString(keyLength); 
  TRI_ASSERT(key != nullptr);

  size_t len = trx->resolver()->getCollectionName(buffer, cid);
  buffer[len] = '/';
  memcpy(buffer + len + 1,  key, static_cast<size_t>(keyLength));
  
  self->ForceSet(TRI_V8_STRING(TRI_VOC_ATTRIBUTE_ID), 
                 TRI_V8_PAIR_STRING(buffer, (int)(len + keyLength + 1)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for a ditch
////////////////////////////////////////////////////////////////////////////////

static void WeakDocumentDitchCallback(v8::WeakCallbackData<
    v8::External, v8::Persistent<v8::External>> const& data) {
  auto isolate = data.GetIsolate();
  auto persistent = data.GetParameter();
  auto myDitch = v8::Local<v8::External>::New(isolate, *persistent);

  auto ditch = static_cast<arangodb::DocumentDitch*>(myDitch->Value());
  TRI_ASSERT(ditch != nullptr);

  TRI_GET_GLOBALS();

  v8g->decreaseActiveExternals();

  LOG(TRACE) << "weak-callback for document ditch called";

  // find the persistent handle
  v8g->JSVPack[ditch].Reset();
  v8g->JSVPack.erase(ditch);

  // get the vocbase pointer from the ditch
  TRI_vocbase_t* vocbase = ditch->collection()->_vocbase;

  ditch->ditches()->freeDocumentDitch(ditch, false /* fromTransaction */);
  // we don't need the ditch anymore, maybe a transaction is still using it

  if (vocbase != nullptr) {
    // decrease the reference-counter for the database
    TRI_ReleaseVocBase(vocbase);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy all vpack attributes into the object so we have a regular
/// JavaScript object that can be modified later
////////////////////////////////////////////////////////////////////////////////

static void CopyAttributes(v8::Isolate* isolate, v8::Handle<v8::Object> self,
                           TRI_df_marker_t const* marker,
                           char const* excludeAttribute = nullptr) {

  auto slice = VPackFromMarker(marker);

  VPackObjectIterator it(slice);
  while (it.valid()) {
    std::string key(it.key().copyString());

    if (excludeAttribute == nullptr || key != excludeAttribute) {
      self->ForceSet(TRI_V8_STD_STRING(key), TRI_VPackToV8(isolate, it.value()));
    }
    it.next();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects the keys from the vpack
////////////////////////////////////////////////////////////////////////////////

static void KeysOfVPack(v8::PropertyCallbackInfo<v8::Array> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // sanity check
  v8::Handle<v8::Object> self = args.Holder();

  if (self->InternalFieldCount() <= SLOT_DITCH) {
    TRI_V8_RETURN(v8::Array::New(isolate));
  }

  // get vpack
  auto marker = TRI_UnwrapClass<TRI_df_marker_t const>(self, WRP_VPACK_TYPE);

  if (marker == nullptr) {
    TRI_V8_RETURN(v8::Array::New(isolate));
  }
    
  auto slice = VPackFromMarker(marker);
  std::vector<std::string> keys(VPackCollection::keys(slice));

  v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(keys.size()));
  uint32_t count = 0;
  for (auto& it : keys) {
    result->Set(count++, TRI_V8_STD_STRING(it));
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a named attribute from the vpack
////////////////////////////////////////////////////////////////////////////////

static void MapGetNamedVPack(
    v8::Local<v8::String> name,
    v8::PropertyCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  try {
    v8::HandleScope scope(isolate);

    // sanity check
    v8::Handle<v8::Object> self = args.Holder();

    if (self->InternalFieldCount() <= SLOT_DITCH) {
      // we better not throw here... otherwise this will cause a segfault
      TRI_V8_RETURN(v8::Handle<v8::Value>());
    }

    // get vpack
    auto marker = TRI_UnwrapClass<TRI_df_marker_t const>(self, WRP_VPACK_TYPE);

    if (marker == nullptr) {
      TRI_V8_RETURN(v8::Handle<v8::Value>());
    }

    // convert the JavaScript string to a string
    // we take the fast path here and don't normalize the string
    v8::String::Utf8Value const str(name);
    std::string const key(*str, (size_t)str.length());

    if (key.empty()) {
      TRI_V8_RETURN(v8::Handle<v8::Value>());
    }
    
    if (key == TRI_VOC_ATTRIBUTE_ID) {
      TRI_V8_RETURN(v8::Handle<v8::Value>());
    }

    auto slice = VPackFromMarker(marker);

    TRI_V8_RETURN(TRI_VPackToV8(isolate, slice.get(key)));
  } catch (...) {
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a named attribute in the vpack
/// Returns the value if the setter intercepts the request.
/// Otherwise, returns an empty handle.
////////////////////////////////////////////////////////////////////////////////

static void MapSetNamedVPack(
    v8::Local<v8::String> name, v8::Local<v8::Value> value,
    v8::PropertyCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  try {
    v8::HandleScope scope(isolate);

    // sanity check
    v8::Handle<v8::Object> self = args.Holder();

    if (self->InternalFieldCount() <= SLOT_DITCH) {
      // we better not throw here... otherwise this will cause a segfault
      TRI_V8_RETURN(v8::Handle<v8::Value>());
    }

    // get vpack
    auto marker = TRI_UnwrapClass<TRI_df_marker_t const>(self, WRP_VPACK_TYPE);

    if (marker == nullptr) {
      return;
    }

    if (self->HasRealNamedProperty(name)) {
      // object already has the property. use the regular property setter
      self->ForceSet(name, value);
      TRI_V8_RETURN_TRUE();
    }

    // copy all attributes from the vpack into the object
    CopyAttributes(isolate, self, marker, nullptr);

    // remove pointer to marker, so the object becomes stand-alone
    self->SetInternalField(SLOT_CLASS, v8::External::New(isolate, nullptr));

    // and now use the regular property setter
    self->ForceSet(name, value);
    TRI_V8_RETURN_TRUE();
  } catch (...) {
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a named attribute from the vpack
/// Returns a non-empty handle if the deleter intercepts the request.
/// The return value is true if the property could be deleted and false
/// otherwise.
////////////////////////////////////////////////////////////////////////////////

static void MapDeleteNamedVPack(
    v8::Local<v8::String> name,
    v8::PropertyCallbackInfo<v8::Boolean> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  try {
    v8::HandleScope scope(isolate);

    // sanity check
    v8::Handle<v8::Object> self = args.Holder();

    if (self->InternalFieldCount() <= SLOT_DITCH) {
      // we better not throw here... otherwise this will cause a segfault
      return;
    }

    // get vpack
    auto marker = TRI_UnwrapClass<TRI_df_marker_t const>(self, WRP_VPACK_TYPE);

    if (marker == nullptr) {
      TRI_V8_RETURN(v8::Handle<v8::Boolean>());
    }

    // remove pointer to marker, so the object becomes stand-alone
    self->SetInternalField(SLOT_CLASS, v8::External::New(isolate, nullptr));

    // copy all attributes from the vpack into the object
    // but the to-be-deleted attribute
    std::string nameString(TRI_ObjectToString(name));
    CopyAttributes(isolate, self, marker, nameString.c_str());

    TRI_V8_RETURN(v8::Handle<v8::Boolean>());
  } catch (...) {
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a property is present
////////////////////////////////////////////////////////////////////////////////

static void PropertyQueryVPack(
    v8::Local<v8::String> name,
    v8::PropertyCallbackInfo<v8::Integer> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  try {
    v8::HandleScope scope(isolate);

    v8::Handle<v8::Object> self = args.Holder();

    // sanity check
    if (self->InternalFieldCount() <= SLOT_DITCH) {
      TRI_V8_RETURN(v8::Handle<v8::Integer>());
    }

    // get vpack
    auto marker =
        TRI_UnwrapClass<TRI_df_marker_t const>(self, WRP_VPACK_TYPE);

    if (marker == nullptr) {
      TRI_V8_RETURN(v8::Handle<v8::Integer>());
    }

    // convert the JavaScript string to a string
    std::string key(TRI_ObjectToString(name));

    if (key.empty()) {
      TRI_V8_RETURN(v8::Handle<v8::Integer>());
    }

    auto slice = VPackFromMarker(marker);

    if (!slice.hasKey(key)) {
      // key not found
      TRI_V8_RETURN(v8::Handle<v8::Integer>());
    }

    // key found
    TRI_V8_RETURN(v8::Handle<v8::Integer>(v8::Integer::New(isolate, v8::None)));
  } catch (...) {
    TRI_V8_RETURN(v8::Handle<v8::Integer>());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects an indexed attribute from the vpack
////////////////////////////////////////////////////////////////////////////////

static void MapGetIndexedVPack(
    uint32_t idx, v8::PropertyCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  char buffer[11];
  size_t len = TRI_StringUInt32InPlace(idx, &buffer[0]);

  v8::Local<v8::String> strVal = TRI_V8_PAIR_STRING(&buffer[0], (int)len);

  MapGetNamedVPack(strVal, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an indexed attribute in the vpack
////////////////////////////////////////////////////////////////////////////////

static void MapSetIndexedVPack(
    uint32_t idx, v8::Local<v8::Value> value,
    v8::PropertyCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  char buffer[11];
  size_t len = TRI_StringUInt32InPlace(idx, &buffer[0]);

  v8::Local<v8::String> strVal = TRI_V8_PAIR_STRING(&buffer[0], (int)len);

  MapSetNamedVPack(strVal, value, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete an indexed attribute in the vpack
////////////////////////////////////////////////////////////////////////////////

static void MapDeleteIndexedVPack(
    uint32_t idx, v8::PropertyCallbackInfo<v8::Boolean> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  char buffer[11];
  size_t len = TRI_StringUInt32InPlace(idx, &buffer[0]);

  v8::Local<v8::String> strVal = TRI_V8_PAIR_STRING(&buffer[0], (int)len);

  MapDeleteNamedVPack(strVal, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a VPackSlice
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8VPackWrapper::wrap(v8::Isolate* isolate, arangodb::Transaction* trx,
                                           TRI_voc_cid_t cid, arangodb::DocumentDitch* ditch,
                                           TRI_doc_mptr_t const* mptr) {
  TRI_ASSERT(mptr != nullptr);
  TRI_ASSERT(ditch != nullptr);

  bool const doCopy = mptr->pointsToWal();
  auto marker = mptr->getMarkerPtr();

  if (doCopy) {
    // we'll create a full copy of the slice
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
 
    CopyAttributes(isolate, result, marker, nullptr);
    
    // copy value of _id
    AddCollectionId(isolate, result, trx, marker);
    
    return result;
  }

  // we'll create a document stub, with a pointer into the datafile

  // create the new handle to return, and set its template type
  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL(VPackTempl, v8::ObjectTemplate);
  v8::Handle<v8::Object> result = VPackTempl->NewInstance();

  if (result.IsEmpty()) {
    // error
    return result;
  }
  
  // point the 0 index Field to the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE,
                           v8::Integer::New(isolate, WRP_VPACK_TYPE));
  result->SetInternalField(
      SLOT_CLASS,
      v8::External::New(isolate,
                        const_cast<void*>(static_cast<void const*>(marker))));
  
  TRI_ASSERT(ditch != nullptr);

  auto it = v8g->JSVPack.find(ditch);

  if (it == v8g->JSVPack.end()) {
    // tell everyone else that this ditch is used by an external
    ditch->setUsedByExternal();

    // increase the reference-counter for the database
    TRI_ASSERT(ditch->collection() != nullptr);
    TRI_UseVocBase(ditch->collection()->_vocbase);
    auto externalDitch = v8::External::New(isolate, ditch);
    v8::Persistent<v8::External>& per(v8g->JSVPack[ditch]);
    per.Reset(isolate, externalDitch);
    result->SetInternalField(SLOT_DITCH, externalDitch);
    per.SetWeak(&per, WeakDocumentDitchCallback);
    v8g->increaseActiveExternals();
  } else {
    auto myDitch = v8::Local<v8::External>::New(isolate, it->second);

    result->SetInternalField(SLOT_DITCH, myDitch);
  }
    
  AddCollectionId(isolate, result, trx, marker);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate the VPack object template
////////////////////////////////////////////////////////////////////////////////

void V8VPackWrapper::initialize(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                                TRI_v8_global_t* v8g) {
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("VPack"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);

  // accessor for named properties (e.g. doc.abcdef)
  rt->SetNamedPropertyHandler(
      MapGetNamedVPack,     // NamedPropertyGetter,
      MapSetNamedVPack,     // NamedPropertySetter setter
      PropertyQueryVPack,   // NamedPropertyQuery,
      MapDeleteNamedVPack,  // NamedPropertyDeleter deleter,
      KeysOfVPack           // NamedPropertyEnumerator,
                            // Handle<Value> data = Handle<Value>());
      );

  // accessor for indexed properties (e.g. doc[1])
  rt->SetIndexedPropertyHandler(
      MapGetIndexedVPack,     // IndexedPropertyGetter,
      MapSetIndexedVPack,     // IndexedPropertySetter,
      nullptr,                // IndexedPropertyQuery,
      MapDeleteIndexedVPack,  // IndexedPropertyDeleter,
      nullptr                 // IndexedPropertyEnumerator,
                              // Handle<Value> data = Handle<Value>());
      );

  v8g->VPackTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("VPack"), ft->GetFunction());
 
  {
    // add legacy ShapedJson object 
    v8::Handle<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(isolate);
    ft->SetClassName(TRI_V8_ASCII_STRING("ShapedJson"));
    TRI_AddGlobalFunctionVocbase(
        isolate, context, TRI_V8_ASCII_STRING("ShapedJson"), ft->GetFunction());
  }
}

