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

#include <unicode/dtfmtsym.h>
#include <unicode/smpdtfmt.h>
#include <unicode/timezone.h>

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <v8.h>
#include <iostream>
#include <thread>

#include "Agency/State.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "Aql/Query.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryExecutionState.h"
#include "Aql/QueryList.h"
#include "Aql/QueryRegistry.h"
#include "Aql/QueryString.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/Utf8Helper.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Rest/Version.h"
#include "RestServer/ConsoleThread.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/V8Context.h"
#include "Utils/ExecContext.h"
#include "V8/JSLoader.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-helper.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-collection.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-replication.h"
#include "V8Server/v8-statistics.h"
#include "V8Server/v8-users.h"
#include "V8Server/v8-views.h"
#include "V8Server/v8-voccursor.h"
#include "V8Server/v8-vocindex.h"
#include "V8Server/v8-general-graph.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/Methods/Transactions.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

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

  // check if we have some transaction object
  if (args.Length() != 1 || !args[0]->IsObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("TRANSACTION(<object>)");
  }

  // filled by function
  v8::Handle<v8::Value> result;
  v8::TryCatch tryCatch;
  Result rv = executeTransactionJS(isolate, args[0], result, tryCatch);

  // do not rethrow if already canceled
  if (isContextCanceled(isolate)){
    TRI_V8_RETURN(result);
  }

  // has caught and could not be converted to arangoError
  // otherwise it would have been reseted
  if (tryCatch.HasCaught()) {
    tryCatch.ReThrow();
    return;
  }

  if (rv.fail()) {
    THROW_ARANGO_EXCEPTION(rv);
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

  v8::Local<v8::String> name(TRI_V8_ASCII_STRING(isolate, "debug loop"));
  v8::Local<v8::String> debug(TRI_V8_ASCII_STRING(isolate, "debug"));

  v8::Local<v8::Object> callerScope;
  if (args.Length() >= 1) {
    TRI_AddGlobalVariableVocbase(isolate, debug,
                                 args[0]);
  }

  MUTEX_LOCKER(mutexLocker, ConsoleThread::serverConsoleMutex);
  V8LineEditor* console = ConsoleThread::serverConsole;

  if (console != nullptr) {
    while (true) {
      ShellBase::EofType eof;
      std::string input = console->prompt("debug> ", "debug>", eof);

      if (eof == ShellBase::EOF_FORCE_ABORT) {
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
                                    TRI_V8_STD_STRING(isolate, input), name, true);

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
      result->Set((uint32_t)i, TRI_V8_PAIR_STRING(isolate, str, resultLength));
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

      result->Set((uint32_t)i, TRI_V8_PAIR_STRING(isolate, str, strlen(str)));
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

  TimeZone* tz = nullptr;
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

  TimeZone* tz = nullptr;
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
/// @brief parses an AQL query
////////////////////////////////////////////////////////////////////////////////

static void JS_ParseAql(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_PARSE(<querystring>)");
  }

  // get the query string
  if (!args[0]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <querystring>");
  }

  std::string const queryString(TRI_ObjectToString(args[0]));
  // If we execute an AQL query from V8 we need to unset the nolock headers
  arangodb::aql::Query query(
    true,
    vocbase,
    aql::QueryString(queryString),
    nullptr,
    nullptr,
    arangodb::aql::PART_MAIN
  );
  auto parseResult = query.parse();

  if (parseResult.code != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_FULL(parseResult.code, parseResult.details);
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING(isolate, "parsed"), v8::True(isolate));

  {
    v8::Handle<v8::Array> collections = v8::Array::New(isolate);
    result->Set(TRI_V8_ASCII_STRING(isolate, "collections"), collections);
    uint32_t i = 0;
    for (auto& elem : parseResult.collectionNames) {
      collections->Set(i++, TRI_V8_STD_STRING(isolate, (elem)));
    }
  }

  {
    v8::Handle<v8::Array> bindVars = v8::Array::New(isolate);
    uint32_t i = 0;
    for (auto const& elem : parseResult.bindParameters) {
      bindVars->Set(i++, TRI_V8_STD_STRING(isolate, (elem)));
    }
    result->Set(TRI_V8_ASCII_STRING(isolate, "parameters"), bindVars); // parameters is deprecated
    result->Set(TRI_V8_ASCII_STRING(isolate, "bindVars"), bindVars);
  }

  result->Set(TRI_V8_ASCII_STRING(isolate, "ast"),
              TRI_VPackToV8(isolate, parseResult.result->slice()));

  if (parseResult.extra == nullptr || !parseResult.extra->slice().hasKey("warnings")) {
    result->Set(TRI_V8_ASCII_STRING(isolate, "warnings"), v8::Array::New(isolate));
  } else {
    result->Set(TRI_V8_ASCII_STRING(isolate, "warnings"),
                TRI_VPackToV8(isolate, parseResult.extra->slice().get("warnings")));
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
    std::string const message = TRI_ObjectToString(args[1]);

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
  auto& vocbase = GetContextVocBase(isolate);

  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "AQL_EXPLAIN(<queryString>, <bindVars>, <options>)");
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
  arangodb::aql::Query query(
    true,
    vocbase,
    aql::QueryString(queryString),
    bindVars,
    options,
    arangodb::aql::PART_MAIN
  );
  auto queryResult = query.explain();

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_FULL(queryResult.code, queryResult.details);
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  if (queryResult.result != nullptr) {
    if (query.queryOptions().allPlans) {
      result->Set(TRI_V8_ASCII_STRING(isolate, "plans"),
                  TRI_VPackToV8(isolate, queryResult.result->slice()));
    } else {
      result->Set(TRI_V8_ASCII_STRING(isolate, "plan"),
                  TRI_VPackToV8(isolate, queryResult.result->slice()));
      result->Set(TRI_V8_ASCII_STRING(isolate, "cacheable"),
                  v8::Boolean::New(isolate, queryResult.cached));
    }
    
    if (queryResult.extra != nullptr) {
      VPackSlice warnings = queryResult.extra->slice().get("warnings");
      if (warnings.isNone()) {
        result->Set(TRI_V8_ASCII_STRING(isolate, "warnings"), v8::Array::New(isolate));
      } else {
        result->Set(TRI_V8_ASCII_STRING(isolate, "warnings"),
                    TRI_VPackToV8(isolate, queryResult.extra->slice().get("warnings")));
      }
      VPackSlice stats = queryResult.extra->slice().get("stats");
      if (stats.isNone()) {
        result->Set(TRI_V8_ASCII_STRING(isolate, "stats"), v8::Object::New(isolate));
      } else {
        result->Set(TRI_V8_ASCII_STRING(isolate, "stats"), TRI_VPackToV8(isolate, stats));
      }
    } else {
      result->Set(TRI_V8_ASCII_STRING(isolate, "warnings"), v8::Array::New(isolate));
      result->Set(TRI_V8_ASCII_STRING(isolate, "stats"), v8::Object::New(isolate));
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
  auto& vocbase = GetContextVocBase(isolate);

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
  arangodb::aql::Query query(
    true,
    vocbase,
    queryBuilder,
    options,
    arangodb::aql::PART_MAIN
  );
  aql::QueryResult queryResult = query.executeSync(static_cast<arangodb::aql::QueryRegistry*>(v8g->_queryRegistry));

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_FULL(queryResult.code, queryResult.details);
  }

  // return the array value as it is. this is a performance optimization
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  if (queryResult.result != nullptr) {
    result->ForceSet(TRI_V8_ASCII_STRING(isolate, "json"),
                     TRI_VPackToV8(isolate, queryResult.result->slice(),
                                   queryResult.context->getVPackOptions()));
  }
  if (queryResult.extra != nullptr) {
    VPackSlice stats = queryResult.extra->slice().get("stats");
    if (!stats.isNone()) {
      result->ForceSet(TRI_V8_ASCII_STRING(isolate, "stats"),
                       TRI_VPackToV8(isolate, stats));
    }
    VPackSlice profile = queryResult.extra->slice().get("profile");
    if (!profile.isNone()) {
      result->ForceSet(TRI_V8_ASCII_STRING(isolate, "profile"),
                       TRI_VPackToV8(isolate, profile));
    }
  }
  
  if (queryResult.extra == nullptr || !queryResult.extra->slice().hasKey("warnings")) {
    result->Set(TRI_V8_ASCII_STRING(isolate, "warnings"), v8::Array::New(isolate));
  } else {
    result->Set(TRI_V8_ASCII_STRING(isolate, "warnings"),
                TRI_VPackToV8(isolate, queryResult.extra->slice().get("warnings")));
  }
  result->ForceSet(TRI_V8_ASCII_STRING(isolate, "cached"),
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
  auto& vocbase = GetContextVocBase(isolate);

  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "AQL_EXECUTE(<queryString>, <bindVars>, <options>)");
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
  }

  // bind parameters will be freed by the query later
  TRI_GET_GLOBALS();
  arangodb::aql::Query query(
    true,
    vocbase,
    aql::QueryString(queryString),
    bindVars,
    options,
    arangodb::aql::PART_MAIN
  );

  std::shared_ptr<arangodb::aql::SharedQueryState> ss = query.sharedState();
  ss->setContinueCallback();
  
  aql::QueryResultV8 queryResult;
  while (true) {
    auto state = query.executeV8(isolate, static_cast<arangodb::aql::QueryRegistry*>(v8g->_queryRegistry), queryResult);
    if (state != aql::ExecutionState::WAITING) {
      break;
    }
    ss->waitForAsyncResponse();
  }

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

  if (!queryResult.result.IsEmpty()) {
    result->ForceSet(TRI_V8_ASCII_STRING(isolate, "json"), queryResult.result);
  }

  if (queryResult.extra != nullptr) {
    VPackSlice stats = queryResult.extra->slice().get("stats");
    if (!stats.isNone()) {
      result->ForceSet(TRI_V8_ASCII_STRING(isolate, "stats"),
                       TRI_VPackToV8(isolate, stats));
    }
    VPackSlice warnings = queryResult.extra->slice().get("warnings");
    if (warnings.isNone()) {
      result->ForceSet(TRI_V8_ASCII_STRING(isolate, "warnings"), v8::Array::New(isolate));
    } else {
      result->ForceSet(TRI_V8_ASCII_STRING(isolate, "warnings"), TRI_VPackToV8(isolate,warnings));
    }
    VPackSlice profile = queryResult.extra->slice().get("profile");
    if (!profile.isNone()) {
      result->ForceSet(TRI_V8_ASCII_STRING(isolate, "profile"),
                       TRI_VPackToV8(isolate, profile));
    }
    VPackSlice plan = queryResult.extra->slice().get("plan");
    if (!plan.isNone()) {
      result->ForceSet(TRI_V8_ASCII_STRING(isolate, "plan"),
                       TRI_VPackToV8(isolate, plan));
    }
  }
  
  result->ForceSet(TRI_V8_ASCII_STRING(isolate, "cached"),
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
  auto& vocbase = GetContextVocBase(isolate);
  auto* queryList = vocbase.queryList();
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
    if (obj->Has(TRI_V8_ASCII_STRING(isolate, "enabled"))) {
      queryList->enabled(
          TRI_ObjectToBoolean(obj->Get(TRI_V8_ASCII_STRING(isolate, "enabled"))));
    }
    if (obj->Has(TRI_V8_ASCII_STRING(isolate, "trackSlowQueries"))) {
      queryList->trackSlowQueries(TRI_ObjectToBoolean(
          obj->Get(TRI_V8_ASCII_STRING(isolate, "trackSlowQueries"))));
    }
    if (obj->Has(TRI_V8_ASCII_STRING(isolate, "trackBindVars"))) {
      queryList->trackBindVars(TRI_ObjectToBoolean(
          obj->Get(TRI_V8_ASCII_STRING(isolate, "trackBindVars"))));
    }
    if (obj->Has(TRI_V8_ASCII_STRING(isolate, "maxSlowQueries"))) {
      queryList->maxSlowQueries(static_cast<size_t>(
          TRI_ObjectToInt64(obj->Get(TRI_V8_ASCII_STRING(isolate, "maxSlowQueries")))));
    }
    if (obj->Has(TRI_V8_ASCII_STRING(isolate, "slowQueryThreshold"))) {
      queryList->slowQueryThreshold(TRI_ObjectToDouble(
          obj->Get(TRI_V8_ASCII_STRING(isolate, "slowQueryThreshold"))));
    }
    if (obj->Has(TRI_V8_ASCII_STRING(isolate, "slowStreamingQueryThreshold"))) {
      queryList->slowStreamingQueryThreshold(TRI_ObjectToDouble(
          obj->Get(TRI_V8_ASCII_STRING(isolate, "slowStreamingQueryThreshold"))));
    }
    if (obj->Has(TRI_V8_ASCII_STRING(isolate, "maxQueryStringLength"))) {
      queryList->maxQueryStringLength(static_cast<size_t>(TRI_ObjectToInt64(
          obj->Get(TRI_V8_ASCII_STRING(isolate, "maxQueryStringLength")))));
    }

    // intentionally falls through
  }

  // return current settings
  auto result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING(isolate, "enabled"),
              v8::Boolean::New(isolate, queryList->enabled()));
  result->Set(TRI_V8_ASCII_STRING(isolate, "trackSlowQueries"),
              v8::Boolean::New(isolate, queryList->trackSlowQueries()));
  result->Set(TRI_V8_ASCII_STRING(isolate, "trackBindVars"),
              v8::Boolean::New(isolate, queryList->trackBindVars()));
  result->Set(TRI_V8_ASCII_STRING(isolate, "maxSlowQueries"),
              v8::Number::New(
                  isolate, static_cast<double>(queryList->maxSlowQueries())));
  result->Set(TRI_V8_ASCII_STRING(isolate, "slowQueryThreshold"),
              v8::Number::New(isolate, queryList->slowQueryThreshold()));
  result->Set(TRI_V8_ASCII_STRING(isolate, "slowStreamingQueryThreshold"),
              v8::Number::New(isolate, queryList->slowStreamingQueryThreshold()));
  result->Set(TRI_V8_ASCII_STRING(isolate, "maxQueryStringLength"),
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
  auto& vocbase = GetContextVocBase(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERIES_CURRENT()");
  }

  auto* queryList = vocbase.queryList();
  TRI_ASSERT(queryList != nullptr);

  try {
    auto queries = queryList->listCurrent();

    uint32_t i = 0;
    auto result = v8::Array::New(isolate, static_cast<int>(queries.size()));

    for (auto q : queries) {
      auto timeString = TRI_StringTimeStamp(q.started, false);

      v8::Handle<v8::Object> obj = v8::Object::New(isolate);
      obj->Set(TRI_V8_ASCII_STRING(isolate, "id"), TRI_V8UInt64String<TRI_voc_tick_t>(isolate, q.id));
      obj->Set(TRI_V8_ASCII_STRING(isolate, "query"), TRI_V8_STD_STRING(isolate, q.queryString));
      if (q.bindParameters != nullptr) {
        obj->Set(TRI_V8_ASCII_STRING(isolate, "bindVars"), TRI_VPackToV8(isolate, q.bindParameters->slice()));
      } else {
        obj->Set(TRI_V8_ASCII_STRING(isolate, "bindVars"), v8::Object::New(isolate));
      }
      obj->Set(TRI_V8_ASCII_STRING(isolate, "started"), TRI_V8_STD_STRING(isolate, timeString));
      obj->Set(TRI_V8_ASCII_STRING(isolate, "runTime"),
               v8::Number::New(isolate, q.runTime));
      obj->Set(TRI_V8_ASCII_STRING(isolate, "state"), TRI_V8_STD_STRING(isolate, aql::QueryExecutionState::toString(q.state)));
      obj->Set(TRI_V8_ASCII_STRING(isolate, "stream"), v8::Boolean::New(isolate, q.stream));
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
  auto& vocbase = GetContextVocBase(isolate);
  auto* queryList = vocbase.queryList();
  TRI_ASSERT(queryList != nullptr);

  if (args.Length() == 1) {
    queryList->clearSlow();
    TRI_V8_RETURN_TRUE();
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERIES_SLOW()");
  }

  try {
    auto queries = queryList->listSlow();

    uint32_t i = 0;
    auto result = v8::Array::New(isolate, static_cast<int>(queries.size()));

    for (auto q : queries) {
      auto timeString = TRI_StringTimeStamp(q.started, false);

      v8::Handle<v8::Object> obj = v8::Object::New(isolate);
      obj->Set(TRI_V8_ASCII_STRING(isolate, "id"), TRI_V8UInt64String<TRI_voc_tick_t>(isolate, q.id));
      obj->Set(TRI_V8_ASCII_STRING(isolate, "query"), TRI_V8_STD_STRING(isolate, q.queryString));
      if (q.bindParameters != nullptr) {
        obj->Set(TRI_V8_ASCII_STRING(isolate, "bindVars"), TRI_VPackToV8(isolate, q.bindParameters->slice()));
      } else {
        obj->Set(TRI_V8_ASCII_STRING(isolate, "bindVars"), v8::Object::New(isolate));
      }
      obj->Set(TRI_V8_ASCII_STRING(isolate, "started"), TRI_V8_STD_STRING(isolate, timeString));
      obj->Set(TRI_V8_ASCII_STRING(isolate, "runTime"),
               v8::Number::New(isolate, q.runTime));
      obj->Set(TRI_V8_ASCII_STRING(isolate, "state"), TRI_V8_STD_STRING(isolate, aql::QueryExecutionState::toString(q.state)));
      obj->Set(TRI_V8_ASCII_STRING(isolate, "stream"), v8::Boolean::New(isolate, q.stream));
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
  auto& vocbase = GetContextVocBase(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERIES_KILL(<id>)");
  }

  auto id = TRI_ObjectToUInt64(args[0], true);
  auto* queryList = vocbase.queryList();
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

  if (args.Length() > 1 || (args.Length() == 1 && !args[0]->IsObject())) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERY_CACHE_PROPERTIES(<properties>)");
  }

  auto queryCache = arangodb::aql::QueryCache::instance();
  VPackBuilder builder;

  if (args.Length() == 1) {
    // called with options
    int res = TRI_V8ToVPack(isolate, builder, args[0], false);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    queryCache->properties(builder.slice());
  }

  builder.clear();
  queryCache->toVelocyPack(builder);
  TRI_V8_RETURN(TRI_VPackToV8(isolate, builder.slice()));

  // fetch current configuration and return it
  TRI_V8_TRY_CATCH_END
}

static void JS_QueryCacheQueriesAql(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_QUERY_CACHE_QUERIES()");
  }
  
  auto& vocbase = GetContextVocBase(isolate);

  VPackBuilder builder;
  arangodb::aql::QueryCache::instance()->queriesToVelocyPack(&vocbase, builder);
  TRI_V8_RETURN(TRI_VPackToV8(isolate, builder.slice()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidates the AQL query cache
////////////////////////////////////////////////////////////////////////////////

static void JS_QueryCacheInvalidateAql(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

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
    auto databaseFeature =
        application_features::ApplicationServer::getFeature<DatabaseFeature>(
            "Database");
    bool const value = databaseFeature->throwCollectionNotLoadedError();
    TRI_V8_RETURN(v8::Boolean::New(isolate, value));
  } else if (args.Length() == 1) {
    auto databaseFeature =
        application_features::ApplicationServer::getFeature<DatabaseFeature>(
            "Database");
    databaseFeature->throwCollectionNotLoadedError(
        TRI_ObjectToBoolean(args[0]));
  } else {
    TRI_V8_THROW_EXCEPTION_USAGE("THROW_COLLECTION_NOT_LOADED(<value>)");
  }

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
    std::this_thread::sleep_for(std::chrono::microseconds(10000));

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
                                          TRI_vocbase_t* database) {
  TRI_GET_GLOBALS();

  v8::Handle<v8::Object> result =
      WrapClass(isolate, v8g->VocbaseTempl, WRP_VOCBASE_TYPE, database);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionDatabaseCollectionName
////////////////////////////////////////////////////////////////////////////////

static void MapGetVocBase(v8::Local<v8::String> const name,
                          v8::PropertyCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

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

  // generate a name under which the cached property is stored
  std::string cacheKey(key, keyLength);
  cacheKey.push_back('*');

  v8::Local<v8::String> cacheName = TRI_V8_STD_STRING(isolate, cacheKey);
  v8::Handle<v8::Object> holder = args.Holder()->ToObject();

  if (*key == '_') {
    // special treatment for all properties starting with _
    v8::Local<v8::String> const l = TRI_V8_PAIR_STRING(isolate, key, (int)keyLength);

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
    auto* collection = UnwrapCollection(value);

    // check if the collection is from the same database
    if (collection && &(collection->vocbase()) == &vocbase) {
      // we cannot use collection->getStatusLocked() here, because we
      // have no idea who is calling us (db[...]). The problem is that
      // if we are called from within a JavaScript transaction, the
      // caller may have already acquired the collection's status lock
      // with that transaction. if we now lock again, we may deadlock!
      auto status = collection->status();
      auto cid = collection->id();
      auto internalVersion = collection->internalVersion();

      // check if the collection is still alive
      if (status != TRI_VOC_COL_STATUS_DELETED
          && cid > 0
          && !ServerState::instance()->isCoordinator()) {
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

  std::shared_ptr<arangodb::LogicalCollection> collection;

  if (ServerState::instance()->isCoordinator()) {
    auto* ci = arangodb::ClusterInfo::instance();
    if (ci) {
      collection = ci->getCollectionNT(vocbase.name(), std::string(key));
    }
  } else {
    collection = vocbase.lookupCollection(std::string(key));
  }

  if (collection == nullptr) {
    if (*key == '_') {
      TRI_V8_RETURN(v8::Handle<v8::Value>());
    }

    TRI_V8_RETURN_UNDEFINED();
  }

  v8::Handle<v8::Value> result = WrapCollection(isolate, collection);

  if (result.IsEmpty()) {
    TRI_V8_RETURN_UNDEFINED();
  }

  if (!cacheObject.IsEmpty()) {
    cacheObject->ForceSet(cacheName, result);
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name and capabilities of the storage engine
////////////////////////////////////////////////////////////////////////////////

static void JS_Engine(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // return engine data
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  VPackBuilder builder;
  engine->getCapabilities(builder);

  TRI_V8_RETURN(TRI_VPackToV8(isolate, builder.slice()));

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return statistics for the storage engine
////////////////////////////////////////////////////////////////////////////////

static void JS_EngineStats(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  // return engine data
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  VPackBuilder builder;
  engine->getStatistics(builder);

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder.slice());
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseVersion
////////////////////////////////////////////////////////////////////////////////

static void JS_VersionServer(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  bool details = false;
  if (args.Length() > 0) {
    details = TRI_ObjectToBoolean(args[0]);
  }

  if (!details) {
    // return version string
    TRI_V8_RETURN(TRI_V8_ASCII_STRING(isolate, ARANGODB_VERSION));
  }

  // return version details
  VPackBuilder builder;
  builder.openObject();
  rest::Version::getVPack(builder);
  builder.close();

  TRI_V8_RETURN(TRI_VPackToV8(isolate, builder.slice()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databasePath
////////////////////////////////////////////////////////////////////////////////

static void JS_PathDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  TRI_V8_RETURN_STD_STRING(engine->databasePath(&vocbase));
  TRI_V8_TRY_CATCH_END
}

static void JS_VersionFilenameDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  TRI_V8_RETURN_STD_STRING(engine->versionFilename(vocbase.id()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseId
////////////////////////////////////////////////////////////////////////////////

static void JS_IdDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  TRI_V8_RETURN(TRI_V8UInt64String<TRI_voc_tick_t>(isolate, vocbase.id()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseName
////////////////////////////////////////////////////////////////////////////////

static void JS_NameDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto& n = vocbase.name();

  TRI_V8_RETURN_STD_STRING(n);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseIsSystem
////////////////////////////////////////////////////////////////////////////////

static void JS_IsSystemDatabase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  TRI_V8_RETURN(v8::Boolean::New(isolate, vocbase.isSystem()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fake this method so the interface is similar to the client.
////////////////////////////////////////////////////////////////////////////////

static void JS_FakeFlushCache(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseUseDatabase
////////////////////////////////////////////////////////////////////////////////

static void JS_UseDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._useDatabase(<name>)");
  }

  TRI_GET_GLOBALS();

  if (!v8g->_allowUseDatabase) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  auto databaseFeature =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database");
  std::string const name = TRI_ObjectToString(args[0]);
  auto* vocbase = &GetContextVocBase(isolate);

  if (vocbase->isDropped() && name != StaticStrings::SystemDatabase) {
    // still allow changing back into the _system database even if
    // the current database has been dropped
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // check if the other database exists, and increase its refcount
  vocbase = databaseFeature->useDatabase(name);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_ASSERT(!vocbase->isDangling());

  // switch databases
  void* orig = v8g->_vocbase;
  TRI_ASSERT(orig != nullptr);

  v8g->_vocbase = vocbase;
  static_cast<TRI_vocbase_t*>(orig)->release();

  TRI_V8_RETURN(WrapVocBase(isolate, vocbase));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseListDatabase
////////////////////////////////////////////////////////////////////////////////

static void JS_Databases(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  uint32_t const argc = args.Length();
  if (argc > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._databases()");
  }

  auto& vocbase = GetContextVocBase(isolate);

  if (argc == 0 && !vocbase.isSystem()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  std::string user;

  if (argc > 0) {
    user = TRI_ObjectToString(args[0]);
  }

  std::vector<std::string> names = methods::Databases::list(user);
  v8::Handle<v8::Array> result = v8::Array::New(isolate, (int)names.size());

  for (size_t i = 0; i < names.size(); ++i) {
    result->Set((uint32_t)i, TRI_V8_STD_STRING(isolate, names[i]));
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseCreateDatabase
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "db._createDatabase(<name>, <options>, <users>)");
  }

  auto& vocbase = GetContextVocBase(isolate);

  TRI_ASSERT(!vocbase.isDangling());

  if (!vocbase.isSystem()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  VPackBuilder options;

  if (args.Length() >= 2 && args[1]->IsObject()) {
    TRI_V8ToVPack(isolate, options, args[1], false);
  }

  VPackBuilder users;

  if (args.Length() >= 3 && args[2]->IsArray()) {
    VPackArrayBuilder a(&users);
    v8::Handle<v8::Array> ar = v8::Handle<v8::Array>::Cast(args[2]);

    for (uint32_t i = 0; i < ar->Length(); ++i) {
      v8::Handle<v8::Value> user = ar->Get(i);

      if (!user->IsObject()) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "user is not an object");
      }

      TRI_V8ToVPackSimple(isolate, users, user);
    }
  }

  std::string const dbName = TRI_ObjectToString(args[0]);
  Result res = methods::Databases::create( dbName, users.slice(), options.slice());

  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock databaseDropDatabase
////////////////////////////////////////////////////////////////////////////////

static void JS_DropDatabase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._dropDatabase(<name>)");
  }

  auto& vocbase = GetContextVocBase(isolate);

  if (!vocbase.isSystem()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  ExecContext const* exec = ExecContext::CURRENT;

  if (exec != nullptr && exec->systemAuthLevel() != auth::Level::RW) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  std::string const name = TRI_ObjectToString(args[0]);
  auto res = methods::Databases::drop(&vocbase, name);

  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
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
  auto& vocbase = GetContextVocBase(isolate);

  if (!vocbase.isSystem()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  v8::Handle<v8::Array> result = v8::Array::New(isolate);
  uint32_t j = 0;

  for (auto const& it : server->httpEndpoints()) {
    v8::Handle<v8::Object> item = v8::Object::New(isolate);
    item->Set(TRI_V8_ASCII_STRING(isolate, "endpoint"), TRI_V8_STD_STRING(isolate, it));

    result->Set(j++, item);
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_TrustedProxies(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  if (GeneralServerFeature::hasProxyCheck()) {
    v8::Handle<v8::Array> result = v8::Array::New(isolate);

    uint32_t i = 0;
    for (auto const& proxyDef : GeneralServerFeature::getTrustedProxies()) {
      result->Set(i++, TRI_V8_STD_STRING(isolate, proxyDef));
    }
    TRI_V8_RETURN(result);
  } else {
    TRI_V8_RETURN(v8::Null(isolate));
  }

  TRI_V8_TRY_CATCH_END
}

static void JS_AuthenticationEnabled(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  // mop: one could argue that this is a function because this might be
  // changable on the fly at some time but the sad truth is server startup
  // order
  // v8 is initialized after GeneralServerFeature
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
  auto authentication = application_features::ApplicationServer::getFeature<AuthenticationFeature>(
    "Authentication");

  TRI_ASSERT(authentication != nullptr);

  v8::Handle<v8::Boolean> result =
      v8::Boolean::New(isolate, authentication->isActive());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_LdapEnabled(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
 
#ifdef USE_ENTERPRISE
  auto ldap = application_features::ApplicationServer::getFeature<LdapFeature>(
    "Ldap");
  TRI_ASSERT(ldap != nullptr);
  TRI_V8_RETURN(v8::Boolean::New(isolate, ldap->isEnabled()));
#else
  // LDAP only enabled in enterprise mode
  TRI_V8_RETURN(v8::False(isolate));
#endif  

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decode a _rev time stamp
////////////////////////////////////////////////////////////////////////////////

static void JS_DecodeRev(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("DECODE_REV(<string>)");
  }

  std::string rev = TRI_ObjectToString(args[0]);
  uint64_t revInt = HybridLogicalClock::decodeTimeStamp(rev);
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  if (revInt == UINT64_MAX) {
    result->Set(TRI_V8_ASCII_STRING(isolate, "date"),
                TRI_V8_ASCII_STRING(isolate, "illegal"));
    result->Set(TRI_V8_ASCII_STRING(isolate, "count"),
                v8::Number::New(isolate, 0.0));
  } else {
    uint64_t timeMilli = HybridLogicalClock::extractTime(revInt);
    uint64_t count = HybridLogicalClock::extractCount(revInt);
    
    time_t timeSeconds = timeMilli / 1000;
    uint64_t millis = timeMilli % 1000;
    struct tm date;
#ifdef _WIN32
    gmtime_s(&date, &timeSeconds);
#else
    gmtime_r(&timeSeconds, &date);
#endif
    char buffer[32];
    strftime(buffer, 32, "%Y-%m-%dT%H:%M:%S.000Z", &date);
    buffer[20] = static_cast<char>(millis / 100) + '0';
    buffer[21] = ((millis / 10) % 10) + '0';
    buffer[22] = (millis % 10) + '0';
    buffer[24] = 0;

    result->Set(TRI_V8_ASCII_STRING(isolate, "date"),
                TRI_V8_ASCII_STRING(isolate, buffer));
    result->Set(TRI_V8_ASCII_STRING(isolate, "count"),
                v8::Number::New(isolate, static_cast<double>(count)));
  }

  TRI_V8_RETURN(result);
  
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current context
////////////////////////////////////////////////////////////////////////////////

void JS_ArangoDBContext(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("ARANGODB_CONTEXT()");
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  
  ExecContext const* exec = ExecContext::CURRENT;
  if (exec != nullptr) {
    result->Set(TRI_V8_ASCII_STRING(isolate, "user"),
                TRI_V8_STD_STRING(isolate, exec->user()));
  }

  TRI_V8_RETURN(result);

  TRI_V8_TRY_CATCH_END;
}

/// @brief return a list of all wal files (empty list if not rocksdb)
static void JS_CurrentWalFiles(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  std::vector<std::string> names = engine->currentWalFiles();
  std::sort(names.begin(), names.end());

  // already create an array of the correct size
  uint32_t const n = static_cast<uint32_t>(names.size());
  v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(n));

  for (uint32_t i = 0; i < n; ++i) {
    result->Set(i, TRI_V8_STD_STRING(isolate, names[i]));
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief this is for single server mode to dump an agency
////////////////////////////////////////////////////////////////////////////////

static void JS_AgencyDump(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("AGENCY_DUMP()");
  }

  uint64_t index = 0;
  uint64_t term = 0;
  auto b = arangodb::consensus::State::latestAgencyState(vocbase, index, term);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING(isolate, "index"),
    v8::Number::New(isolate, static_cast<double>(index)));
  result->Set(TRI_V8_ASCII_STRING(isolate, "term"),
    v8::Number::New(isolate, static_cast<double>(term)));
  result->Set(TRI_V8_ASCII_STRING(isolate, "data"),
    TRI_VPackToV8(isolate, b->slice()));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8VocBridge(
    v8::Isolate* isolate,
    v8::Handle<v8::Context> context,
    arangodb::aql::QueryRegistry* queryRegistry,
    TRI_vocbase_t& vocbase,
    size_t threadNumber
) {
  v8::HandleScope scope(isolate);

  // check the isolate
  TRI_GET_GLOBALS();

  TRI_ASSERT(v8g->_transactionContext == nullptr);
  v8g->_transactionContext = new transaction::V8Context(vocbase, true);
  static_cast<transaction::V8Context*>(v8g->_transactionContext)->makeGlobal();

  // register the query registry
  TRI_ASSERT(queryRegistry != nullptr);
  v8g->_queryRegistry = queryRegistry;

  // register the database
  v8g->_vocbase = &vocbase;

  // .............................................................................
  // generate the TRI_vocbase_t template
  // .............................................................................

  v8::Handle<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoDatabase"));

  v8::Handle<v8::ObjectTemplate> ArangoNS = ft->InstanceTemplate();
  ArangoNS->SetInternalFieldCount(2);
  ArangoNS->SetNamedPropertyHandler(MapGetVocBase);

  // for any database function added here, be sure to add it to in function
  // JS_CompletionsVocbase, too for the auto-completion

  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_engine"),
                       JS_Engine);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_engineStats"),
                       JS_EngineStats);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_version"),
                       JS_VersionServer);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_id"),
                       JS_IdDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_isSystem"),
                       JS_IsSystemDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_name"),
                       JS_NameDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_path"),
                       JS_PathDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_currentWalFiles"),
                       JS_CurrentWalFiles, true);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_versionFilename"),
                       JS_VersionFilenameDatabase, true);
  TRI_AddMethodVocbase(isolate, ArangoNS,
                       TRI_V8_ASCII_STRING(isolate, "_createDatabase"),
                       JS_CreateDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_dropDatabase"),
                       JS_DropDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_databases"),
                       JS_Databases);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_useDatabase"),
                       JS_UseDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING(isolate, "_flushCache"),
                       JS_FakeFlushCache, true);
  
  v8g->VocbaseTempl.Reset(isolate, ArangoNS);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "ArangoDatabase"),
                               ft->GetFunction());

  TRI_InitV8Statistics(isolate, context);

  TRI_InitV8IndexArangoDB(isolate, ArangoNS);

  TRI_InitV8Collections(context, &vocbase, v8g, isolate, ArangoNS);
  TRI_InitV8Views(context, &vocbase, v8g, isolate, ArangoNS);
  TRI_InitV8Users(context, &vocbase, v8g, isolate);
  TRI_InitV8GeneralGraph(context, &vocbase, v8g, isolate);

  TRI_InitV8cursor(context, v8g);

  // .............................................................................
  // generate global functions
  // .............................................................................

  // AQL functions. not intended to be used directly by end users
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "AQL_EXECUTE"),
                               JS_ExecuteAql, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "AQL_EXECUTEJSON"),
                               JS_ExecuteAqlJson, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "AQL_EXPLAIN"),
                               JS_ExplainAql, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "AQL_PARSE"), JS_ParseAql, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "AQL_WARNING"),
                               JS_WarningAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING(isolate, "AQL_QUERIES_PROPERTIES"),
                               JS_QueriesPropertiesAql, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "AQL_QUERIES_CURRENT"),
                               JS_QueriesCurrentAql, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "AQL_QUERIES_SLOW"),
                               JS_QueriesSlowAql, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "AQL_QUERIES_KILL"),
                               JS_QueriesKillAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING(isolate, "AQL_QUERY_SLEEP"),
                               JS_QuerySleepAql, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "AQL_QUERY_IS_KILLED"),
                               JS_QueryIsKilledAql, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "AQL_QUERY_CACHE_PROPERTIES"),
      JS_QueryCachePropertiesAql, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "AQL_QUERY_CACHE_QUERIES"),
      JS_QueryCacheQueriesAql, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "AQL_QUERY_CACHE_INVALIDATE"),
      JS_QueryCacheInvalidateAql, true);

  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING(isolate, "OBJECT_HASH"),
                               JS_ObjectHash, true);

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "THROW_COLLECTION_NOT_LOADED"),
      JS_ThrowCollectionNotLoaded, true);

  TRI_InitV8Replication(isolate, context, &vocbase, threadNumber, v8g);

  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING(isolate, "COMPARE_STRING"),
                               JS_CompareString);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "NORMALIZE_STRING"),
                               JS_NormalizeString);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "TIMEZONES"), JS_GetIcuTimezones);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "LOCALES"),
                               JS_GetIcuLocales);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING(isolate, "FORMAT_DATETIME"),
                               JS_FormatDatetime);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING(isolate, "PARSE_DATETIME"),
                               JS_ParseDatetime);

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "ENDPOINTS"), JS_Endpoints, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "TRANSACTION"),
                               JS_Transaction, true);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "ENABLE_NATIVE_BACKTRACES"),
                               JS_EnableNativeBacktraces, true);

  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "Debug"),
                               JS_Debug, true);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "AUTHENTICATION_ENABLED"),
                               JS_AuthenticationEnabled, true);
  
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "LDAP_ENABLED"),
                               JS_LdapEnabled, true);

  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING(isolate, "TRUSTED_PROXIES"),
                               JS_TrustedProxies, true);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "DECODE_REV"),
                               JS_DecodeRev, true);

  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING(isolate, "ARANGODB_CONTEXT"),
                               JS_ArangoDBContext,
                               true);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "AGENCY_DUMP"),
                               JS_AgencyDump, true);

  // .............................................................................
  // create global variables
  // .............................................................................

  v8::Handle<v8::Object> v = WrapVocBase(isolate, &vocbase);

  if (v.IsEmpty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "out of memory when initializing VocBase";
    FATAL_ERROR_ABORT();
  }

  TRI_AddGlobalVariableVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "db"), v);

  // add collections cache object
  context->Global()->ForceSet(TRI_V8_ASCII_STRING(isolate, "__dbcache__"),
                              v8::Object::New(isolate), v8::DontEnum);

  // current thread number
  context->Global()->ForceSet(TRI_V8_ASCII_STRING(isolate, "THREAD_NUMBER"),
                              v8::Number::New(isolate, (double)threadNumber),
                              v8::ReadOnly);

  // whether or not statistics are enabled
  context->Global()->ForceSet(
      TRI_V8_ASCII_STRING(isolate, "ENABLE_STATISTICS"),
      v8::Boolean::New(isolate,
                       StatisticsFeature::enabled()));  //, v8::ReadOnly);

  // a thread-global variable that will is supposed to contain the AQL module
  // do not remove this, otherwise AQL queries will break
  context->Global()->ForceSet(TRI_V8_ASCII_STRING(isolate, "_AQL"),
                              v8::Undefined(isolate), v8::DontEnum);
}
