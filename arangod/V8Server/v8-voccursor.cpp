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

#include "v8-vocbaseprivate.h"
#include "Aql/QueryCursor.h"
#include "Aql/QueryResult.h"
#include "Basics/conversions.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "Transaction/Context.h"
#include "Transaction/V8Context.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-voccursor.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a general cursor from an array
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateCursor(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("CREATE_CURSOR(<data>, <batchSize>, <ttl>)");
  }

  if (!args[0]->IsArray()) {
    TRI_V8_THROW_TYPE_ERROR("<data> must be an array");
  }

  // extract objects
  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(args[0]);
  auto builder = std::make_shared<VPackBuilder>();
  int res = TRI_V8ToVPack(isolate, *builder, array, false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_TYPE_ERROR("cannot convert <array> to JSON");
  }

  // maximum number of results to return at once
  uint32_t batchSize = 1000;

  if (args.Length() >= 2) {
    int64_t maxValue = TRI_ObjectToInt64(args[1]);
    if (maxValue > 0 && maxValue < (int64_t)UINT32_MAX) {
      batchSize = static_cast<uint32_t>(maxValue);
    }
  }

  double ttl = 0.0;
  if (args.Length() >= 3) {
    ttl = TRI_ObjectToDouble(args[2]);
  }

  if (ttl <= 0.0) {
    ttl = 30.0;  // default ttl
  }

  auto* cursors = vocbase.cursorRepository(); // create a cursor
  arangodb::aql::QueryResult result(TRI_ERROR_NO_ERROR);

  result.result = builder;
  result.cached = false;
  result.context = transaction::V8Context::Create(vocbase, false);

  TRI_ASSERT(builder.get() != nullptr);

  try {
    arangodb::Cursor* cursor = cursors->createFromQueryResult(
        std::move(result), static_cast<size_t>(batchSize), ttl, true);

    TRI_ASSERT(cursor != nullptr);
    TRI_DEFER(cursors->release(cursor));

    // need to fetch id before release() as release() might delete the cursor
    CursorId id = cursor->id();

    auto result = TRI_V8UInt64String<TRI_voc_tick_t>(isolate, id);
    TRI_V8_RETURN(result);
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a JSON object from the specified cursor
////////////////////////////////////////////////////////////////////////////////

static void JS_JsonCursor(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("JSON_CURSOR(<id>)");
  }

  std::string const id = TRI_ObjectToString(args[0]);
  auto cursorId = static_cast<arangodb::CursorId>(
      arangodb::basics::StringUtils::uint64(id));

  // find the cursor
  auto cursors = vocbase.cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  bool busy;
  Cursor* cursor = cursors->find(cursorId, Cursor::CURSOR_VPACK, busy);
  TRI_DEFER(cursors->release(cursor));

  if (cursor == nullptr) {
    if (busy) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_CURSOR_BUSY);
    }

    TRI_V8_THROW_EXCEPTION(TRI_ERROR_CURSOR_NOT_FOUND);
  }

  auto cth = cursor->context()->orderCustomTypeHandler();
  VPackOptions opts = VPackOptions::Defaults;
  opts.customTypeHandler = cth.get();

  VPackBuilder builder(&opts);
  builder.openObject(true); // conversion uses sequential iterator, no indexing
  Result r = cursor->dumpSync(builder);
  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MEMORY(); // for compatibility
  }
  builder.close();

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

// .............................................................................
// cursor v8 class
// .............................................................................

struct V8Cursor {
  static constexpr uint16_t CID = 455656;
  
  V8Cursor(v8::Isolate* isolate,
           v8::Handle<v8::Object> holder,
           std::unique_ptr<aql::QueryStreamCursor> cursor)
  : _isolate(isolate), _cursor(std::move(cursor)),
    _ctx(_cursor->context()) {
    // sanity checks
    TRI_ASSERT(_handle.IsEmpty());
    TRI_ASSERT(holder->InternalFieldCount() > 0);
    TRI_ASSERT(_cursor);
    
    // create a new persistent handle
    holder->SetAlignedPointerInInternalField(0, this);
    _handle.Reset(_isolate, holder);
    _handle.SetWrapperClassId(CID);
    
    // and make it weak, so that we can garbage collect
    _handle.SetWeak(&_handle, weakCallback, v8::WeakCallbackType::kFinalizer);
  }
  
  virtual ~V8Cursor() {
    if (!_handle.IsEmpty()) {
      TRI_ASSERT(_handle.IsNearDeath());
      
      _handle.ClearWeak();
      v8::Local<v8::Object> data =
      v8::Local<v8::Object>::New(_isolate, _handle);
      data->SetInternalField(0, v8::Undefined(_isolate));
      _handle.Reset();
    }
    
    LOG_DEVEL << " ~V8Cursor()";
  }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief unwraps a structure
  //////////////////////////////////////////////////////////////////////////////
  
