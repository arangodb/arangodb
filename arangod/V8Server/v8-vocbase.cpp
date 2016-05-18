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

#include <unicode/timezone.h>
#include <unicode/smpdtfmt.h>
#include <unicode/dtfmtsym.h>

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <v8.h>
#include <iostream>

#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "Aql/Query.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Aql/QueryRegistry.h"
#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/Timers.h"
#include "Basics/Utf8Helper.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Rest/Version.h"
#include "RestServer/ConsoleThread.h"
#include "RestServer/RestServerFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Statistics/StatisticsFeature.h"
#include "Utils/ExplicitTransaction.h"
#include "Utils/V8TransactionContext.h"
#include "V8/JSLoader.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/V8Traverser.h"
#include "V8Server/V8VPackWrapper.h"
#include "V8Server/v8-collection.h"
#include "V8Server/v8-replication.h"
#include "V8Server/v8-statistics.h"
#include "V8Server/v8-voccursor.h"
#include "V8Server/v8-vocindex.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/auth.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::traverser;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_t
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
////////////////////////////////////////////////////////////////////////////////

int32_t const WRP_VOCBASE_TYPE = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_col_t
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
/// - SLOT_COLLECTION
////////////////////////////////////////////////////////////////////////////////

int32_t const WRP_VOCBASE_COL_TYPE = 2;

struct CollectionDitchInfo {
  arangodb::DocumentDitch* ditch;
  TRI_document_collection_t* col;

  CollectionDitchInfo(arangodb::DocumentDitch* ditch,
                      TRI_document_collection_t* col)
      : ditch(ditch), col(col) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a C++ into a v8::Object
////////////////////////////////////////////////////////////////////////////////

template <class T>
static v8::Handle<v8::Object> WrapClass(
    v8::Isolate* isolate, v8::Persistent<v8::ObjectTemplate>& classTempl,
    int32_t type, T* y) {
  v8::EscapableHandleScope scope(isolate);

  auto localClassTemplate =
      v8::Local<v8::ObjectTemplate>::New(isolate, classTempl);
  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = localClassTemplate->NewInstance();

  if (result.IsEmpty()) {
    // error
    return scope.Escape<v8::Object>(result);
  }

  // set the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(isolate, type));
  result->SetInternalField(SLOT_CLASS, v8::External::New(isolate, y));

  return scope.Escape<v8::Object>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a transaction
////////////////////////////////////////////////////////////////////////////////

static void JS_Transaction(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("TRANSACTION(<object>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // treat the argument as an object from now on
  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);

  // extract the properties from the object

  // "lockTimeout"
  double lockTimeout =
      (double)(TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);

  if (object->Has(TRI_V8_ASCII_STRING("lockTimeout"))) {
    static std::string const timeoutError =
        "<lockTimeout> must be a valid numeric value";

    if (!object->Get(TRI_V8_ASCII_STRING("lockTimeout"))->IsNumber()) {
      TRI_V8_THROW_EXCEPTION_PARAMETER(timeoutError);
    }

    lockTimeout =
        TRI_ObjectToDouble(object->Get(TRI_V8_ASCII_STRING("lockTimeout")));

    if (lockTimeout < 0.0) {
      TRI_V8_THROW_EXCEPTION_PARAMETER(timeoutError);
    }
  }

  // "waitForSync"
  bool waitForSync = false;

  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL_STRING(WaitForSyncKey);
  if (object->Has(WaitForSyncKey)) {
    if (!object->Get(WaitForSyncKey)->IsBoolean() &&
        !object->Get(WaitForSyncKey)->IsBooleanObject()) {
      TRI_V8_THROW_EXCEPTION_PARAMETER("<waitForSync> must be a boolean value");
    }

    waitForSync = TRI_ObjectToBoolean(WaitForSyncKey);
  }

  // "collections"
  static std::string const collectionError =
      "missing/invalid collections definition for transaction";

  if (!object->Has(TRI_V8_ASCII_STRING("collections")) ||
      !object->Get(TRI_V8_ASCII_STRING("collections"))->IsObject()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(collectionError);
  }

  // extract collections
  v8::Handle<v8::Object> collections = v8::Handle<v8::Object>::Cast(
      object->Get(TRI_V8_ASCII_STRING("collections")));

  if (collections.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(collectionError);
  }

  bool isValid = true;
  std::vector<std::string> readCollections;
  std::vector<std::string> writeCollections;

  bool allowImplicitCollections = true;
  if (collections->Has(TRI_V8_ASCII_STRING("allowImplicit"))) {
    allowImplicitCollections = TRI_ObjectToBoolean(
        collections->Get(TRI_V8_ASCII_STRING("allowImplicit")));
  }

  // collections.read
  if (collections->Has(TRI_V8_ASCII_STRING("read"))) {
    if (collections->Get(TRI_V8_ASCII_STRING("read"))->IsArray()) {
      v8::Handle<v8::Array> names = v8::Handle<v8::Array>::Cast(
          collections->Get(TRI_V8_ASCII_STRING("read")));

      for (uint32_t i = 0; i < names->Length(); ++i) {
        v8::Handle<v8::Value> collection = names->Get(i);
        if (!collection->IsString()) {
          isValid = false;
          break;
        }

        readCollections.emplace_back(TRI_ObjectToString(collection));
      }
    } else if (collections->Get(TRI_V8_ASCII_STRING("read"))->IsString()) {
      readCollections.emplace_back(
          TRI_ObjectToString(collections->Get(TRI_V8_ASCII_STRING("read"))));
    } else {
      isValid = false;
    }
  }

  // collections.write
  if (collections->Has(TRI_V8_ASCII_STRING("write"))) {
    if (collections->Get(TRI_V8_ASCII_STRING("write"))->IsArray()) {
      v8::Handle<v8::Array> names = v8::Handle<v8::Array>::Cast(
          collections->Get(TRI_V8_ASCII_STRING("write")));

      for (uint32_t i = 0; i < names->Length(); ++i) {
        v8::Handle<v8::Value> collection = names->Get(i);
        if (!collection->IsString()) {
          isValid = false;
          break;
        }

        writeCollections.emplace_back(TRI_ObjectToString(collection));
      }
    } else if (collections->Get(TRI_V8_ASCII_STRING("write"))->IsString()) {
      writeCollections.emplace_back(
          TRI_ObjectToString(collections->Get(TRI_V8_ASCII_STRING("write"))));
    } else {
      isValid = false;
    }
  }

  if (!isValid) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(collectionError);
  }

  // extract the "action" property
  static std::string const actionErrorPrototype =
      "missing/invalid action definition for transaction";
  std::string actionError = actionErrorPrototype;

  if (!object->Has(TRI_V8_ASCII_STRING("action"))) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(actionError);
  }

  // function parameters
  v8::Handle<v8::Value> params;

  if (object->Has(TRI_V8_ASCII_STRING("params"))) {
    params =
        v8::Handle<v8::Array>::Cast(object->Get(TRI_V8_ASCII_STRING("params")));
  } else {
    params = v8::Undefined(isolate);
  }

  if (params.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  bool embed = false;
  if (object->Has(TRI_V8_ASCII_STRING("embed"))) {
    v8::Handle<v8::Value> v =
        v8::Handle<v8::Object>::Cast(object->Get(TRI_V8_ASCII_STRING("embed")));
    embed = TRI_ObjectToBoolean(v);
  }

  v8::Handle<v8::Object> current = isolate->GetCurrentContext()->Global();

  // callback function
  v8::Handle<v8::Function> action;

  if (object->Get(TRI_V8_ASCII_STRING("action"))->IsFunction()) {
    action = v8::Handle<v8::Function>::Cast(
        object->Get(TRI_V8_ASCII_STRING("action")));
  } else if (object->Get(TRI_V8_ASCII_STRING("action"))->IsString()) {
    v8::TryCatch tryCatch;
    // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
    v8::Local<v8::Function> ctor = v8::Local<v8::Function>::Cast(
        current->Get(TRI_V8_ASCII_STRING("Function")));

    // Invoke Function constructor to create function with the given body and no
    // arguments
    std::string body = TRI_ObjectToString(
        object->Get(TRI_V8_ASCII_STRING("action"))->ToString());
    body = "return (" + body + ")(params);";
    v8::Handle<v8::Value> args[2] = {TRI_V8_ASCII_STRING("params"),
                                     TRI_V8_STD_STRING(body)};
    v8::Local<v8::Object> function = ctor->NewInstance(2, args);

    action = v8::Local<v8::Function>::Cast(function);
    if (tryCatch.HasCaught()) {
      actionError += " - ";
      actionError += *v8::String::Utf8Value(tryCatch.Message()->Get());
      actionError += " - ";
      actionError += *v8::String::Utf8Value(tryCatch.StackTrace());

      TRI_CreateErrorObject(isolate, TRI_ERROR_BAD_PARAMETER, actionError);
      tryCatch.ReThrow();
      return;
    }
  } else {
    TRI_V8_THROW_EXCEPTION_PARAMETER(actionError);
  }

  if (action.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(actionError);
  }

  auto transactionContext =
      std::make_shared<V8TransactionContext>(vocbase, embed);

  // start actual transaction
  ExplicitTransaction trx(transactionContext, readCollections, writeCollections,
                          lockTimeout, waitForSync, allowImplicitCollections);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Value> result;
  try {
    v8::TryCatch tryCatch;
    v8::Handle<v8::Value> arguments = params;
    result = action->Call(current, 1, &arguments);

    if (tryCatch.HasCaught()) {
      trx.abort();

      if (tryCatch.CanContinue()) {
        tryCatch.ReThrow();
        return;
      } else {
        v8g->_canceled = true;
        TRI_V8_RETURN(result);
      }
    }
  } catch (arangodb::basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::bad_alloc const&) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  res = trx.commit();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock walPropertiesGet
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock walPropertiesSet
////////////////////////////////////////////////////////////////////////////////

static void JS_PropertiesWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 1 || (args.Length() == 1 && !args[0]->IsObject())) {
    TRI_V8_THROW_EXCEPTION_USAGE("properties(<object>)");
  }

  auto l = arangodb::wal::LogfileManager::instance();

  if (args.Length() == 1) {
    // set the properties
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);
    if (object->Has(TRI_V8_ASCII_STRING("allowOversizeEntries"))) {
      bool value = TRI_ObjectToBoolean(
          object->Get(TRI_V8_ASCII_STRING("allowOversizeEntries")));
      l->allowOversizeEntries(value);
    }

    if (object->Has(TRI_V8_ASCII_STRING("logfileSize"))) {
      uint32_t value = static_cast<uint32_t>(TRI_ObjectToUInt64(
          object->Get(TRI_V8_ASCII_STRING("logfileSize")), true));
      l->filesize(value);
    }

    if (object->Has(TRI_V8_ASCII_STRING("historicLogfiles"))) {
      uint32_t value = static_cast<uint32_t>(TRI_ObjectToUInt64(
          object->Get(TRI_V8_ASCII_STRING("historicLogfiles")), true));
      l->historicLogfiles(value);
    }

    if (object->Has(TRI_V8_ASCII_STRING("reserveLogfiles"))) {
      uint32_t value = static_cast<uint32_t>(TRI_ObjectToUInt64(
          object->Get(TRI_V8_ASCII_STRING("reserveLogfiles")), true));
      l->reserveLogfiles(value);
    }

    if (object->Has(TRI_V8_ASCII_STRING("throttleWait"))) {
      uint64_t value = TRI_ObjectToUInt64(
          object->Get(TRI_V8_ASCII_STRING("throttleWait")), true);
      l->maxThrottleWait(value);
    }

    if (object->Has(TRI_V8_ASCII_STRING("throttleWhenPending"))) {
      uint64_t value = TRI_ObjectToUInt64(
          object->Get(TRI_V8_ASCII_STRING("throttleWhenPending")), true);
      l->throttleWhenPending(value);
    }
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("allowOversizeEntries"),
              v8::Boolean::New(isolate, l->allowOversizeEntries()));
  result->Set(TRI_V8_ASCII_STRING("logfileSize"),
              v8::Number::New(isolate, l->filesize()));
  result->Set(TRI_V8_ASCII_STRING("historicLogfiles"),
              v8::Number::New(isolate, l->historicLogfiles()));
  result->Set(TRI_V8_ASCII_STRING("reserveLogfiles"),
              v8::Number::New(isolate, l->reserveLogfiles()));
  result->Set(TRI_V8_ASCII_STRING("syncInterval"),
              v8::Number::New(isolate, (double)l->syncInterval()));
  result->Set(TRI_V8_ASCII_STRING("throttleWait"),
              v8::Number::New(isolate, (double)l->maxThrottleWait()));
  result->Set(TRI_V8_ASCII_STRING("throttleWhenPending"),
              v8::Number::New(isolate, (double)l->throttleWhenPending()));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock walFlush
