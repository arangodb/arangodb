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
#include "Basics/conversions.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "Utils/StandaloneTransactionContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-voccursor.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a general cursor from an array
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateCursor(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

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

  // create a cursor
  auto cursors = vocbase->cursorRepository();

  arangodb::aql::QueryResult result(TRI_ERROR_NO_ERROR);
  result.result = builder;
  result.cached = false;
  result.context = std::make_shared<arangodb::StandaloneTransactionContext>(vocbase);

  TRI_ASSERT(builder.get() != nullptr);

  try {
    arangodb::Cursor* cursor = cursors->createFromQueryResult(
        std::move(result), static_cast<size_t>(batchSize), nullptr, ttl, true);

    TRI_ASSERT(cursor != nullptr);
    cursors->release(cursor);

    auto result = V8TickId(isolate, cursor->id());
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

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("JSON_CURSOR(<id>)");
  }

  std::string const id = TRI_ObjectToString(args[0]);
  auto cursorId = static_cast<arangodb::CursorId>(
      arangodb::basics::StringUtils::uint64(id));

  // find the cursor
  auto cursors = vocbase->cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  bool busy;
  auto cursor = cursors->find(cursorId, busy);

  if (cursor == nullptr) {
    if (busy) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_CURSOR_BUSY);
    }

    TRI_V8_THROW_EXCEPTION(TRI_ERROR_CURSOR_NOT_FOUND);
  }

  try {
    auto result = v8::Object::New(isolate);

    // build documents
    auto docs = v8::Array::New(isolate);
    size_t const n = cursor->batchSize();

    for (size_t i = 0; i < n; ++i) {
      if (!cursor->hasNext()) {
        break;
      }

      auto row = cursor->next();

      if (row.isNone()) {
        TRI_V8_THROW_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      docs->Set(static_cast<uint32_t>(i), TRI_VPackToV8(isolate, row));
    }

    result->ForceSet(TRI_V8_ASCII_STRING("result"), docs);

    bool hasCount = cursor->hasCount();
    size_t count = cursor->count();
    bool hasNext = cursor->hasNext();
    VPackSlice const extra = cursor->extra();

    result->ForceSet(TRI_V8_ASCII_STRING("hasMore"),
                     v8::Boolean::New(isolate, hasNext));

    if (hasNext) {
      result->ForceSet(TRI_V8_ASCII_STRING("id"),
                       V8TickId(isolate, cursor->id()));
    }

    if (hasCount) {
      result->ForceSet(TRI_V8_ASCII_STRING("count"),
                       v8::Number::New(isolate, static_cast<double>(count)));
    }
    if (!extra.isNone()) {
      result->ForceSet(TRI_V8_ASCII_STRING("extra"),
                       TRI_VPackToV8(isolate, extra));
    }

    cursors->release(cursor);

    TRI_V8_RETURN(result);
  } catch (...) {
    cursors->release(cursor);
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  TRI_V8_TRY_CATCH_END
}

// .............................................................................
// generate the general cursor template
// .............................................................................

void TRI_InitV8cursor(v8::Handle<v8::Context> context, TRI_v8_global_t* v8g) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  // cursor functions. not intended to be used by end users
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("CREATE_CURSOR"),
                               JS_CreateCursor, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("JSON_CURSOR"),
                               JS_JsonCursor, true);
}