  static V8Cursor* unwrap(v8::Local<v8::Object> handle) {
    TRI_ASSERT(handle->InternalFieldCount() > 0);
    return static_cast<V8Cursor*>(handle->GetAlignedPointerFromInternalField(0));
  }
  
  aql::QueryStreamCursor* cursor() const {
    return _cursor.get();
  }
  
  /// has more cache
  bool hasMore = true;
  
  VPackSlice dataSlice = VPackSlice::noneSlice();
  /// cached extras which might be attached to the stream
  VPackSlice extraSlice = VPackSlice::noneSlice();
  /// @brief pointer to the current result
  std::unique_ptr<VPackArrayIterator> dataIterator;
  /// vpack options
  VPackOptions const* options() const {
    return _ctx->getVPackOptions();
  }
  
  /// @brief fetch the next batch
  Result fetchData() {
    TRI_ASSERT(hasMore);
    TRI_ASSERT(dataIterator == nullptr);
    
    _tmpResult.clear();
    _tmpResult.openObject();
    Result r = _cursor->dumpSync(_tmpResult);
    if (r.fail()) {
      return r;
    }
    _tmpResult.close();
    
    TRI_ASSERT(_tmpResult.slice().isObject());
    // TODO as an optimization
    for(auto pair : VPackObjectIterator(_tmpResult.slice())) {
      if (pair.key.isEqualString("result")) {
        dataSlice = pair.value;
        dataIterator = std::make_unique<VPackArrayIterator>(dataSlice);
      } else if (pair.key.isEqualString("hasMore")) {
        hasMore = pair.value.getBool();
      } else if (pair.key.isEqualString("extra")) {
        extraSlice = pair.value;
      }
    }
    return Result{};
  }
  
  /// @brief return false on error
  bool maybeFetchBatch(v8::Isolate* isolate) {
    if (dataIterator == nullptr && hasMore) { // fetch more data
      Result r = fetchData();
      if (r.fail()) {
        TRI_CreateErrorObject(isolate, r);
        return false;
      }
    }
    return true;
  }
  
private:
  static void weakCallback(const v8::WeakCallbackInfo<v8::Persistent<v8::Object>>& data) {
    auto isolate = data.GetIsolate();
    auto persistent = data.GetParameter();
    auto myPointer = v8::Local<v8::Object>::New(isolate, *persistent);
    
    TRI_ASSERT(myPointer->InternalFieldCount() > 0);
    V8Cursor* obj = V8Cursor::unwrap(myPointer);
    TRI_ASSERT(obj);
    
    TRI_ASSERT(persistent == &obj->_handle);
    TRI_ASSERT(persistent->IsNearDeath());
    delete obj;
  }
  
  /// @brief persistent handle for V8 object
  v8::Persistent<v8::Object> _handle;
  /// @brief isolate
  v8::Isolate* _isolate;
  
  /// @brief query cursor
  std::unique_ptr<aql::QueryStreamCursor> _cursor;

  /// @brief temporary result object
  VPackBuilder _tmpResult;
  
  /// @brief transaction context to use
  std::shared_ptr<transaction::Context> _ctx;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new streaming cursor from arguments
////////////////////////////////////////////////////////////////////////////////

static void cursor_New(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  
  if (!args.IsConstructCall()) { // if not call as a constructor call it
    TRI_V8_THROW_EXCEPTION_USAGE("only instance-able by constructor");
  }
  
  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("ArangoQueryStreamCursor(<queryString>, <bindVars>, <options>)");
  }
  
  // get the query string
  if (!args[0]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <queryString>");
  }
  std::string const queryString(TRI_ObjectToString(args[0]));

  // bind parameters
  std::shared_ptr<VPackBuilder> bindVars;
  
  if (args.Length() > 1) {
    if (!args[1]->IsUndefined() && !args[1]->IsNull() && !args[1]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting object for <bindVars>");
    }
    if (args[1]->IsObject()) {
      bindVars.reset(new VPackBuilder);
      int res = TRI_V8ToVPack(isolate, *(bindVars.get()), args[1], false);
      
      if (res != TRI_ERROR_NO_ERROR) {
        TRI_V8_THROW_EXCEPTION(res);
      }
    }
  }
  
  // options
  auto options = std::make_shared<VPackBuilder>();
  if (args.Length() > 2) {
    // we have options! yikes!
    if (!args[2]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting object for <options>");
    }
    
    int res = TRI_V8ToVPack(isolate, *options, args[2], false);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  } else {
    VPackObjectBuilder guard(options.get());
  }
  
  TRI_vocbase_t* vocbase = v8g->_vocbase;
  TRI_ASSERT(vocbase != nullptr);
  auto cursor = std::make_unique<aql::QueryStreamCursor>(*vocbase, /*id*/0, queryString,
                                                         std::move(bindVars),
                                                         std::move(options),
                                                         /*batchSize*/1000, /*ttl*/1e10);
  