////////////////////////////////////////////////////////////////////////////////

static void JS_FlushWal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  bool waitForSync = false;
  bool waitForCollector = false;
  bool writeShutdownFile = false;

  if (args.Length() > 0) {
    if (args[0]->IsObject()) {
      v8::Handle<v8::Object> obj = args[0]->ToObject();
      if (obj->Has(TRI_V8_ASCII_STRING("waitForSync"))) {
        waitForSync =
            TRI_ObjectToBoolean(obj->Get(TRI_V8_ASCII_STRING("waitForSync")));
      }
      if (obj->Has(TRI_V8_ASCII_STRING("waitForCollector"))) {
        waitForCollector = TRI_ObjectToBoolean(
            obj->Get(TRI_V8_ASCII_STRING("waitForCollector")));
      }
      if (obj->Has(TRI_V8_ASCII_STRING("writeShutdownFile"))) {
        writeShutdownFile = TRI_ObjectToBoolean(
            obj->Get(TRI_V8_ASCII_STRING("writeShutdownFile")));
      }
    } else {
      waitForSync = TRI_ObjectToBoolean(args[0]);

      if (args.Length() > 1) {
        waitForCollector = TRI_ObjectToBoolean(args[1]);

        if (args.Length() > 2) {
          writeShutdownFile = TRI_ObjectToBoolean(args[2]);
        }
      }
    }
  }

  int res;

  if (ServerState::instance()->isCoordinator()) {
    res = flushWalOnAllDBServers(waitForSync, waitForCollector);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
    TRI_V8_RETURN_TRUE();
  }

  res = arangodb::wal::LogfileManager::instance()->flush(
      waitForSync, waitForCollector, writeShutdownFile);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for WAL collector to finish operations for the specified
/// collection
////////////////////////////////////////////////////////////////////////////////

static void JS_WaitCollectorWal(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "WAL_WAITCOLLECTOR(<collection-id>, <timeout>)");
  }

  std::string const name = TRI_ObjectToString(args[0]);

  TRI_vocbase_col_t* col =
      TRI_LookupCollectionByNameVocBase(vocbase, name);

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  double timeout = 30.0;
  if (args.Length() > 1) {
    timeout = TRI_ObjectToDouble(args[1]);
  }

  int res = arangodb::wal::LogfileManager::instance()->waitForCollectorQueue(
      col->_cid, timeout);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get information about the currently running transactions
////////////////////////////////////////////////////////////////////////////////