  // TODO difference between args.Holder() vs args.This
  auto v8cursor = new V8Cursor(isolate, args.Holder(), std::move(cursor));
  TRI_ASSERT(v8cursor->cursor() != nullptr);
  
  // fetch for good measure
  if (!v8cursor->maybeFetchBatch(isolate)) { // sets exception
    return;
  }
  
  TRI_V8_RETURN(args.This());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ucs2Slice
////////////////////////////////////////////////////////////////////////////////

static void cursor_toArray(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  V8Cursor* v8cursor = V8Cursor::unwrap(args.Holder());
  if (v8cursor == nullptr) {
    TRI_V8_RETURN(v8::Undefined(isolate));
  }
  
#warning TODO
  
  
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ucs2Slice
////////////////////////////////////////////////////////////////////////////////

static void cursor_getExtra(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
  V8Cursor* v8cursor = V8Cursor::unwrap(args.Holder());
  if (v8cursor == nullptr) {
    TRI_V8_RETURN(v8::Undefined(isolate));
  }
  
  // we always need to fetch
  if (!v8cursor->maybeFetchBatch(isolate)) { // sets exception
    return;
  }
  
  if (v8cursor->extraSlice.isObject()) {
    TRI_V8_RETURN(TRI_VPackToV8(isolate, v8cursor->extraSlice));
  }
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief query stream cursor has next
////////////////////////////////////////////////////////////////////////////////

static void cursor_hasNext(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  V8Cursor* v8cursor = V8Cursor::unwrap(args.Holder());
  if (v8cursor == nullptr) {
    TRI_V8_RETURN_UNDEFINED();
  }
  
  // we always need to fetch
  if (!v8cursor->maybeFetchBatch(isolate)) { // sets exception
    return;
  }
  
  if (v8cursor->dataIterator != nullptr) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ucs2Slice
////////////////////////////////////////////////////////////////////////////////

static void cursor_next(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
  V8Cursor* v8cursor = V8Cursor::unwrap(args.Holder());
  if (v8cursor == nullptr) {
    TRI_V8_RETURN_UNDEFINED();
  }
  
  // we always need to fetch
  if (!v8cursor->maybeFetchBatch(isolate)) { // sets exception
    return;
  }
  
  if (v8cursor->dataIterator) { // got a current batch
    TRI_ASSERT(v8cursor->dataIterator->valid());
    
    VPackSlice s = v8cursor->dataIterator->value();
    v8::Local<v8::Value> val = TRI_VPackToV8(isolate, s, v8cursor->options());
    
    ++(*v8cursor->dataIterator);
    // reset so that the next one can fetch again
    if (!v8cursor->dataIterator->valid()) {
      v8cursor->dataIterator.reset();
    }
    TRI_V8_RETURN(val);
  }
  
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ucs2Slice
////////////////////////////////////////////////////////////////////////////////

static void cursor_count(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
  V8Cursor* parent = V8Cursor::unwrap(args.Holder());
  if (parent == nullptr) {
    TRI_V8_RETURN(v8::Undefined(isolate));
  }
  
  TRI_V8_RETURN_UNDEFINED(); // always undefined
  TRI_V8_TRY_CATCH_END
}

// .............................................................................
// generate the general cursor template
// .............................................................................

void TRI_InitV8cursor(v8::Handle<v8::Context> context, TRI_v8_global_t* v8g) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  // cursor functions. not intended to be used by end users
  // these cursor functions are the APIs implemented in js/actions/api-simple.js
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "CREATE_CURSOR"),
                               JS_CreateCursor, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "JSON_CURSOR"),
                               JS_JsonCursor, true);
  
  // actual streaming query cursor object
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;
  
  ft = v8::FunctionTemplate::New(isolate, cursor_New);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoQueryStreamCursor"));
  
  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(1);
  
  /*TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "isArangoResultSet"),
                       cursor_isArangoResultSet);*/
  ft->PrototypeTemplate()->Set(TRI_V8_ASCII_STRING(isolate, "isArangoResultSet"),
                               v8::True(isolate));
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "toArray"),
                       cursor_toArray);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "getExtra"),
                       cursor_getExtra);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "hasNext"),
                        cursor_hasNext);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "next"),
                        cursor_next);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "count"),
                        cursor_count);
  
  v8g->StreamQueryCursorTempl.Reset(isolate, ft);
  v8::MaybeLocal<v8::Function> ctor = ft->GetFunction(context);
  if (ctor.IsEmpty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "error creating v8 stream cursor");
  }
  //ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoStreamQueryCursorCtor"));
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "ArangoQueryStreamCursor"),
                               ctor.ToLocalChecked(), true);
  
}