static void JS_TransactionsWal(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto const& info =
      arangodb::wal::LogfileManager::instance()->runningTransactions();

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->ForceSet(
      TRI_V8_ASCII_STRING("runningTransactions"),
      v8::Number::New(isolate, static_cast<double>(std::get<0>(info))));

  // lastCollectedId
  {
    auto value = std::get<1>(info);
    if (value == UINT64_MAX) {
      result->ForceSet(TRI_V8_ASCII_STRING("minLastCollected"),
                       v8::Null(isolate));
    } else {
      result->ForceSet(TRI_V8_ASCII_STRING("minLastCollected"),
                       V8TickId(isolate, static_cast<TRI_voc_tick_t>(value)));
    }
  }

  // lastSealedId
  {
    auto value = std::get<2>(info);
    if (value == UINT64_MAX) {
      result->ForceSet(TRI_V8_ASCII_STRING("minLastSealed"), v8::Null(isolate));
    } else {
      result->ForceSet(TRI_V8_ASCII_STRING("minLastSealed"),
                       V8TickId(isolate, static_cast<TRI_voc_tick_t>(value)));
    }
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_NormalizeString(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("NORMALIZE_STRING(<string>)");
  }

  TRI_normalize_V8_Obj(args, args[0]);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enables or disables native backtrace
////////////////////////////////////////////////////////////////////////////////

static void JS_EnableNativeBacktraces(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("ENABLE_NATIVE_BACKTRACES(<value>)");
  }

  arangodb::basics::Exception::SetVerbose(TRI_ObjectToBoolean(args[0]));

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

extern V8LineEditor* theConsole;

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a debugging console
////////////////////////////////////////////////////////////////////////////////

static void JS_Debug(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  v8::Local<v8::String> name(TRI_V8_ASCII_STRING("debug loop"));
  v8::Local<v8::String> debug(TRI_V8_ASCII_STRING("debug"));

  v8::Local<v8::Object> callerScope;
  if (args.Length() >= 1) {
    TRI_AddGlobalVariableVocbase(isolate, isolate->GetCurrentContext(), debug,
                                 args[0]);
  }

  MUTEX_LOCKER(mutexLocker, ConsoleThread::serverConsoleMutex);
  V8LineEditor* console = ConsoleThread::serverConsole;

  if (console != nullptr) {
    while (true) {
      bool eof;
      std::string input = console->prompt("debug> ", "debug", eof);

      if (eof) {
        break;
      }

      if (input.empty()) {
        continue;
      }

      console->addHistory(input);

      {
        v8::HandleScope scope(isolate);
        v8::TryCatch tryCatch;

        TRI_ExecuteJavaScriptString(isolate, isolate->GetCurrentContext(),
                                    TRI_V8_STRING(input.c_str()), name, true);

        if (tryCatch.HasCaught()) {
          std::cout << TRI_StringifyV8Exception(isolate, &tryCatch);
        }
      }
    }
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_CompareString(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "COMPARE_STRING(<left string>, <right string>)");
  }

  v8::String::Value left(args[0]);
  v8::String::Value right(args[1]);

  // ..........................................................................
  // Take note here: we are assuming that the ICU type UChar is two bytes.
  // There is no guarantee that this will be the case on all platforms and
  // compilers.
  // ..........................................................................
  int result = Utf8Helper::DefaultUtf8Helper.compareUtf16(
      *left, left.length(), *right, right.length());

  TRI_V8_RETURN(v8::Integer::New(isolate, result));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of timezones
////////////////////////////////////////////////////////////////////////////////

static void JS_GetIcuTimezones(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("TIMEZONES()");
  }

  v8::Handle<v8::Array> result = v8::Array::New(isolate);

  UErrorCode status = U_ZERO_ERROR;

  StringEnumeration* timeZones = TimeZone::createEnumeration();
  if (timeZones) {
    int32_t idsCount = timeZones->count(status);

    for (int32_t i = 0; i < idsCount && U_ZERO_ERROR == status; ++i) {
      int32_t resultLength;
      char const* str = timeZones->next(&resultLength, status);
      result->Set((uint32_t)i, TRI_V8_PAIR_STRING(str, resultLength));
    }

    delete timeZones;
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of locales
////////////////////////////////////////////////////////////////////////////////

static void JS_GetIcuLocales(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("LOCALES()");
  }

  v8::Handle<v8::Array> result = v8::Array::New(isolate);

  int32_t count = 0;
  const Locale* locales = Locale::getAvailableLocales(count);
  if (locales) {
    for (int32_t i = 0; i < count; ++i) {
      const Locale* l = locales + i;
      char const* str = l->getBaseName();

      result->Set((uint32_t)i, TRI_V8_STRING(str));
    }
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief format datetime
////////////////////////////////////////////////////////////////////////////////

static void JS_FormatDatetime(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "FORMAT_DATETIME(<datetime in sec>, <pattern>, [<timezone>, "
        "[<locale>]])");
  }

  int64_t datetime = TRI_ObjectToInt64(args[0]);
  v8::String::Value pattern(args[1]);

  TimeZone* tz = 0;
  if (args.Length() > 2) {
    v8::String::Value value(args[2]);

    // ..........................................................................
    // Take note here: we are assuming that the ICU type UChar is two bytes.
    // There is no guarantee that this will be the case on all platforms and
    // compilers.
    // ..........................................................................

    UnicodeString ts((const UChar*)*value, value.length());
    tz = TimeZone::createTimeZone(ts);
  } else {
    tz = TimeZone::createDefault();
  }

  Locale locale;
  if (args.Length() > 3) {
    std::string name = TRI_ObjectToString(args[3]);
    locale = Locale::createFromName(name.c_str());
  } else {
    // use language of default collator
    std::string name = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();
    locale = Locale::createFromName(name.c_str());
  }

  UnicodeString formattedString;
  UErrorCode status = U_ZERO_ERROR;
  UnicodeString aPattern((const UChar*)*pattern, pattern.length());
  DateFormatSymbols* ds = new DateFormatSymbols(locale, status);
  SimpleDateFormat* s = new SimpleDateFormat(aPattern, ds, status);
  s->setTimeZone(*tz);
  s->format((UDate)(datetime * 1000), formattedString);

  std::string resultString;
  formattedString.toUTF8String(resultString);
  delete s;
  delete tz;

  TRI_V8_RETURN_STD_STRING(resultString);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse datetime
////////////////////////////////////////////////////////////////////////////////

static void JS_ParseDatetime(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "PARSE_DATETIME(<datetime string>, <pattern>, [<timezone>, "
        "[<locale>]])");
  }

  v8::String::Value datetimeString(args[0]);
  v8::String::Value pattern(args[1]);

  TimeZone* tz = 0;
  if (args.Length() > 2) {
    v8::String::Value value(args[2]);

    // ..........................................................................
    // Take note here: we are assuming that the ICU type UChar is two bytes.
    // There is no guarantee that this will be the case on all platforms and
    // compilers.
    // ..........................................................................

    UnicodeString ts((const UChar*)*value, value.length());
    tz = TimeZone::createTimeZone(ts);
  } else {
    tz = TimeZone::createDefault();
  }

  Locale locale;
  if (args.Length() > 3) {
    std::string name = TRI_ObjectToString(args[3]);
    locale = Locale::createFromName(name.c_str());
  } else {
    // use language of default collator
    std::string name = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();
    locale = Locale::createFromName(name.c_str());
  }

  UnicodeString formattedString((const UChar*)*datetimeString,
                                datetimeString.length());
  UErrorCode status = U_ZERO_ERROR;
  UnicodeString aPattern((const UChar*)*pattern, pattern.length());
  DateFormatSymbols* ds = new DateFormatSymbols(locale, status);
  SimpleDateFormat* s = new SimpleDateFormat(aPattern, ds, status);
  s->setTimeZone(*tz);

  UDate udate = s->parse(formattedString, status);

  delete s;
  delete tz;

  TRI_V8_RETURN(v8::Number::New(isolate, udate / 1000));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the authentication info, coordinator case
////////////////////////////////////////////////////////////////////////////////

static bool ReloadAuthCoordinator(TRI_vocbase_t* vocbase) {
  VPackBuilder builder;
  builder.openArray();

  int res = usersOnCoordinator(std::string(vocbase->_name), builder, 60.0);

  if (res == TRI_ERROR_NO_ERROR) {
    builder.close();
    return TRI_PopulateAuthInfo(vocbase, builder.slice());
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the authentication info from collection _users
////////////////////////////////////////////////////////////////////////////////

static void JS_ReloadAuth(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("RELOAD_AUTH()");
  }

  bool result;
  if (ServerState::instance()->isCoordinator()) {
    result = ReloadAuthCoordinator(vocbase);
  } else {
    result = TRI_ReloadAuthInfo(vocbase);
  }
  if (result) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an AQL query
////////////////////////////////////////////////////////////////////////////////

static void JS_ParseAql(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_PARSE(<querystring>)");
  }

  // get the query string
  if (!args[0]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <querystring>");
  }

  std::string const queryString(TRI_ObjectToString(args[0]));

  arangodb::aql::Query query(true, vocbase, queryString.c_str(),
                             queryString.size(), nullptr, nullptr,
                             arangodb::aql::PART_MAIN);

  auto parseResult = query.parse();

  if (parseResult.code != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_FULL(parseResult.code, parseResult.details);
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("parsed"), v8::True(isolate));

  {
    v8::Handle<v8::Array> collections = v8::Array::New(isolate);
    result->Set(TRI_V8_ASCII_STRING("collections"), collections);
    uint32_t i = 0;
    for (auto& elem : parseResult.collectionNames) {
      collections->Set(i++, TRI_V8_STD_STRING((elem)));
    }
  }

  {
    v8::Handle<v8::Array> bindVars = v8::Array::New(isolate);
    uint32_t i = 0;
    for (auto const& elem : parseResult.bindParameters) {
      bindVars->Set(i++, TRI_V8_STD_STRING((elem)));
    }
    result->Set(TRI_V8_ASCII_STRING("parameters"), bindVars);
  }

  result->Set(TRI_V8_ASCII_STRING("ast"),
              TRI_VPackToV8(isolate, parseResult.result->slice()));

  if (parseResult.warnings == nullptr) {
    result->Set(TRI_V8_ASCII_STRING("warnings"), v8::Array::New(isolate));
  } else {
    result->Set(TRI_V8_ASCII_STRING("warnings"),
                TRI_VPackToV8(isolate, parseResult.warnings->slice()));
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a warning for the currently running AQL query
/// this function is called from aql.js
////////////////////////////////////////////////////////////////////////////////

static void JS_WarningAql(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_WARNING(<code>, <message>)");
  }

  // get the query string
  if (!args[1]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <message>");
  }

  TRI_GET_GLOBALS();

  if (v8g->_query != nullptr) {
    // only register the error if we have a query...
    // note: we may not have a query if the AQL functions are called without
    // a query, e.g. during tests
    int code = static_cast<int>(TRI_ObjectToInt64(args[0]));
    std::string const&& message = TRI_ObjectToString(args[1]);

    auto query = static_cast<arangodb::aql::Query*>(v8g->_query);
    query->registerWarning(code, message.c_str());
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explains an AQL query
////////////////////////////////////////////////////////////////////////////////

static void JS_ExplainAql(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "AQL_EXPLAIN(<querystring>, <bindvalues>, <options>)");
  }

  // get the query string
  if (!args[0]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <querystring>");
  }

  std::string const queryString(TRI_ObjectToString(args[0]));

  // bind parameters
  std::shared_ptr<VPackBuilder> bindVars;

  if (args.Length() > 1) {
    if (!args[1]->IsUndefined() && !args[1]->IsNull() && !args[1]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting object for <bindvalues>");
    }
    if (args[1]->IsObject()) {
      bindVars.reset(new VPackBuilder);

      int res = TRI_V8ToVPack(isolate, *(bindVars.get()), args[1], false);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_V8_THROW_EXCEPTION(res);
      }
    }
  }

  auto options = std::make_shared<VPackBuilder>();

  if (args.Length() > 2) {
    // handle options
    if (!args[2]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting object for <options>");
    }
    int res = TRI_V8ToVPack(isolate, *options, args[2], false);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  // bind parameters will be freed by the query later
  arangodb::aql::Query query(true, vocbase, queryString.c_str(),
                             queryString.size(), bindVars, options,
                             arangodb::aql::PART_MAIN);

  auto queryResult = query.explain();

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_FULL(queryResult.code, queryResult.details);
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  if (queryResult.result != nullptr) {
    if (query.allPlans()) {
      result->Set(TRI_V8_ASCII_STRING("plans"),
                  TRI_VPackToV8(isolate, queryResult.result->slice()));
    } else {
      result->Set(TRI_V8_ASCII_STRING("plan"),
                  TRI_VPackToV8(isolate, queryResult.result->slice()));
      result->Set(TRI_V8_ASCII_STRING("cacheable"),
                  v8::Boolean::New(isolate, queryResult.cached));
    }

    if (queryResult.warnings == nullptr) {
      result->Set(TRI_V8_ASCII_STRING("warnings"), v8::Array::New(isolate));
    } else {
      result->Set(TRI_V8_ASCII_STRING("warnings"),
                  TRI_VPackToV8(isolate, queryResult.warnings->slice()));
    }
    if (queryResult.stats != nullptr) {
      VPackSlice stats = queryResult.stats->slice();
      if (stats.isNone()) {
        result->Set(TRI_V8_STRING("stats"), v8::Object::New(isolate));
      } else {
        result->Set(TRI_V8_STRING("stats"), TRI_VPackToV8(isolate, stats));
      }
    } else {
      result->Set(TRI_V8_STRING("stats"), v8::Object::New(isolate));
    }
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an AQL query
////////////////////////////////////////////////////////////////////////////////

static void JS_ExecuteAqlJson(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() < 1 || args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_EXECUTEJSON(<queryjson>, <options>)");
  }

  if (!args[0]->IsObject()) {
    TRI_V8_THROW_TYPE_ERROR("expecting object for <queryjson>");
  }

  auto queryBuilder = std::make_shared<VPackBuilder>();
  int res = TRI_V8ToVPack(isolate, *queryBuilder, args[0], false);
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  auto options = std::make_shared<VPackBuilder>();

  if (args.Length() > 1) {
    // we have options! yikes!
    if (!args[1]->IsUndefined() && !args[1]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting object for <options>");
    }

    res = TRI_V8ToVPack(isolate, *options, args[1], false);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_GET_GLOBALS();
  arangodb::aql::Query query(true, vocbase, queryBuilder, options,
                             arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(
      static_cast<arangodb::aql::QueryRegistry*>(v8g->_queryRegistry));

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_FULL(queryResult.code, queryResult.details);
  }

  auto transactionContext =
      std::make_shared<StandaloneTransactionContext>(vocbase);
  // return the array value as it is. this is a performance optimization
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  if (queryResult.result != nullptr) {
    result->ForceSet(TRI_V8_ASCII_STRING("json"),
                     TRI_VPackToV8(isolate, queryResult.result->slice(),
                                   transactionContext->getVPackOptions()));
  }
  if (queryResult.stats != nullptr) {
    VPackSlice stats = queryResult.stats->slice();
    if (!stats.isNone()) {
      result->ForceSet(TRI_V8_ASCII_STRING("stats"),
                       TRI_VPackToV8(isolate, stats));
    }
  }
  if (queryResult.profile != nullptr) {
    result->ForceSet(TRI_V8_ASCII_STRING("profile"),
                     TRI_VPackToV8(isolate, queryResult.profile->slice()));
  }
  if (queryResult.warnings == nullptr) {
    result->ForceSet(TRI_V8_ASCII_STRING("warnings"), v8::Array::New(isolate));
  } else {
    result->ForceSet(TRI_V8_ASCII_STRING("warnings"),
                     TRI_VPackToV8(isolate, queryResult.warnings->slice()));
  }
  result->ForceSet(TRI_V8_ASCII_STRING("cached"),
                   v8::Boolean::New(isolate, queryResult.cached));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an AQL query
////////////////////////////////////////////////////////////////////////////////

static void JS_ExecuteAql(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "AQL_EXECUTE(<querystring>, <bindvalues>, <options>)");
  }

  // get the query string
  if (!args[0]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <querystring>");
  }

  std::string const queryString(TRI_ObjectToString(args[0]));

  // bind parameters
  std::shared_ptr<VPackBuilder> bindVars;

  // options
  auto options = std::make_shared<VPackBuilder>();

  if (args.Length() > 1) {
    if (!args[1]->IsUndefined() && !args[1]->IsNull() && !args[1]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting object for <bindvalues>");
    }
    if (args[1]->IsObject()) {
      bindVars.reset(new VPackBuilder);
      int res = TRI_V8ToVPack(isolate, *(bindVars.get()), args[1], false);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_V8_THROW_EXCEPTION(res);
      }
    }
  }

  if (args.Length() > 2) {
    // we have options! yikes!
    if (!args[2]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting object for <options>");
    }

    int res = TRI_V8ToVPack(isolate, *options, args[2], false);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  // bind parameters will be freed by the query later
  TRI_GET_GLOBALS();
  arangodb::aql::Query query(true, vocbase, queryString.c_str(),
                             queryString.size(), bindVars, options,
                             arangodb::aql::PART_MAIN);

  auto queryResult = query.executeV8(
      isolate, static_cast<arangodb::aql::QueryRegistry*>(v8g->_queryRegistry));

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED) {
      TRI_GET_GLOBALS();
      v8g->_canceled = true;
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    TRI_V8_THROW_EXCEPTION_FULL(queryResult.code, queryResult.details);
  }

  // return the array value as it is. this is a performance optimization
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->ForceSet(TRI_V8_ASCII_STRING("json"), queryResult.result);

  if (queryResult.stats != nullptr) {
    VPackSlice stats = queryResult.stats->slice();
    if (!stats.isNone()) {
      result->ForceSet(TRI_V8_ASCII_STRING("stats"),
                       TRI_VPackToV8(isolate, stats));
    }
  }
  if (queryResult.profile != nullptr) {
    result->ForceSet(TRI_V8_ASCII_STRING("profile"),
                     TRI_VPackToV8(isolate, queryResult.profile->slice()));
  }
  if (queryResult.warnings == nullptr) {
    result->ForceSet(TRI_V8_ASCII_STRING("warnings"), v8::Array::New(isolate));
  } else {
    result->ForceSet(TRI_V8_ASCII_STRING("warnings"),
                     TRI_VPackToV8(isolate, queryResult.warnings->slice()));
  }
  result->ForceSet(TRI_V8_ASCII_STRING("cached"),
                   v8::Boolean::New(isolate, queryResult.cached));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief retrieve global query options or configure them
////////////////////////////////////////////////////////////////////////////////

static void JS_QueriesPropertiesAql(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto queryList = static_cast<arangodb::aql::QueryList*>(vocbase->_queries);
  TRI_ASSERT(queryList != nullptr);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERIES_PROPERTIES(<options>)");
  }

  if (args.Length() == 1) {
    // store options
    if (!args[0]->IsObject()) {
      TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERIES_PROPERTIES(<options>)");
    }

    auto obj = args[0]->ToObject();
    if (obj->Has(TRI_V8_ASCII_STRING("enabled"))) {
      queryList->enabled(
          TRI_ObjectToBoolean(obj->Get(TRI_V8_ASCII_STRING("enabled"))));
    }
    if (obj->Has(TRI_V8_ASCII_STRING("trackSlowQueries"))) {
      queryList->trackSlowQueries(TRI_ObjectToBoolean(
          obj->Get(TRI_V8_ASCII_STRING("trackSlowQueries"))));
    }
    if (obj->Has(TRI_V8_ASCII_STRING("maxSlowQueries"))) {
      queryList->maxSlowQueries(static_cast<size_t>(
          TRI_ObjectToInt64(obj->Get(TRI_V8_ASCII_STRING("maxSlowQueries")))));
    }
    if (obj->Has(TRI_V8_ASCII_STRING("slowQueryThreshold"))) {
      queryList->slowQueryThreshold(TRI_ObjectToDouble(
          obj->Get(TRI_V8_ASCII_STRING("slowQueryThreshold"))));
    }
    if (obj->Has(TRI_V8_ASCII_STRING("maxQueryStringLength"))) {
      queryList->maxQueryStringLength(static_cast<size_t>(TRI_ObjectToInt64(
          obj->Get(TRI_V8_ASCII_STRING("maxQueryStringLength")))));
    }

    // fall-through intentional
  }

  // return current settings
  auto result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("enabled"),
              v8::Boolean::New(isolate, queryList->enabled()));
  result->Set(TRI_V8_ASCII_STRING("trackSlowQueries"),
              v8::Boolean::New(isolate, queryList->trackSlowQueries()));
  result->Set(TRI_V8_ASCII_STRING("maxSlowQueries"),
              v8::Number::New(
                  isolate, static_cast<double>(queryList->maxSlowQueries())));
  result->Set(TRI_V8_ASCII_STRING("slowQueryThreshold"),
              v8::Number::New(isolate, queryList->slowQueryThreshold()));
  result->Set(TRI_V8_ASCII_STRING("maxQueryStringLength"),
              v8::Number::New(isolate, static_cast<double>(
                                           queryList->maxQueryStringLength())));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of currently running queries
////////////////////////////////////////////////////////////////////////////////

static void JS_QueriesCurrentAql(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERIES_CURRENT()");
  }

  auto queryList = static_cast<arangodb::aql::QueryList*>(vocbase->_queries);
  TRI_ASSERT(queryList != nullptr);

  try {
    auto const&& queries = queryList->listCurrent();

    uint32_t i = 0;
    auto result = v8::Array::New(isolate, static_cast<int>(queries.size()));

    for (auto q : queries) {
      auto const&& timeString = TRI_StringTimeStamp(q.started);
      auto const& queryState = q.queryState.substr(8, q.queryState.size() - 9);

      v8::Handle<v8::Object> obj = v8::Object::New(isolate);
      obj->Set(TRI_V8_ASCII_STRING("id"), V8TickId(isolate, q.id));
      obj->Set(TRI_V8_ASCII_STRING("query"), TRI_V8_STD_STRING(q.queryString));
      obj->Set(TRI_V8_ASCII_STRING("started"), TRI_V8_STD_STRING(timeString));
      obj->Set(TRI_V8_ASCII_STRING("runTime"),
               v8::Number::New(isolate, q.runTime));
      obj->Set(TRI_V8_ASCII_STRING("state"), TRI_V8_STD_STRING(queryState));
      result->Set(i++, obj);
    }

    TRI_V8_RETURN(result);
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of slow running queries or clears the list
////////////////////////////////////////////////////////////////////////////////

static void JS_QueriesSlowAql(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto queryList = static_cast<arangodb::aql::QueryList*>(vocbase->_queries);
  TRI_ASSERT(queryList != nullptr);

  if (args.Length() == 1) {
    queryList->clearSlow();
    TRI_V8_RETURN_TRUE();
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERIES_SLOW()");
  }

  try {
    auto const&& queries = queryList->listSlow();

    uint32_t i = 0;
    auto result = v8::Array::New(isolate, static_cast<int>(queries.size()));

    for (auto q : queries) {
      auto const&& timeString = TRI_StringTimeStamp(q.started);
      auto const& queryState = q.queryState.substr(8, q.queryState.size() - 9);

      v8::Handle<v8::Object> obj = v8::Object::New(isolate);
      obj->Set(TRI_V8_ASCII_STRING("id"), V8TickId(isolate, q.id));
      obj->Set(TRI_V8_ASCII_STRING("query"), TRI_V8_STD_STRING(q.queryString));
      obj->Set(TRI_V8_ASCII_STRING("started"), TRI_V8_STD_STRING(timeString));
      obj->Set(TRI_V8_ASCII_STRING("runTime"),
               v8::Number::New(isolate, q.runTime));
      obj->Set(TRI_V8_ASCII_STRING("state"), TRI_V8_STD_STRING(queryState));
      result->Set(i++, obj);
    }

    TRI_V8_RETURN(result);
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief kills an AQL query
////////////////////////////////////////////////////////////////////////////////

static void JS_QueriesKillAql(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERIES_KILL(<id>)");
  }

  auto id = TRI_ObjectToUInt64(args[0], true);

  auto queryList = static_cast<arangodb::aql::QueryList*>(vocbase->_queries);
  TRI_ASSERT(queryList != nullptr);

  auto res = queryList->kill(id);

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_THROW_EXCEPTION(res);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a query is killed
////////////////////////////////////////////////////////////////////////////////

static void JS_QueryIsKilledAql(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();
  if (v8g->_query != nullptr &&
      static_cast<arangodb::aql::Query*>(v8g->_query)->killed()) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief configures the AQL query cache
////////////////////////////////////////////////////////////////////////////////

static void JS_QueryCachePropertiesAql(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() > 1 || (args.Length() == 1 && !args[0]->IsObject())) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERY_CACHE_PROPERTIES(<properties>)");
  }

  auto queryCache = arangodb::aql::QueryCache::instance();

  if (args.Length() == 1) {
    // called with options
    auto obj = args[0]->ToObject();

    std::pair<std::string, size_t> cacheProperties;
    // fetch current configuration
    queryCache->properties(cacheProperties);

    if (obj->Has(TRI_V8_ASCII_STRING("mode"))) {
      cacheProperties.first =
          TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("mode")));
    }

    if (obj->Has(TRI_V8_ASCII_STRING("maxResults"))) {
      cacheProperties.second = static_cast<size_t>(
          TRI_ObjectToInt64(obj->Get(TRI_V8_ASCII_STRING("maxResults"))));
    }

    // set mode and max elements
    queryCache->setProperties(cacheProperties);
  }

  auto properties = queryCache->properties();
  TRI_V8_RETURN(TRI_VPackToV8(isolate, properties.slice()));

  // fetch current configuration and return it
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidates the AQL query cache
////////////////////////////////////////////////////////////////////////////////

static void JS_QueryCacheInvalidateAql(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERY_CACHE_INVALIDATE()");
  }

  arangodb::aql::QueryCache::instance()->invalidate();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief throw collection not loaded
////////////////////////////////////////////////////////////////////////////////

static void JS_ThrowCollectionNotLoaded(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() == 0) {
    bool value = TRI_GetThrowCollectionNotLoadedVocBase();
    TRI_V8_RETURN(v8::Boolean::New(isolate, value));
  } else if (args.Length() == 1) {
    TRI_SetThrowCollectionNotLoadedVocBase(TRI_ObjectToBoolean(args[0]));
  } else {
    TRI_V8_THROW_EXCEPTION_USAGE("THROW_COLLECTION_NOT_LOADED(<value>)");
  }

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms VertexId to v8 object
///        NOTE: Collection has to be known to the transaction.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> VertexIdToData(v8::Isolate* isolate,
                                            Transaction* trx,
                                            std::string const& vertexId) {
  size_t pos = vertexId.find('/');
  TRI_ASSERT(pos != std::string::npos); // All are internal _id attributes

  TransactionBuilderLeaser builder(trx);
  builder->openObject();
  builder->add(StaticStrings::KeyString, VPackValue(vertexId.substr(pos + 1)));
  builder->close();

  OperationOptions options;
  OperationResult opRes = trx->document(vertexId.substr(0, pos), builder->slice(), options);

  if (opRes.failed()) {
    v8::EscapableHandleScope scope(isolate);
    return scope.Escape<v8::Value>(v8::Null(isolate));
  }
  VPackOptions resultOptions = VPackOptions::Defaults;
  resultOptions.customTypeHandler = opRes.customTypeHandler.get();
  return TRI_VPackToV8(isolate, opRes.slice(), &resultOptions);
}

static v8::Handle<v8::Value> VertexIdToData(v8::Isolate* isolate,
                                            Transaction* trx,
                                            VPackSlice const& vertexId) {
  return VertexIdToData(isolate, trx, vertexId.copyString());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Start a new transaction for the given collections and request
///        all necessary ditches.
///        The caller is responsible to finish and delete the transaction.
///        If this function throws the transaction is non-existent.
////////////////////////////////////////////////////////////////////////////////

static ExplicitTransaction* BeginTransaction(
    std::shared_ptr<V8TransactionContext> transactionContext,
    std::vector<std::string> const& readCollections,
    std::vector<std::string> const& writeCollections) {
  // IHHF isCoordinator
  double lockTimeout =
      (double)(TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);
  bool waitForSync = false;

  // Start Transaction to collect all parts of the path
  auto trx = std::make_unique<ExplicitTransaction>(
      transactionContext, readCollections, writeCollections, lockTimeout,
      waitForSync, true);

  int res = trx->begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return trx.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms an ArangoDBPathFinder::Path to v8 json values
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> PathIdsToV8(v8::Isolate* isolate,
                                         TRI_vocbase_t* vocbase,
                                         arangodb::Transaction* trx,
                                         ArangoDBPathFinder::Path const& p,
                                         bool& includeData) {
  v8::EscapableHandleScope scope(isolate);
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  uint32_t const vn = static_cast<uint32_t>(p.vertices.size());
  v8::Handle<v8::Array> vertices =
      v8::Array::New(isolate, static_cast<int>(vn));

  uint32_t const en = static_cast<uint32_t>(p.edges.size());
  v8::Handle<v8::Array> edges = v8::Array::New(isolate, static_cast<int>(en));

  if (includeData) {
    for (uint32_t j = 0; j < vn; ++j) {
      vertices->Set(j, VertexIdToData(isolate, trx, p.vertices[j]));
    }
    for (uint32_t j = 0; j < en; ++j) {
      VPackOptions resultOptions = VPackOptions::Defaults;
      resultOptions.customTypeHandler =
          trx->transactionContext()->orderCustomTypeHandler().get();
      edges->Set(j, TRI_VPackToV8(isolate, p.edges[j], &resultOptions));
    }
  } else {
    for (uint32_t j = 0; j < vn; ++j) {
      vertices->Set(j, TRI_V8_STD_STRING(p.vertices[j].copyString()));
    }
    for (uint32_t j = 0; j < en; ++j) {
      edges->Set(j, TRI_V8_STD_STRING(trx->extractIdString(p.edges[j])));
    }
  }

  result->Set(TRI_V8_STRING("vertices"), vertices);
  result->Set(TRI_V8_STRING("edges"), edges);
  result->Set(TRI_V8_STRING("distance"),
              v8::Number::New(isolate, static_cast<double>(p.weight)));

  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms an ConstDistanceFinder::Path to v8 json values
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> PathIdsToV8(
    v8::Isolate* isolate, TRI_vocbase_t* vocbase, arangodb::Transaction* trx,
    ArangoDBConstDistancePathFinder::Path const& p, bool& includeData) {
  v8::EscapableHandleScope scope(isolate);
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  uint32_t const vn = static_cast<uint32_t>(p.vertices.size());
  v8::Handle<v8::Array> vertices =
      v8::Array::New(isolate, static_cast<int>(vn));

  uint32_t const en = static_cast<uint32_t>(p.edges.size());
  v8::Handle<v8::Array> edges = v8::Array::New(isolate, static_cast<int>(en));

  if (includeData) {
    for (uint32_t j = 0; j < vn; ++j) {
      vertices->Set(j, VertexIdToData(isolate, trx, p.vertices[j]));
    }
    for (uint32_t j = 0; j < en; ++j) {
      VPackOptions resultOptions = VPackOptions::Defaults;
      resultOptions.customTypeHandler =
          trx->transactionContext()->orderCustomTypeHandler().get();
      edges->Set(j, TRI_VPackToV8(isolate, p.edges[j], &resultOptions));
    }
  } else {
    for (uint32_t j = 0; j < vn; ++j) {
      vertices->Set(j, TRI_V8_STD_STRING(p.vertices[j].copyString()));
    }
    for (uint32_t j = 0; j < en; ++j) {
      edges->Set(j, TRI_V8_STD_STRING(trx->extractIdString(p.edges[j])));
    }
  }

  result->Set(TRI_V8_STRING("vertices"), vertices);
  result->Set(TRI_V8_STRING("edges"), edges);
  result->Set(TRI_V8_STRING("distance"),
              v8::Number::New(isolate, static_cast<double>(p.weight)));

  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extract collection names from v8 array.
////////////////////////////////////////////////////////////////////////////////

static void V8ArrayToStrings(const v8::Handle<v8::Value>& parameter,
                             std::unordered_set<std::string>& result) {
  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(parameter);
  uint32_t const n = array->Length();
  for (uint32_t i = 0; i < n; ++i) {
    if (array->Get(i)->IsString()) {
      result.emplace(TRI_ObjectToString(array->Get(i)));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Define edge weight by the number of hops.
///        Respectively 1 for any edge.
////////////////////////////////////////////////////////////////////////////////

class HopWeightCalculator {
 public:
  HopWeightCalculator(){};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Callable weight calculator for edge
  //////////////////////////////////////////////////////////////////////////////

  double operator()(VPackSlice const edge) { return 1; }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Define edge weight by ony special attribute.
///        Respectively 1 for any edge.
////////////////////////////////////////////////////////////////////////////////

class AttributeWeightCalculator {
  std::string const _attribute;
  double const _defaultWeight;

 public:
  AttributeWeightCalculator(std::string const& attribute, double defaultWeight)
      : _attribute(attribute), _defaultWeight(defaultWeight) {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Callable weight calculator for edge
  //////////////////////////////////////////////////////////////////////////////

  double operator()(VPackSlice const edge) {
    if (_attribute.empty()) {
      return _defaultWeight;
    }

    VPackSlice attr = edge.get(_attribute);
    if (!attr.isNumber()) {
      return _defaultWeight;
    }
    return attr.getNumericValue<double>();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Executes a shortest Path Traversal
////////////////////////////////////////////////////////////////////////////////

static void JS_QueryShortestPath(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 4 || args.Length() > 5) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "CPP_SHORTEST_PATH(<vertexcollcetions[]>, <edgecollections[]>, "
        "<start>, <end>, <options>)");
  }

  // get the vertex collections
  if (!args[0]->IsArray()) {
    TRI_V8_THROW_TYPE_ERROR("expecting array for <vertexcollections[]>");
  }
  std::unordered_set<std::string> vertexCollectionNames;
  V8ArrayToStrings(args[0], vertexCollectionNames);

  // get the edge collections
  if (!args[1]->IsArray()) {
    TRI_V8_THROW_TYPE_ERROR("expecting array for <edgecollections[]>");
  }
  std::unordered_set<std::string> edgeCollectionNames;
  V8ArrayToStrings(args[1], edgeCollectionNames);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (!args[2]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting id for <startVertex>");
  }
  std::string const startVertex = TRI_ObjectToString(args[2]);

  if (!args[3]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting id for <targetVertex>");
  }
  std::string const targetVertex = TRI_ObjectToString(args[3]);

  auto transactionContext =
      std::make_shared<V8TransactionContext>(vocbase, true);

  int res = TRI_ERROR_NO_ERROR;
  std::vector<std::string> readCollections;
  std::vector<std::string> writeCollections;

  for (auto const& it : edgeCollectionNames) {
    readCollections.emplace_back(it);
  }
  for (auto const& it : vertexCollectionNames) {
    readCollections.emplace_back(it);
  }

  // Start the transaction
  std::unique_ptr<ExplicitTransaction> trx;

  try {
    trx.reset(BeginTransaction(transactionContext, readCollections,
                               writeCollections));
  } catch (Exception& e) {
    TRI_V8_THROW_EXCEPTION(e.code());
  }

  traverser::ShortestPathOptions opts(trx.get());

  bool includeData = false;
  v8::Handle<v8::Object> edgeExample;
  v8::Handle<v8::Object> vertexExample;
  if (args.Length() == 5) {
    if (!args[4]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting json for <options>");
    }
    v8::Handle<v8::Object> options = args[4]->ToObject();

    // Parse direction
    v8::Local<v8::String> keyDirection = TRI_V8_ASCII_STRING("direction");
    if (options->Has(keyDirection)) {
      opts.direction = TRI_ObjectToString(options->Get(keyDirection));
      if (opts.direction != "outbound" && opts.direction != "inbound" &&
          opts.direction != "any") {
        TRI_V8_THROW_TYPE_ERROR(
            "expecting direction to be 'outbound', 'inbound' or 'any'");
      }
    }

    // Parse Distance
    v8::Local<v8::String> keyWeight = TRI_V8_ASCII_STRING("weight");
    v8::Local<v8::String> keyDefaultWeight =
        TRI_V8_ASCII_STRING("defaultWeight");
    if (options->Has(keyWeight) && options->Has(keyDefaultWeight)) {
      opts.useWeight = true;
      opts.weightAttribute = TRI_ObjectToString(options->Get(keyWeight));
      opts.defaultWeight = TRI_ObjectToDouble(options->Get(keyDefaultWeight));
    }

    // Parse includeData
    v8::Local<v8::String> keyIncludeData = TRI_V8_ASCII_STRING("includeData");
    if (options->Has(keyIncludeData)) {
      includeData = TRI_ObjectToBoolean(options->Get(keyIncludeData));
    }

    // Parse bidirectional
    v8::Local<v8::String> keyBidirectional =
        TRI_V8_ASCII_STRING("bidirectional");
    if (options->Has(keyBidirectional)) {
      opts.bidirectional = TRI_ObjectToBoolean(options->Get(keyBidirectional));
    }

    // Parse multiThreaded
    v8::Local<v8::String> keyMultiThreaded =
        TRI_V8_ASCII_STRING("multiThreaded");
    if (options->Has(keyMultiThreaded)) {
      opts.multiThreaded = TRI_ObjectToBoolean(options->Get(keyMultiThreaded));
    }

    // Parse filterEdges
    // note: only works with edge examples and not with user-defined AQL
    // functions
    v8::Local<v8::String> keyFilterEdges = TRI_V8_ASCII_STRING("filterEdges");
    if (options->Has(keyFilterEdges)) {
      opts.useEdgeFilter = true;
      edgeExample = v8::Handle<v8::Object>::Cast(options->Get(keyFilterEdges));
    }

    // Parse vertexFilter
    // note: only works with vertex examples and not with user-defined AQL
    // functions
    v8::Local<v8::String> keyFilterVertices =
        TRI_V8_ASCII_STRING("filterVertices");
    if (options->Has(keyFilterVertices)) {
      opts.useVertexFilter = true;
      vertexExample =
          v8::Handle<v8::Object>::Cast(options->Get(keyFilterVertices));
    }
  }

  std::vector<EdgeCollectionInfo*> edgeCollectionInfos;

  arangodb::basics::ScopeGuard guard{[]() -> void {},
                                     [&edgeCollectionInfos]() -> void {
                                       for (auto& p : edgeCollectionInfos) {
                                         delete p;
                                       }
                                     }};

  if (opts.useWeight) {
    for (auto const& it : edgeCollectionNames) {
      edgeCollectionInfos.emplace_back(new EdgeCollectionInfo(
          trx.get(), it,
          AttributeWeightCalculator(opts.weightAttribute, opts.defaultWeight)));
    }
  } else {
    for (auto const& it : edgeCollectionNames) {
      edgeCollectionInfos.emplace_back(
          new EdgeCollectionInfo(trx.get(), it, HopWeightCalculator()));
    }
  }

  if (opts.useEdgeFilter) {
    std::string errorMessage;
    for (auto const& it : edgeCollectionNames) {
      try {
        opts.addEdgeFilter(isolate, edgeExample, it, errorMessage);
      } catch (Exception& e) {
        // ELEMENT not found is expected, if there is no shape of this type in
        // this collection
        if (e.code() != TRI_RESULT_ELEMENT_NOT_FOUND) {
          TRI_V8_THROW_EXCEPTION(e.code());
        }
      }
    }
  }

  if (opts.useVertexFilter) {
    std::string errorMessage;
    for (auto const& it : vertexCollectionNames) {
      try {
        opts.addVertexFilter(isolate, vertexExample, trx.get(), it,
                             errorMessage);
      } catch (Exception& e) {
        // ELEMENT not found is expected, if there is no shape of this type in
        // this collection
        if (e.code() != TRI_RESULT_ELEMENT_NOT_FOUND) {
          TRI_V8_THROW_EXCEPTION(e.code());
        }
      }
    }
  }

  try {
    opts.setStart(startVertex);
    opts.setEnd(targetVertex);
  } catch (Exception& e) {
    // Id string might have illegal collection name
    trx->finish(e.code());
    TRI_V8_THROW_EXCEPTION(e.code());
  }

  if (opts.useVertexFilter || opts.useEdgeFilter || opts.useWeight) {
    // Compute the path
    std::unique_ptr<ArangoDBPathFinder::Path> path;

    try {
      path = TRI_RunShortestPathSearch(edgeCollectionInfos, opts);
    } catch (Exception& e) {
      trx->finish(e.code());
      TRI_V8_THROW_EXCEPTION(e.code());
    }

    // Lift the result to v8
    if (path.get() == nullptr) {
      v8::EscapableHandleScope scope(isolate);
      trx->finish(res);
      TRI_V8_RETURN(scope.Escape<v8::Value>(v8::Null(isolate)));
    }

    try {
      auto result =
          PathIdsToV8(isolate, vocbase, trx.get(), *path, includeData);
      TRI_V8_RETURN(result);
    } catch (Exception& e) {
      TRI_V8_THROW_EXCEPTION(e.code());
    }
  } else {
    // No Data reading required for this path. Use shortcuts.
    // Compute the path
    std::unique_ptr<ArangoDBConstDistancePathFinder::Path> path;

    try {
      path =
          TRI_RunSimpleShortestPathSearch(edgeCollectionInfos, trx.get(), opts);
    } catch (Exception& e) {
      trx->finish(e.code());
      TRI_V8_THROW_EXCEPTION(e.code());
    }

    // Lift the result to v8
    if (path.get() == nullptr) {
      v8::EscapableHandleScope scope(isolate);
      trx->finish(res);
      TRI_V8_RETURN(scope.Escape<v8::Value>(v8::Null(isolate)));
    }

    try {
      auto result =
          PathIdsToV8(isolate, vocbase, trx.get(), *path, includeData);
      TRI_V8_RETURN(result);
    } catch (Exception& e) {
      TRI_V8_THROW_EXCEPTION(e.code());
    }
  }
  trx->finish(res);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms an vector<VertexId> to v8 json values
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> VertexIdsToV8(v8::Isolate* isolate,
                                           ExplicitTransaction* trx,
                                           std::vector<VPackSlice> const& ids,
                                           bool includeData = false) {
  v8::EscapableHandleScope scope(isolate);
  uint32_t const vn = static_cast<uint32_t>(ids.size());
  v8::Handle<v8::Array> vertices =
      v8::Array::New(isolate, static_cast<int>(vn));

  uint32_t j = 0;
  if (includeData) {
    for (auto& it : ids) {
      vertices->Set(j, VertexIdToData(isolate, trx, it.copyString()));
      ++j;
    }
  } else {
    for (auto& it : ids) {
      vertices->Set(j, TRI_V8_STD_STRING(it.copyString()));
      ++j;
    }
  }
  return scope.Escape<v8::Value>(vertices);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Executes a Neighbors computation
////////////////////////////////////////////////////////////////////////////////

static void JS_QueryNeighbors(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || args.Length() > 4) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "CPP_NEIGHBORS(<vertexcollections[]>, <edgecollections[]>, <start>, "
        "<options>)");
  }

  // get the vertex collections
  if (!args[0]->IsArray()) {
    TRI_V8_THROW_TYPE_ERROR("expecting array for <vertexcollections[]>");
  }
  std::unordered_set<std::string> vertexCollectionNames;
  V8ArrayToStrings(args[0], vertexCollectionNames);

  // get the edge collections
  if (!args[1]->IsArray()) {
    TRI_V8_THROW_TYPE_ERROR("expecting array for <edgecollections[]>");
  }
  std::unordered_set<std::string> edgeCollectionNames;
  V8ArrayToStrings(args[1], edgeCollectionNames);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::vector<std::string> startVertices;
  if (args[2]->IsString()) {
    startVertices.emplace_back(TRI_ObjectToString(args[2]));
  } else if (args[2]->IsArray()) {
    auto list = v8::Handle<v8::Array>::Cast(args[2]);
    for (uint32_t i = 0; i < list->Length(); i++) {
      if (list->Get(i)->IsString()) {
        startVertices.emplace_back(TRI_ObjectToString(list->Get(i)));
      } else {
        TRI_V8_THROW_TYPE_ERROR("expecting array of IDs for <startVertex>");
      }
    }
  } else {
    TRI_V8_THROW_TYPE_ERROR("expecting string ID for <startVertex>");
  }

  std::vector<std::string> readCollections;
  std::vector<std::string> writeCollections;

  auto transactionContext =
      std::make_shared<V8TransactionContext>(vocbase, true);

  int res = TRI_ERROR_NO_ERROR;

  for (auto const& it : edgeCollectionNames) {
    readCollections.emplace_back(it);
  }
  for (auto const& it : vertexCollectionNames) {
    readCollections.emplace_back(it);
  }

  std::unique_ptr<ExplicitTransaction> trx;

  try {
    trx.reset(BeginTransaction(transactionContext, readCollections,
                               writeCollections));
  } catch (Exception& e) {
    TRI_V8_THROW_EXCEPTION(e.code());
  }

  traverser::NeighborsOptions opts(trx.get());
  bool includeData = false;
  v8::Handle<v8::Value> edgeExample;
  v8::Handle<v8::Value> vertexExample;

  if (args.Length() == 4) {
    if (!args[3]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting json for <options>");
    }
    v8::Handle<v8::Object> options = args[3]->ToObject();

    // Parse direction
    v8::Local<v8::String> keyDirection = TRI_V8_ASCII_STRING("direction");
    if (options->Has(keyDirection)) {
      std::string dir = TRI_ObjectToString(options->Get(keyDirection));
      if (dir == "outbound") {
        opts.direction = TRI_EDGE_OUT;
      } else if (dir == "inbound") {
        opts.direction = TRI_EDGE_IN;
      } else if (dir == "any") {
        opts.direction = TRI_EDGE_ANY;
      } else {
        TRI_V8_THROW_TYPE_ERROR(
            "expecting direction to be 'outbound', 'inbound' or 'any'");
      }
    }

    // Parse includeData
    v8::Local<v8::String> keyIncludeData = TRI_V8_ASCII_STRING("includeData");
    if (options->Has(keyIncludeData)) {
      includeData = TRI_ObjectToBoolean(options->Get(keyIncludeData));
    }

    // Parse filterEdges
    v8::Local<v8::String> keyFilterEdges = TRI_V8_ASCII_STRING("filterEdges");
    if (options->Has(keyFilterEdges)) {
      opts.useEdgeFilter = true;
      edgeExample = options->Get(keyFilterEdges);
    }

    // Parse vertexFilter
    v8::Local<v8::String> keyFilterVertices =
        TRI_V8_ASCII_STRING("filterVertices");
    if (options->Has(keyFilterVertices)) {
      opts.useVertexFilter = true;
      // note: only works with vertex examples and not with user-defined AQL
      // functions
      vertexExample =
          v8::Handle<v8::Object>::Cast(options->Get(keyFilterVertices));
    }

    // Parse minDepth
    v8::Local<v8::String> keyMinDepth = TRI_V8_ASCII_STRING("minDepth");
    if (options->Has(keyMinDepth)) {
      opts.minDepth = TRI_ObjectToUInt64(options->Get(keyMinDepth), false);
    }

    // Parse maxDepth
    v8::Local<v8::String> keyMaxDepth = TRI_V8_ASCII_STRING("maxDepth");
    if (options->Has(keyMaxDepth)) {
      opts.maxDepth = TRI_ObjectToUInt64(options->Get(keyMaxDepth), false);
    }
  }

  std::vector<EdgeCollectionInfo*> edgeCollectionInfos;

  arangodb::basics::ScopeGuard guard{[]() -> void {},
                                     [&edgeCollectionInfos]() -> void {
                                       for (auto& p : edgeCollectionInfos) {
                                         delete p;
                                       }
                                     }};

  for (auto const& it : edgeCollectionNames) {
    edgeCollectionInfos.emplace_back(
        new EdgeCollectionInfo(trx.get(), it, HopWeightCalculator()));
    TRI_IF_FAILURE("EdgeCollectionDitchOOM") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  for (auto const& it : vertexCollectionNames) {
    opts.addCollectionRestriction(it);
  }

  if (opts.useEdgeFilter) {
    std::string errorMessage;
    for (auto const& it : edgeCollectionInfos) {
      try {
        opts.addEdgeFilter(isolate, edgeExample, it->getName(), errorMessage);
      } catch (Exception& e) {
        // ELEMENT not found is expected, if there is no shape of this type in
        // this collection
        if (e.code() != TRI_RESULT_ELEMENT_NOT_FOUND) {
          TRI_V8_THROW_EXCEPTION(e.code());
        }
      }
    }
  }

  if (opts.useVertexFilter) {
    std::string errorMessage;
    for (auto const& it : vertexCollectionNames) {
      try {
        opts.addVertexFilter(isolate, vertexExample, trx.get(), it,
                             errorMessage);
      } catch (Exception& e) {
        // ELEMENT not found is expected, if there is no shape of this type in
        // this collection
        if (e.code() != TRI_RESULT_ELEMENT_NOT_FOUND) {
          TRI_V8_THROW_EXCEPTION(e.code());
        }
      }
    }
  }

  std::vector<VPackSlice> neighbors;
  std::unordered_set<VPackSlice,
                     arangodb::basics::VelocyPackHelper::VPackStringHash,
                     arangodb::basics::VelocyPackHelper::VPackStringEqual> visited;
  for (auto const& startVertex : startVertices) {
    opts.setStart(startVertex);
    try {
      TRI_RunNeighborsSearch(edgeCollectionInfos, opts, visited, neighbors);
    } catch (Exception& e) {
      trx->finish(e.code());
      TRI_V8_THROW_EXCEPTION(e.code());
    }
  }

  auto result = VertexIdsToV8(isolate, trx.get(), neighbors, includeData);

  trx->finish(res);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sleeps and checks for query abortion in between
////////////////////////////////////////////////////////////////////////////////

static void JS_QuerySleepAql(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("sleep(<seconds>)");
  }

  TRI_GET_GLOBALS();
  arangodb::aql::Query* query = static_cast<arangodb::aql::Query*>(v8g->_query);

  if (query == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_QUERY_NOT_FOUND);
  }

  double n = TRI_ObjectToDouble(args[0]);
  double const until = TRI_microtime() + n;

  while (TRI_microtime() < until) {
    usleep(10000);

    if (query != nullptr) {
      if (query->killed()) {
        TRI_V8_THROW_EXCEPTION(TRI_ERROR_QUERY_KILLED);
      }
    }
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes a V8 object
////////////////////////////////////////////////////////////////////////////////

static void JS_ObjectHash(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("hash(<object>)");
  }

  VPackBuilder builder;
  int res = TRI_V8ToVPack(isolate, builder, args[0], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // throw away the top bytes so the hash value can safely be used
  // without precision loss when storing in JavaScript etc.
  uint64_t hash = builder.slice().normalizedHash() & 0x0007ffffffffffffULL;

  TRI_V8_RETURN(v8::Number::New(isolate, static_cast<double>(hash)));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapVocBase(v8::Isolate* isolate,
                                          TRI_vocbase_t const* database) {
  TRI_GET_GLOBALS();

  v8::Handle<v8::Object> result =
      WrapClass(isolate, v8g->VocbaseTempl, WRP_VOCBASE_TYPE,
                const_cast<TRI_vocbase_t*>(database));
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionDatabaseCollectionName
////////////////////////////////////////////////////////////////////////////////

static void MapGetVocBase(v8::Local<v8::String> const name,
                          v8::PropertyCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // convert the JavaScript string to a string
  v8::String::Utf8Value s(name);
  char* key = *s;

  size_t keyLength = s.length();
  if (keyLength > 2 && key[keyLength - 2] == '(') {
    keyLength -= 2;
    key[keyLength] = '\0';
  }

  // empty or null
  if (key == nullptr || *key == '\0') {
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }

  if (strcmp(key, "hasOwnProperty") == 0 ||  // this prevents calling the
                                             // property getter again (i.e.
                                             // recursion!)
      strcmp(key, "toString") == 0 || strcmp(key, "toJSON") == 0) {
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }

  TRI_vocbase_col_t* collection = nullptr;

  // generate a name under which the cached property is stored
  std::string cacheKey(key, keyLength);
  cacheKey.push_back('*');

  v8::Local<v8::String> cacheName = TRI_V8_STD_STRING(cacheKey);
  v8::Handle<v8::Object> holder = args.Holder()->ToObject();

  if (*key == '_') {
    // special treatment for all properties starting with _
    v8::Local<v8::String> const l = TRI_V8_PAIR_STRING(key, (int)keyLength);

    if (holder->HasRealNamedProperty(l)) {
      // some internal function inside db
      TRI_V8_RETURN(v8::Handle<v8::Value>());
    }

    // something in the prototype chain?
    v8::Local<v8::Value> v = holder->GetRealNamedPropertyInPrototypeChain(l);

    if (!v.IsEmpty()) {
      if (!v->IsExternal()) {
        // something but an external... this means we can directly return this
        TRI_V8_RETURN(v8::Handle<v8::Value>());
      }
    }
  }

  TRI_GET_GLOBALS();

  auto globals = isolate->GetCurrentContext()->Global();

  v8::Handle<v8::Object> cacheObject;
  TRI_GET_GLOBAL_STRING(_DbCacheKey);
  if (globals->Has(_DbCacheKey)) {
    cacheObject = globals->Get(_DbCacheKey)->ToObject();
  }

  if (!cacheObject.IsEmpty() && cacheObject->HasRealNamedProperty(cacheName)) {
    v8::Handle<v8::Object> value =
        cacheObject->GetRealNamedProperty(cacheName)->ToObject();

    collection =
        TRI_UnwrapClass<TRI_vocbase_col_t>(value, WRP_VOCBASE_COL_TYPE);

    // check if the collection is from the same database
    if (collection != nullptr && collection->_vocbase == vocbase) {
      bool lock = true;
      auto ctx = static_cast<arangodb::V8TransactionContext*>(
          v8g->_transactionContext);
      if (ctx != nullptr && ctx->getParentTransaction() != nullptr) {
        TRI_transaction_t* trx = ctx->getParentTransaction();
        if (TRI_IsContainedCollectionTransaction(trx, collection->_cid)) {
          lock = false;
        }
      }

      TRI_vocbase_col_status_e status;
      TRI_voc_cid_t cid;
      uint32_t internalVersion;

      if (lock) {
        READ_LOCKER(readLocker, collection->_lock);
        status = collection->_status;
        cid = collection->_cid;
        internalVersion = collection->_internalVersion;
      } else {
        status = collection->_status;
        cid = collection->_cid;
        internalVersion = collection->_internalVersion;
      }

      // check if the collection is still alive
      if (status != TRI_VOC_COL_STATUS_DELETED && cid > 0 &&
          collection->_isLocal) {
        TRI_GET_GLOBAL_STRING(_IdKey);
        TRI_GET_GLOBAL_STRING(VersionKeyHidden);
        if (value->Has(_IdKey)) {
          auto cachedCid = static_cast<TRI_voc_cid_t>(
              TRI_ObjectToUInt64(value->Get(_IdKey), true));
          uint32_t cachedVersion =
              (uint32_t)TRI_ObjectToInt64(value->Get(VersionKeyHidden));

          if (cachedCid == cid && cachedVersion == internalVersion) {
            // cache hit
            TRI_V8_RETURN(value);
          }

          // store the updated version number in the object for future
          // comparisons
          value->ForceSet(VersionKeyHidden,
                          v8::Number::New(isolate, (double)internalVersion),
                          v8::DontEnum);

          // cid has changed (i.e. collection has been dropped and re-created)
          // or version has changed
        }
      }
    }

    // cache miss
    cacheObject->Delete(cacheName);
  }

  if (ServerState::instance()->isCoordinator()) {
    std::shared_ptr<CollectionInfo> const ci =
        ClusterInfo::instance()->getCollection(vocbase->_name,
                                               std::string(key));

    if ((*ci).empty()) {
      collection = nullptr;
    } else {
      collection = CoordinatorCollection(vocbase, *ci);

      if (collection != nullptr && collection->_cid == 0) {
        delete collection;
        collection = nullptr;
        TRI_V8_RETURN(v8::Handle<v8::Value>());
      }
    }
  } else {
    collection = TRI_LookupCollectionByNameVocBase(vocbase, std::string(key));
  }

  if (collection == nullptr) {
    if (*key == '_') {
      TRI_V8_RETURN(v8::Handle<v8::Value>());
    }

    TRI_V8_RETURN_UNDEFINED();
  }

  v8::Handle<v8::Value> result = WrapCollection(isolate, collection);

  if (result.IsEmpty()) {
    if (ServerState::instance()->isCoordinator()) {
      delete collection;
    }
    TRI_V8_RETURN_UNDEFINED();
  }

  if (!cacheObject.IsEmpty()) {
    cacheObject->ForceSet(cacheName, result);
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseVersion
////////////////////////////////////////////////////////////////////////////////

static void JS_VersionServer(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_V8_RETURN(TRI_V8_ASCII_STRING(ARANGODB_VERSION));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databasePath
////////////////////////////////////////////////////////////////////////////////

static void JS_PathDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_V8_RETURN_STRING(vocbase->_path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseId
////////////////////////////////////////////////////////////////////////////////

static void JS_IdDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_V8_RETURN(V8TickId(isolate, vocbase->_id));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseName
////////////////////////////////////////////////////////////////////////////////

static void JS_NameDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_V8_RETURN_STRING(vocbase->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseIsSystem
////////////////////////////////////////////////////////////////////////////////

static void JS_IsSystemDatabase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_V8_RETURN(v8::Boolean::New(isolate, TRI_IsSystemVocBase(vocbase)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseUseDatabase
////////////////////////////////////////////////////////////////////////////////

static void JS_UseDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._useDatabase(<name>)");
  }

  TRI_GET_GLOBALS();

  if (!v8g->_allowUseDatabase) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  std::string&& name = TRI_ObjectToString(args[0]);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  if (TRI_IsDeletedVocBase(vocbase)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (TRI_EqualString(name.c_str(), vocbase->_name)) {
    // same database. nothing to do
    TRI_V8_RETURN(WrapVocBase(isolate, vocbase));
  }

  if (ServerState::instance()->isCoordinator()) {
    vocbase = TRI_UseCoordinatorDatabaseServer(
        static_cast<TRI_server_t*>(v8g->_server), name.c_str());
  } else {
    // check if the other database exists, and increase its refcount
    vocbase = TRI_UseDatabaseServer(static_cast<TRI_server_t*>(v8g->_server),
                                    name.c_str());
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // switch databases
  void* orig = v8g->_vocbase;
  TRI_ASSERT(orig != nullptr);

  v8g->_vocbase = vocbase;
  TRI_ASSERT(orig != vocbase);
  TRI_ReleaseDatabaseServer(static_cast<TRI_server_t*>(v8g->_server),
                            static_cast<TRI_vocbase_t*>(orig));

  TRI_V8_RETURN(WrapVocBase(isolate, vocbase));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all existing databases in a coordinator
////////////////////////////////////////////////////////////////////////////////

static void ListDatabasesCoordinator(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // Arguments are already checked, there are 0 or 3.

  ClusterInfo* ci = ClusterInfo::instance();

  if (args.Length() == 0) {
    std::vector<DatabaseID> list = ci->databases(true);
    v8::Handle<v8::Array> result = v8::Array::New(isolate);
    for (size_t i = 0; i < list.size(); ++i) {
      result->Set((uint32_t)i, TRI_V8_STD_STRING(list[i]));
    }
    TRI_V8_RETURN(result);
  } else {
    // We have to ask a DBServer, any will do:
    int tries = 0;
    std::vector<ServerID> DBServers;
    while (true) {
      DBServers = ci->getCurrentDBServers();

      if (!DBServers.empty()) {
        ServerID sid = DBServers[0];
        ClusterComm* cc = ClusterComm::instance();

        std::unordered_map<std::string, std::string> headers;
        headers["Authentication"] = TRI_ObjectToString(args[2]);
        auto res = cc->syncRequest(
            "", 0, "server:" + sid, arangodb::GeneralRequest::RequestType::GET,
            "/_api/database/user", std::string(""), headers, 0.0);

        if (res->status == CL_COMM_SENT) {
          // We got an array back as JSON, let's parse it and build a v8
          StringBuffer& body = res->result->getBody();

          std::shared_ptr<VPackBuilder> builder =
              VPackParser::fromJson(body.c_str(), body.length());
          VPackSlice resultSlice = builder->slice();

          if (resultSlice.isObject()) {
            VPackSlice r = resultSlice.get("result");
            if (r.isArray()) {
              uint32_t i = 0;
              v8::Handle<v8::Array> result = v8::Array::New(isolate);
              for (auto const& it : VPackArrayIterator(r)) {
                std::string v = it.copyString();
                result->Set(i++, TRI_V8_STD_STRING(v));
              }
              TRI_V8_RETURN(result);
            }
          }
        }
      }
      if (++tries >= 2) {
        break;
      }
      ci->loadCurrentDBServers();  // just in case some new have arrived
    }
    // Give up:
    TRI_V8_RETURN_UNDEFINED();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseListDatabase
////////////////////////////////////////////////////////////////////////////////

static void JS_Databases(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  uint32_t const argc = args.Length();
  if (argc > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._databases()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (argc == 0 && !TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
    ListDatabasesCoordinator(args);
    return;
  }

  TRI_GET_GLOBALS();

  std::vector<std::string> names;

  int res;

  if (argc == 0) {
    // return all databases
    res = TRI_GetDatabaseNamesServer(static_cast<TRI_server_t*>(v8g->_server),
                                     names);
  } else {
    // return all databases for a specific user
    std::string&& username = TRI_ObjectToString(args[0]);

    res = TRI_GetUserDatabasesServer(static_cast<TRI_server_t*>(v8g->_server),
                                     username.c_str(), names);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Array> result = v8::Array::New(isolate, (int)names.size());
  for (size_t i = 0; i < names.size(); ++i) {
    result->Set((uint32_t)i, TRI_V8_STD_STRING(names[i]));
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new database, case of a coordinator in a cluster
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for the agency
///
/// `place` can be "/Target", "/Plan" or "/Current" and name is the database
/// name.
////////////////////////////////////////////////////////////////////////////////

static void CreateDatabaseCoordinator(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // First work with the arguments to create a VelocyPack entry:
  std::string const name = TRI_ObjectToString(args[0]);

  if (!TRI_IsAllowedNameVocBase(false, name.c_str())) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
  }

  uint64_t const id = ClusterInfo::instance()->uniqid();
  VPackBuilder builder;
  try {
    VPackObjectBuilder b(&builder);
    std::string const idString(StringUtils::itoa(id));

    builder.add("id", VPackValue(idString));

    std::string const valueString(TRI_ObjectToString(args[0]));
    builder.add("name", VPackValue(valueString));

    if (args.Length() > 1) {
      VPackBuilder tmpBuilder;
      int res = TRI_V8ToVPack(isolate, tmpBuilder, args[1], false);
      if (res != TRI_ERROR_NO_ERROR) {
        TRI_V8_THROW_EXCEPTION_MEMORY();
      }
      builder.add("options", tmpBuilder.slice());
    }

    std::string const serverId(ServerState::instance()->getId());
    builder.add("coordinator", VPackValue(serverId));
  } catch (VPackException const&) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  ClusterInfo* ci = ClusterInfo::instance();
  std::string errorMsg;

  int res =
      ci->createDatabaseCoordinator(name, builder.slice(), errorMsg, 120.0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, errorMsg);
  }

  // database was created successfully in agency

  TRI_GET_GLOBALS();

  // now wait for heartbeat thread to create the database object
  TRI_vocbase_t* vocbase = nullptr;
  int tries = 0;

  while (++tries <= 6000) {
    vocbase = TRI_UseByIdCoordinatorDatabaseServer(
        static_cast<TRI_server_t*>(v8g->_server), id);

    if (vocbase != nullptr) {
      break;
    }

    // sleep
    usleep(10000);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  // now run upgrade and copy users into context
  if (args.Length() >= 3 && args[2]->IsArray()) {
    v8::Handle<v8::Object> users = v8::Object::New(isolate);
    users->Set(TRI_V8_ASCII_STRING("users"), args[2]);

    isolate->GetCurrentContext()->Global()->Set(
        TRI_V8_ASCII_STRING("UPGRADE_ARGS"), users);
  } else {
    isolate->GetCurrentContext()->Global()->Set(
        TRI_V8_ASCII_STRING("UPGRADE_ARGS"), v8::Object::New(isolate));
  }

  // switch databases
  TRI_vocbase_t* orig = v8g->_vocbase;
  TRI_ASSERT(orig != nullptr);

  v8g->_vocbase = vocbase;

  // initalize database
  bool allowUseDatabase = v8g->_allowUseDatabase;
  v8g->_allowUseDatabase = true;

  V8DealerFeature::DEALER->startupLoader()->executeGlobalScript(
      isolate, isolate->GetCurrentContext(),
      "server/bootstrap/coordinator-database.js");

  v8g->_allowUseDatabase = allowUseDatabase;

  // and switch back
  v8g->_vocbase = orig;

  TRI_ReleaseVocBase(vocbase);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseCreateDatabase
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "db._createDatabase(<name>, <options>, <users>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
  }

  if (!TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  if (ServerState::instance()->isCoordinator()) {
    CreateDatabaseCoordinator(args);
    return;
  }

  TRI_GET_GLOBALS();
  TRI_voc_tick_t id = 0;

  // get database defaults from server
  TRI_vocbase_defaults_t defaults;
  TRI_GetDatabaseDefaultsServer(static_cast<TRI_server_t*>(v8g->_server),
                                &defaults);

  v8::Local<v8::String> keyDefaultMaximalSize =
      TRI_V8_ASCII_STRING("defaultMaximalSize");
  v8::Local<v8::String> keyDefaultWaitForSync =
      TRI_V8_ASCII_STRING("defaultWaitForSync");
  v8::Local<v8::String> keyRequireAuthentication =
      TRI_V8_ASCII_STRING("requireAuthentication");
  v8::Local<v8::String> keyRequireAuthenticationUnixSockets =
      TRI_V8_ASCII_STRING("requireAuthenticationUnixSockets");
  v8::Local<v8::String> keyAuthenticateSystemOnly =
      TRI_V8_ASCII_STRING("authenticateSystemOnly");
  v8::Local<v8::String> keyForceSyncProperties =
      TRI_V8_ASCII_STRING("forceSyncProperties");

  // overwrite database defaults from args[2]
  if (args.Length() > 1 && args[1]->IsObject()) {
    v8::Handle<v8::Object> options = args[1]->ToObject();

    if (options->Has(keyDefaultMaximalSize)) {
      defaults.defaultMaximalSize =
          (TRI_voc_size_t)options->Get(keyDefaultMaximalSize)->IntegerValue();
    }

    if (options->Has(keyDefaultWaitForSync)) {
      defaults.defaultWaitForSync =
          options->Get(keyDefaultWaitForSync)->BooleanValue();
    }

    if (options->Has(keyRequireAuthentication)) {
      defaults.requireAuthentication =
          options->Get(keyRequireAuthentication)->BooleanValue();
    }

    if (options->Has(keyRequireAuthenticationUnixSockets)) {
      defaults.requireAuthenticationUnixSockets =
          options->Get(keyRequireAuthenticationUnixSockets)->BooleanValue();
    }

    if (options->Has(keyAuthenticateSystemOnly)) {
      defaults.authenticateSystemOnly =
          options->Get(keyAuthenticateSystemOnly)->BooleanValue();
    }

    if (options->Has(keyForceSyncProperties)) {
      defaults.forceSyncProperties =
          options->Get(keyForceSyncProperties)->BooleanValue();
    }

    TRI_GET_GLOBAL_STRING(IdKey);
    if (options->Has(IdKey)) {
      // only used for testing to create database with a specific id
      id = TRI_ObjectToUInt64(options->Get(IdKey), true);
    }
  }

  std::string const name = TRI_ObjectToString(args[0]);

  TRI_vocbase_t* database;
  int res =
      TRI_CreateDatabaseServer(static_cast<TRI_server_t*>(v8g->_server), id,
                               name.c_str(), &defaults, &database, true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(database != nullptr);

  database->_deadlockDetector.enabled(
      !arangodb::ServerState::instance()->isRunningInCluster());

  // copy users into context
  if (args.Length() >= 3 && args[2]->IsArray()) {
    v8::Handle<v8::Object> users = v8::Object::New(isolate);
    users->Set(TRI_V8_ASCII_STRING("users"), args[2]);

    isolate->GetCurrentContext()->Global()->Set(
        TRI_V8_ASCII_STRING("UPGRADE_ARGS"), users);
  } else {
    isolate->GetCurrentContext()->Global()->Set(
        TRI_V8_ASCII_STRING("UPGRADE_ARGS"), v8::Object::New(isolate));
  }

  // switch databases
  TRI_vocbase_t* orig = v8g->_vocbase;
  TRI_ASSERT(orig != nullptr);

  v8g->_vocbase = database;

  // initalize database
  V8DealerFeature::DEALER->startupLoader()->executeGlobalScript(
      isolate, isolate->GetCurrentContext(),
      "server/bootstrap/local-database.js");

  // and switch back
  v8g->_vocbase = orig;

  // populate the authentication cache. otherwise no one can access the new
  // database
  TRI_ReloadAuthInfo(database);

  // finally decrease the reference-counter
  TRI_ReleaseVocBase(database);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop a database, case of a coordinator in a cluster
////////////////////////////////////////////////////////////////////////////////

static void DropDatabaseCoordinator(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  // Arguments are already checked, there is exactly one argument
  std::string const name = TRI_ObjectToString(args[0]);
  TRI_vocbase_t* vocbase = TRI_UseCoordinatorDatabaseServer(
      static_cast<TRI_server_t*>(v8g->_server), name.c_str());

  if (vocbase == nullptr) {
    // no such database
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_voc_tick_t const id = vocbase->_id;
  TRI_ReleaseVocBase(vocbase);

  ClusterInfo* ci = ClusterInfo::instance();
  std::string errorMsg;

  // clear local sid cache for database
  arangodb::VocbaseContext::clearSid(name);

  int res = ci->dropDatabaseCoordinator(name, errorMsg, 120.0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, errorMsg);
  }

  // now wait for heartbeat thread to drop the database object
  int tries = 0;

  while (++tries <= 6000) {
    TRI_vocbase_t* vocbase = TRI_UseByIdCoordinatorDatabaseServer(
        static_cast<TRI_server_t*>(v8g->_server), id);

    if (vocbase == nullptr) {
      // object has vanished
      break;
    }

    TRI_ReleaseVocBase(vocbase);
    // sleep
    usleep(10000);
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseDropDatabase
////////////////////////////////////////////////////////////////////////////////

static void JS_DropDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._dropDatabase(<name>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (!TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  // clear collections in cache object
  TRI_ClearObjectCacheV8(isolate);

  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
    DropDatabaseCoordinator(args);
    return;
  }

  std::string const name = TRI_ObjectToString(args[0]);
  TRI_GET_GLOBALS();

  int res = TRI_DropDatabaseServer(static_cast<TRI_server_t*>(v8g->_server),
                                   name.c_str(), true, true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // clear local sid cache for the database
  arangodb::VocbaseContext::clearSid(name);

  // run the garbage collection in case the database held some objects which can
  // now be freed
  TRI_RunGarbageCollectionV8(isolate, 0.25);

  TRI_V8ReloadRouting(isolate);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list of all endpoints
///
/// @FUN{ENDPOINTS}
////////////////////////////////////////////////////////////////////////////////

static void JS_Endpoints(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._endpoints()");
  }

  auto server =
      application_features::ApplicationServer::getFeature<HttpEndpointProvider>(
          "Endpoint");

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (!TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  v8::Handle<v8::Array> result = v8::Array::New(isolate);
  uint32_t j = 0;

  for (auto const& it : server->httpEndpoints()) {
    v8::Handle<v8::Object> item = v8::Object::New(isolate);
    item->Set(TRI_V8_ASCII_STRING("endpoint"), TRI_V8_STD_STRING(it));

    result->Set(j++, item);
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_ClearTimers(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::basics::Timers::clear();

  TRI_V8_RETURN(v8::Undefined(isolate));
  TRI_V8_TRY_CATCH_END
}

static void JS_GetTimers(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Handle<v8::Object> totals = v8::Object::New(isolate);
  v8::Handle<v8::Object> counts = v8::Object::New(isolate);

  for (auto& it : arangodb::basics::Timers::get()) {
    totals->ForceSet(TRI_V8_STD_STRING(it.first),
                     v8::Number::New(isolate, it.second.first));
    counts->ForceSet(
        TRI_V8_STD_STRING(it.first),
        v8::Number::New(isolate, static_cast<double>(it.second.second)));
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->ForceSet(TRI_V8_ASCII_STRING("totals"), totals);
  result->ForceSet(TRI_V8_ASCII_STRING("counts"), counts);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_TrustedProxies(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  
  if (RestServerFeature::hasProxyCheck()) {
    v8::Handle<v8::Array> result = v8::Array::New(isolate);

    uint32_t i = 0;
    for (auto const& proxyDef: RestServerFeature::getTrustedProxies()) {
      result->Set(i++, TRI_V8_STD_STRING(proxyDef));
    }
    TRI_V8_RETURN(result);
  } else {
    TRI_V8_RETURN(v8::Null(isolate));
  }
  
  TRI_V8_TRY_CATCH_END
}

static void JS_AuthenticationEnabled(v8::FunctionCallbackInfo<v8::Value> const& args) {
  // mop: one could argue that this is a function because this might be changable on the fly
  // at some time but the sad truth is server startup order :S v8 is initialized after RestServerFeature
  // :weglaecheln:
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Handle<v8::Boolean> result = v8::Boolean::New(isolate, RestServerFeature::authenticationEnabled());
  
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the private WRP_VOCBASE_COL_TYPE value
////////////////////////////////////////////////////////////////////////////////

int32_t TRI_GetVocBaseColType() { return WRP_VOCBASE_COL_TYPE; }

////////////////////////////////////////////////////////////////////////////////
/// @brief run version check
////////////////////////////////////////////////////////////////////////////////

bool TRI_UpgradeDatabase(TRI_vocbase_t* vocbase,
                         v8::Handle<v8::Context> context) {
  auto isolate = context->GetIsolate();

  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();
  TRI_vocbase_t* orig = v8g->_vocbase;
  v8g->_vocbase = vocbase;

  auto startupLoader = V8DealerFeature::DEALER->startupLoader();
  v8::Handle<v8::Value> result = startupLoader->executeGlobalScript(
      isolate, isolate->GetCurrentContext(), "server/upgrade-database.js");
  bool ok = TRI_ObjectToBoolean(result);

  if (!ok) {
    vocbase->_state = (sig_atomic_t)TRI_VOCBASE_STATE_FAILED_VERSION;
  }

  v8g->_vocbase = orig;

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run upgrade check
////////////////////////////////////////////////////////////////////////////////

int TRI_CheckDatabaseVersion(TRI_vocbase_t* vocbase,
                             v8::Handle<v8::Context> context) {
  auto isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();
  TRI_vocbase_t* orig = v8g->_vocbase;
  v8g->_vocbase = vocbase;

  auto startupLoader = V8DealerFeature::DEALER->startupLoader();
  v8::Handle<v8::Value> result = startupLoader->executeGlobalScript(
      isolate, isolate->GetCurrentContext(), "server/check-version.js");
  int code = (int)TRI_ObjectToInt64(result);

  v8g->_vocbase = orig;

  return code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads routing
////////////////////////////////////////////////////////////////////////////////

void TRI_V8ReloadRouting(v8::Isolate* isolate) {
  TRI_ExecuteJavaScriptString(
      isolate, isolate->GetCurrentContext(),
      TRI_V8_ASCII_STRING(
          "require('internal').executeGlobalContextFunction('reloadRouting')"),
      TRI_V8_ASCII_STRING("reload routing"), false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8VocBridge(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                         arangodb::aql::QueryRegistry* queryRegistry,
                         TRI_server_t* server, TRI_vocbase_t* vocbase,
                         size_t threadNumber) {
  v8::HandleScope scope(isolate);

  // check the isolate
  TRI_GET_GLOBALS();

  TRI_ASSERT(v8g->_transactionContext == nullptr);
  v8g->_transactionContext = new V8TransactionContext(vocbase, true);
  static_cast<V8TransactionContext*>(v8g->_transactionContext)->makeGlobal();

  // register the query registry
  TRI_ASSERT(queryRegistry != nullptr);
  v8g->_queryRegistry = queryRegistry;

  // register the server
  v8g->_server = server;

  // register the database
  v8g->_vocbase = vocbase;

  v8::Handle<v8::ObjectTemplate> ArangoNS;
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;
  v8::Handle<v8::Template> pt;

  // .............................................................................
  // generate the TRI_vocbase_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoDatabase"));

  ArangoNS = ft->InstanceTemplate();
  ArangoNS->SetInternalFieldCount(2);
  ArangoNS->SetNamedPropertyHandler(MapGetVocBase);

  // for any database function added here, be sure to add it to in function
  // JS_CompletionsVocbase, too for the auto-completion

  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_version"),
                       JS_VersionServer);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_id"),
                       JS_IdDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_isSystem"),
                       JS_IsSystemDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_name"),
                       JS_NameDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_path"),
                       JS_PathDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS,
                       TRI_V8_ASCII_STRING("_createDatabase"),
                       JS_CreateDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_dropDatabase"),
                       JS_DropDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_databases"),
                       JS_Databases);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_useDatabase"),
                       JS_UseDatabase);

  TRI_InitV8Statistics(isolate, context);

  TRI_InitV8indexArangoDB(isolate, ArangoNS);

  TRI_InitV8collection(context, server, vocbase, threadNumber, v8g, isolate,
                       ArangoNS);

  v8g->VocbaseTempl.Reset(isolate, ArangoNS);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ArangoDatabase"),
                               ft->GetFunction());

  arangodb::V8VPackWrapper::initialize(isolate, context, v8g);

  TRI_InitV8cursor(context, v8g);

  // .............................................................................
  // generate global functions
  // .............................................................................

  // AQL functions. not intended to be used directly by end users
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("AQL_EXECUTE"),
                               JS_ExecuteAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("AQL_EXECUTEJSON"),
                               JS_ExecuteAqlJson, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("AQL_EXPLAIN"),
                               JS_ExplainAql, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("AQL_PARSE"), JS_ParseAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("AQL_WARNING"),
                               JS_WarningAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("AQL_QUERIES_PROPERTIES"),
                               JS_QueriesPropertiesAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("AQL_QUERIES_CURRENT"),
                               JS_QueriesCurrentAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("AQL_QUERIES_SLOW"),
                               JS_QueriesSlowAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("AQL_QUERIES_KILL"),
                               JS_QueriesKillAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("AQL_QUERY_SLEEP"),
                               JS_QuerySleepAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("AQL_QUERY_IS_KILLED"),
                               JS_QueryIsKilledAql, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("AQL_QUERY_CACHE_PROPERTIES"),
      JS_QueryCachePropertiesAql, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("AQL_QUERY_CACHE_INVALIDATE"),
      JS_QueryCacheInvalidateAql, true);
  
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("OBJECT_HASH"),
                               JS_ObjectHash, true);

  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("THROW_COLLECTION_NOT_LOADED"),
      JS_ThrowCollectionNotLoaded, true);

  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("CPP_SHORTEST_PATH"),
                               JS_QueryShortestPath, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("CPP_NEIGHBORS"),
                               JS_QueryNeighbors, true);

  TRI_InitV8Replication(isolate, context, server, vocbase, threadNumber, v8g);

  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("COMPARE_STRING"),
                               JS_CompareString);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("NORMALIZE_STRING"),
                               JS_NormalizeString);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("TIMEZONES"), JS_GetIcuTimezones);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("LOCALES"),
                               JS_GetIcuLocales);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("FORMAT_DATETIME"),
                               JS_FormatDatetime);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("PARSE_DATETIME"),
                               JS_ParseDatetime);

  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ENDPOINTS"),
                               JS_Endpoints, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("RELOAD_AUTH"),
                               JS_ReloadAuth, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("TRANSACTION"),
                               JS_Transaction, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("WAL_FLUSH"), JS_FlushWal, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("WAL_WAITCOLLECTOR"),
                               JS_WaitCollectorWal, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("WAL_PROPERTIES"),
                               JS_PropertiesWal, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("WAL_TRANSACTIONS"),
                               JS_TransactionsWal, true);

  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ENABLE_NATIVE_BACKTRACES"),
                               JS_EnableNativeBacktraces, true);

  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("Debug"),
                               JS_Debug, true);

  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("CLEAR_TIMERS"),
                               JS_ClearTimers, true);

  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("GET_TIMERS"), JS_GetTimers, true);

  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("AUTHENTICATION_ENABLED"), JS_AuthenticationEnabled, true);
  
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("TRUSTED_PROXIES"), JS_TrustedProxies, true);
  // .............................................................................
  // create global variables
  // .............................................................................

  v8::Handle<v8::Object> v = WrapVocBase(isolate, vocbase);
  if (v.IsEmpty()) {
    LOG(ERR) << "out of memory when initializing VocBase";
  } else {
    TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("db"),
                                 v);
  }

  // add collections cache object
  context->Global()->ForceSet(TRI_V8_ASCII_STRING("__dbcache__"),
                              v8::Object::New(isolate), v8::DontEnum);

  // current thread number
  context->Global()->ForceSet(TRI_V8_ASCII_STRING("THREAD_NUMBER"),
                              v8::Number::New(isolate, (double)threadNumber),
                              v8::ReadOnly);
  
  // whether or not statistics are enabled
  context->Global()->ForceSet(
      TRI_V8_ASCII_STRING("ENABLE_STATISTICS"),
      v8::Boolean::New(isolate,
                       StatisticsFeature::enabled()));  //, v8::ReadOnly);

  // a thread-global variable that will is supposed to contain the AQL module
  // do not remove this, otherwise AQL queries will break
  context->Global()->ForceSet(TRI_V8_ASCII_STRING("_AQL"),
                              v8::Undefined(isolate), v8::DontEnum);
}
