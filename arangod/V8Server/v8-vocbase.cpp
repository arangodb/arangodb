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
#include "v8-wrapshapedjson.h"
#include "v8-replication.h"
#include "v8-vocindex.h"
#include "v8-collection.h"

#include "VocBase/general-cursor.h"
#include "v8-voccursor.h"

#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Utf8Helper.h"

#include "Basics/conversions.h"
#include "Basics/json-utilities.h"
#include "Utils/transactions.h"
#include "Utils/V8ResolverGuard.h"

#include "HttpServer/ApplicationEndpointServer.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "Wal/LogfileManager.h"

#include "VocBase/auth.h"
#include "v8.h"
#include "V8/JSLoader.h"

#include "Cluster/AgencyComm.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"

#include "unicode/timezone.h"
#include "unicode/smpdtfmt.h"
#include "unicode/dtfmtsym.h"


using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

extern bool TRI_ENABLE_STATISTICS;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                  HELPER FUNCTIONS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a C++ into a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static v8::Handle<v8::Object> WrapClass (v8::Isolate *isolate,
                                         v8::Persistent<v8::ObjectTemplate>& classTempl,
                                         int32_t type,
                                         T* y) {
  v8::EscapableHandleScope scope(isolate);

  auto localClassTemplate = v8::Local<v8::ObjectTemplate>::New(isolate, classTempl);
  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = localClassTemplate->NewInstance();

  if (result.IsEmpty()) {
    // error
    return scope.Escape<v8::Object>(result);
  }

  // set the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(isolate, type));
  result->SetInternalField(SLOT_CLASS,      v8::External::New(isolate, y));

  return scope.Escape<v8::Object>(result);
}


// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a transaction
////////////////////////////////////////////////////////////////////////////////

static void JS_Transaction (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || ! args[0]->IsObject()) {
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
  double lockTimeout = (double) (TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);

  if (object->Has(TRI_V8_ASCII_STRING("lockTimeout"))) {
    static const string timeoutError = "<lockTimeout> must be a valid numeric value";

    if (! object->Get(TRI_V8_ASCII_STRING("lockTimeout"))->IsNumber()) {
      TRI_V8_THROW_EXCEPTION_PARAMETER(timeoutError);
    }

    lockTimeout = TRI_ObjectToDouble(object->Get(TRI_V8_ASCII_STRING("lockTimeout")));

    if (lockTimeout < 0.0) {
      TRI_V8_THROW_EXCEPTION_PARAMETER(timeoutError);
    }
  }

  // "waitForSync"
  bool waitForSync = false;

  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL_STRING(WaitForSyncKey);
  if (object->Has(WaitForSyncKey)) {
    if (! object->Get(WaitForSyncKey)->IsBoolean() &&
        ! object->Get(WaitForSyncKey)->IsBooleanObject()) {
      TRI_V8_THROW_EXCEPTION_PARAMETER("<waitForSync> must be a boolean value");
    }

    waitForSync = TRI_ObjectToBoolean(WaitForSyncKey);
  }

  // "collections"
  static string const collectionError = "missing/invalid collections definition for transaction";

  if (! object->Has(TRI_V8_ASCII_STRING("collections")) || ! object->Get(TRI_V8_ASCII_STRING("collections"))->IsObject()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(collectionError);
  }

  // extract collections
  v8::Handle<v8::Array> collections = v8::Handle<v8::Array>::Cast(object->Get(TRI_V8_ASCII_STRING("collections")));

  if (collections.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(collectionError);
  }

  bool isValid = true;
  vector<string> readCollections;
  vector<string> writeCollections;

  // collections.read
  if (collections->Has(TRI_V8_ASCII_STRING("read"))) {
    if (collections->Get(TRI_V8_ASCII_STRING("read"))->IsArray()) {
      v8::Handle<v8::Array> names = v8::Handle<v8::Array>::Cast(collections->Get(TRI_V8_ASCII_STRING("read")));

      for (uint32_t i = 0 ; i < names->Length(); ++i) {
        v8::Handle<v8::Value> collection = names->Get(i);
        if (! collection->IsString()) {
          isValid = false;
          break;
        }

        readCollections.push_back(TRI_ObjectToString(collection));
      }
    }
    else if (collections->Get(TRI_V8_ASCII_STRING("read"))->IsString()) {
      readCollections.push_back(TRI_ObjectToString(collections->Get(TRI_V8_ASCII_STRING("read"))));
    }
    else {
      isValid = false;
    }
  }

  // collections.write
  if (collections->Has(TRI_V8_ASCII_STRING("write"))) {
    if (collections->Get(TRI_V8_ASCII_STRING("write"))->IsArray()) {
      v8::Handle<v8::Array> names = v8::Handle<v8::Array>::Cast(collections->Get(TRI_V8_ASCII_STRING("write")));

      for (uint32_t i = 0 ; i < names->Length(); ++i) {
        v8::Handle<v8::Value> collection = names->Get(i);
        if (! collection->IsString()) {
          isValid = false;
          break;
        }

        writeCollections.push_back(TRI_ObjectToString(collection));
      }
    }
    else if (collections->Get(TRI_V8_ASCII_STRING("write"))->IsString()) {
      writeCollections.push_back(TRI_ObjectToString(collections->Get(TRI_V8_ASCII_STRING("write"))));
    }
    else {
      isValid = false;
    }
  }

  if (! isValid) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(collectionError);
  }

  // extract the "action" property
  static const string actionErrorPrototype = "missing/invalid action definition for transaction";
  string actionError = actionErrorPrototype;

  if (! object->Has(TRI_V8_ASCII_STRING("action"))) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(actionError);
  }

  // function parameters
  v8::Handle<v8::Value> params;

  if (object->Has(TRI_V8_ASCII_STRING("params"))) {
    params = v8::Handle<v8::Array>::Cast(object->Get(TRI_V8_ASCII_STRING("params")));
  }
  else {
    params = v8::Undefined(isolate);
  }

  if (params.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  bool embed = false;  
  if (object->Has(TRI_V8_ASCII_STRING("embed"))) {
    v8::Handle<v8::Value> v = v8::Handle<v8::Object>::Cast(object->Get(TRI_V8_ASCII_STRING("embed")));
    embed = TRI_ObjectToBoolean(v);
  }

  v8::Handle<v8::Object> current = isolate->GetCurrentContext()->Global();

  // callback function
  v8::Handle<v8::Function> action;

  if (object->Get(TRI_V8_ASCII_STRING("action"))->IsFunction()) {
    action = v8::Handle<v8::Function>::Cast(object->Get(TRI_V8_ASCII_STRING("action")));
  }
  else if (object->Get(TRI_V8_ASCII_STRING("action"))->IsString()) {
    v8::TryCatch tryCatch;
    // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
    v8::Local<v8::Function> ctor = v8::Local<v8::Function>::Cast(current->Get(TRI_V8_ASCII_STRING("Function")));

    // Invoke Function constructor to create function with the given body and no arguments
    string body = TRI_ObjectToString(object->Get(TRI_V8_ASCII_STRING("action"))->ToString());
    body = "return (" + body + ")(params);";
    v8::Handle<v8::Value> args[2] = { TRI_V8_ASCII_STRING("params"), TRI_V8_STD_STRING(body) };
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
  }
  else {
    TRI_V8_THROW_EXCEPTION_PARAMETER(actionError);
  }

  if (action.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(actionError);
  }

  // start actual transaction
  ExplicitTransaction trx(vocbase,
                          readCollections,
                          writeCollections,
                          lockTimeout,
                          waitForSync,
                          embed);

  int res = trx.begin();
  
  if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Value> result;
  {
    v8::TryCatch tryCatch;
    v8::Handle<v8::Value> arguments = params;
    result = action->Call(current, 1, &arguments);

    if (tryCatch.HasCaught()) {
      trx.abort();

      if (tryCatch.CanContinue()) {
        tryCatch.ReThrow();
        return;
      }
      else {
        v8g->_canceled = true;
        TRI_V8_RETURN(result);
      }
    }
  }

  res = trx.commit();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  
  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief retrieves the configuration of the write-ahead log
/// @startDocuBlock walPropertiesGet
/// `internal.wal.properties()`
///
/// Retrieves the configuration of the write-ahead log. The result is a JSON
/// array with the following attributes:
/// - *allowOversizeEntries*: whether or not operations that are bigger than a
///   single logfile can be executed and stored
/// - *logfileSize*: the size of each write-ahead logfile
/// - *historicLogfiles*: the maximum number of historic logfiles to keep
/// - *reserveLogfiles*: the maximum number of reserve logfiles that ArangoDB
///   allocates in the background
/// - *syncInterval*: the interval for automatic synchronization of not-yet
///   synchronized write-ahead log data (in milliseconds)
/// - *throttleWait*: the maximum wait time that operations will wait before
///   they get aborted if case of write-throttling (in milliseconds)
/// - *throttleWhenPending*: the number of unprocessed garbage-collection 
///   operations that, when reached, will activate write-throttling. A value of
///   *0* means that write-throttling will not be triggered.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{WalPropertiesGet}
///   require("internal").wal.properties();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief configures the write-ahead log
/// @startDocuBlock walPropertiesSet
/// `internal.wal.properties(properties)`
///
/// Configures the behavior of the write-ahead log. *properties* must be a JSON
/// JSON object with the following attributes:
/// - *allowOversizeEntries*: whether or not operations that are bigger than a 
///   single logfile can be executed and stored
/// - *logfileSize*: the size of each write-ahead logfile
/// - *historicLogfiles*: the maximum number of historic logfiles to keep
/// - *reserveLogfiles*: the maximum number of reserve logfiles that ArangoDB
///   allocates in the background
/// - *throttleWait*: the maximum wait time that operations will wait before
///   they get aborted if case of write-throttling (in milliseconds)
/// - *throttleWhenPending*: the number of unprocessed garbage-collection 
///   operations that, when reached, will activate write-throttling. A value of
///   *0* means that write-throttling will not be triggered.
///
/// Specifying any of the above attributes is optional. Not specified attributes
/// will be ignored and the configuration for them will not be modified.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{WalPropertiesSet}
///   require("internal").wal.properties({ allowOverSizeEntries: true, logfileSize: 32 * 1024 * 1024 });
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_PropertiesWal (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() > 1 || (args.Length() == 1 && ! args[0]->IsObject())) {
    TRI_V8_THROW_EXCEPTION_USAGE("properties(<object>)");
  }
  
  auto l = triagens::wal::LogfileManager::instance();

  if (args.Length() == 1) {
    // set the properties
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);
    if (object->Has(TRI_V8_ASCII_STRING("allowOversizeEntries"))) {
      bool value = TRI_ObjectToBoolean(object->Get(TRI_V8_ASCII_STRING("allowOversizeEntries")));
      l->allowOversizeEntries(value);
    }
    
    if (object->Has(TRI_V8_ASCII_STRING("logfileSize"))) {
      uint32_t value = static_cast<uint32_t>(TRI_ObjectToUInt64(object->Get(TRI_V8_ASCII_STRING("logfileSize")), true));
      l->filesize(value);
    }

    if (object->Has(TRI_V8_ASCII_STRING("historicLogfiles"))) {
      uint32_t value = static_cast<uint32_t>(TRI_ObjectToUInt64(object->Get(TRI_V8_ASCII_STRING("historicLogfiles")), true));
      l->historicLogfiles(value);
    }
    
    if (object->Has(TRI_V8_ASCII_STRING("reserveLogfiles"))) {
      uint32_t value = static_cast<uint32_t>(TRI_ObjectToUInt64(object->Get(TRI_V8_ASCII_STRING("reserveLogfiles")), true));
      l->reserveLogfiles(value);
    }
    
    if (object->Has(TRI_V8_ASCII_STRING("throttleWait"))) {
      uint64_t value = TRI_ObjectToUInt64(object->Get(TRI_V8_ASCII_STRING("throttleWait")), true);
      l->maxThrottleWait(value);
    }
  
    if (object->Has(TRI_V8_ASCII_STRING("throttleWhenPending"))) {
      uint64_t value = TRI_ObjectToUInt64(object->Get(TRI_V8_ASCII_STRING("throttleWhenPending")), true);
      l->throttleWhenPending(value);
    }
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("allowOversizeEntries"), v8::Boolean::New(isolate, l->allowOversizeEntries()));
  result->Set(TRI_V8_ASCII_STRING("logfileSize"),           v8::Number::New(isolate, l->filesize()));
  result->Set(TRI_V8_ASCII_STRING("historicLogfiles"),      v8::Number::New(isolate, l->historicLogfiles()));
  result->Set(TRI_V8_ASCII_STRING("reserveLogfiles"),       v8::Number::New(isolate, l->reserveLogfiles()));
  result->Set(TRI_V8_ASCII_STRING("syncInterval"),          v8::Number::New(isolate, (double) l->syncInterval()));
  result->Set(TRI_V8_ASCII_STRING("throttleWait"),          v8::Number::New(isolate, (double) l->maxThrottleWait()));
  result->Set(TRI_V8_ASCII_STRING("throttleWhenPending"),   v8::Number::New(isolate, (double) l->throttleWhenPending()));

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes the currently open WAL logfile
/// @startDocuBlock walFlush
/// `internal.wal.flush(waitForSync, waitForCollector)`
///
/// Flushes the write-ahead log. By flushing the currently active write-ahead
/// logfile, the data in it can be transferred to collection journals and
/// datafiles. This is useful to ensure that all data for a collection is
/// present in the collection journals and datafiles, for example, when dumping
/// the data of a collection.
///
/// The *waitForSync* option determines whether or not the operation should 
/// block until the not-yet synchronized data in the write-ahead log was 
/// synchronized to disk.
///
/// The *waitForCollector* operation can be used to specify that the operation
/// should block until the data in the flushed log has been collected by the 
/// write-ahead log garbage collector. Note that setting this option to *true* 
/// might block for a long time if there are long-running transactions and 
/// the write-ahead log garbage collector cannot finish garbage collection.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{WalFlush}
///   require("internal").wal.flush();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_FlushWal (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  bool waitForSync = false;
  if (args.Length() > 0) {
    waitForSync = TRI_ObjectToBoolean(args[0]);
  }

  bool waitForCollector = false;
  if (args.Length() > 1) {
    waitForCollector = TRI_ObjectToBoolean(args[1]);
  }

  int res;

  if (ServerState::instance()->isCoordinator()) {
    res = flushWalOnAllDBServers( waitForSync, waitForCollector );
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
    TRI_V8_RETURN_TRUE();
  }

  res = triagens::wal::LogfileManager::instance()->flush(waitForSync, waitForCollector, false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_normalize_string (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("NORMALIZE_STRING(<string>)");
  }

  TRI_normalize_V8_Obj(args, args[0]);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enables or disables native backtrace
////////////////////////////////////////////////////////////////////////////////

static void JS_EnableNativeBacktraces (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("ENABLE_NATIVE_BACKTRACES(<value>)");
  }

  triagens::arango::Exception::SetVerbose(TRI_ObjectToBoolean(args[0]));

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_compare_string (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("COMPARE_STRING(<left string>, <right string>)");
  }

  v8::String::Value left(args[0]);
  v8::String::Value right(args[1]);

  // ..........................................................................
  // Take note here: we are assuming that the ICU type UChar is two bytes.
  // There is no guarantee that this will be the case on all platforms and
  // compilers.
  // ..........................................................................
  int result = Utf8Helper::DefaultUtf8Helper.compareUtf16(*left, left.length(), *right, right.length());

  TRI_V8_RETURN(v8::Integer::New(isolate, result));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of timezones
////////////////////////////////////////////////////////////////////////////////

static void JS_getIcuTimezones (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
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
      const char* str = timeZones->next(&resultLength, status);
      result->Set((uint32_t) i, TRI_V8_PAIR_STRING(str, resultLength));
    }

    delete timeZones;
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of locales
////////////////////////////////////////////////////////////////////////////////

static void JS_getIcuLocales (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
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
      const char* str = l->getBaseName();

      result->Set((uint32_t) i, TRI_V8_STRING(str));
    }
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief format datetime
////////////////////////////////////////////////////////////////////////////////

static void JS_formatDatetime (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("FORMAT_DATETIME(<datetime in sec>, <pattern>, [<timezone>, [<locale>]])");
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

    UnicodeString ts((const UChar *) *value, value.length());
    tz = TimeZone::createTimeZone(ts);
  }
  else {
    tz = TimeZone::createDefault();
  }

  Locale locale;
  if (args.Length() > 3) {
    string name = TRI_ObjectToString(args[3]);
    locale = Locale::createFromName(name.c_str());
  }
  else {
    // use language of default collator
    string name = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();
    locale = Locale::createFromName(name.c_str());
  }

  UnicodeString formattedString;
  UErrorCode status = U_ZERO_ERROR;
  UnicodeString aPattern((const UChar *) *pattern, pattern.length());
  DateFormatSymbols* ds = new DateFormatSymbols(locale, status);
  SimpleDateFormat* s = new SimpleDateFormat(aPattern, ds, status);
  s->setTimeZone(*tz);
  s->format((UDate) (datetime * 1000), formattedString);

  string resultString;
  formattedString.toUTF8String(resultString);
  delete s;
  delete tz;

  TRI_V8_RETURN_STD_STRING(resultString);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse datetime
////////////////////////////////////////////////////////////////////////////////

static void JS_parseDatetime (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("PARSE_DATETIME(<datetime string>, <pattern>, [<timezone>, [<locale>]])");
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

    UnicodeString ts((const UChar *) *value, value.length());
    tz = TimeZone::createTimeZone(ts);
  }
  else {
    tz = TimeZone::createDefault();
  }

  Locale locale;
  if (args.Length() > 3) {
    string name = TRI_ObjectToString(args[3]);
    locale = Locale::createFromName(name.c_str());
  }
  else {
    // use language of default collator
    string name = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();
    locale = Locale::createFromName(name.c_str());
  }

  UnicodeString formattedString((const UChar *) *datetimeString, datetimeString.length());
  UErrorCode status = U_ZERO_ERROR;
  UnicodeString aPattern((const UChar *) *pattern, pattern.length());
  DateFormatSymbols* ds = new DateFormatSymbols(locale, status);
  SimpleDateFormat* s = new SimpleDateFormat(aPattern, ds, status);
  s->setTimeZone(*tz);

  UDate udate = s->parse(formattedString, status);

  delete s;
  delete tz;

  TRI_V8_RETURN(v8::Number::New(isolate, udate / 1000));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the authentication info, coordinator case
////////////////////////////////////////////////////////////////////////////////

static bool ReloadAuthCoordinator (TRI_vocbase_t* vocbase) {
  TRI_json_t* json = nullptr;
  bool result;

  int res = usersOnCoordinator(string(vocbase->_name),
                               json, 60.0);

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_ASSERT(json != nullptr);

    result = TRI_PopulateAuthInfo(vocbase, json);
  }
  else {
    result = false;
  }

  if (json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the authentication info from collection _users
////////////////////////////////////////////////////////////////////////////////

static void JS_ReloadAuth (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == 0) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("RELOAD_AUTH()");
  }

  bool result;
  if (ServerState::instance()->isCoordinator()) {
    result = ReloadAuthCoordinator(vocbase);
  }
  else {
    result = TRI_ReloadAuthInfo(vocbase);
  }
  if (result) {
    TRI_V8_RETURN_TRUE();
  }
  else {
    TRI_V8_RETURN_FALSE();
  }
}


// -----------------------------------------------------------------------------
// --SECTION--                                                               AQL
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an AQL query
////////////////////////////////////////////////////////////////////////////////

static void JS_ParseAql (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_PARSE(<querystring>)");
  }

  // get the query string
  if (! args[0]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <querystring>");
  }

  string const&& queryString = TRI_ObjectToString(args[0]);

  TRI_GET_GLOBALS();
  triagens::aql::Query query(v8g->_applicationV8, true, vocbase, queryString.c_str(), queryString.size(), nullptr, nullptr, triagens::aql::PART_MAIN);

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
    for (auto it = parseResult.collectionNames.begin(); it != parseResult.collectionNames.end(); ++it) {
      collections->Set(i++, TRI_V8_STD_STRING((*it)));
    }
  }

  {
    v8::Handle<v8::Array> bindVars = v8::Array::New(isolate);
    uint32_t i = 0;
    for (auto it = parseResult.bindParameters.begin(); it != parseResult.bindParameters.end(); ++it) {
      bindVars->Set(i++, TRI_V8_STD_STRING((*it)));
    }
    result->Set(TRI_V8_ASCII_STRING("parameters"), bindVars); 
  }

  result->Set(TRI_V8_ASCII_STRING("ast"), TRI_ObjectJson(isolate, parseResult.json));
    
  if (parseResult.warnings == nullptr) {
    result->Set(TRI_V8_ASCII_STRING("warnings"), v8::Array::New(isolate));
  }
  else {
    result->Set(TRI_V8_ASCII_STRING("warnings"), TRI_ObjectJson(isolate, parseResult.warnings));
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a warning for the currently running AQL query
/// this function is called from aql.js
////////////////////////////////////////////////////////////////////////////////

static void JS_WarningAql (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_WARNING(<code>, <message>)");
  }

  // get the query string
  if (! args[1]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <message>");
  }
  
  TRI_GET_GLOBALS();

  if (v8g->_query != nullptr) {
    // only register the error if we have a query...
    // note: we may not have a query if the AQL functions are called without
    // a query, e.g. during tests
    int code = static_cast<int>(TRI_ObjectToInt64(args[0]));
    std::string const&& message = TRI_ObjectToString(args[1]);

    auto query = static_cast<triagens::aql::Query*>(v8g->_query);
    query->registerWarning(code, message.c_str());
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explains an AQL query
////////////////////////////////////////////////////////////////////////////////

static void JS_ExplainAql (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  
  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_EXPLAIN(<querystring>, <bindvalues>, <options>)");
  }

  // get the query string
  if (! args[0]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <querystring>");
  }

  string const&& queryString = TRI_ObjectToString(args[0]);

  // bind parameters
  TRI_json_t* parameters = nullptr;
  
  if (args.Length() > 1) {
    if (! args[1]->IsUndefined() && ! args[1]->IsNull() && ! args[1]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting object for <bindvalues>");
    }
    if (args[1]->IsObject()) {
      parameters = TRI_ObjectToJson(isolate, args[1]);
    }
  }

  TRI_json_t* options = nullptr;

  if (args.Length() > 2) {
    // handle options
    if (! args[2]->IsObject()) {
      if (parameters != nullptr) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);
      }
      TRI_V8_THROW_TYPE_ERROR("expecting object for <options>");
    }

    options = TRI_ObjectToJson(isolate, args[2]);
  }

  // bind parameters will be freed by the query later
  TRI_GET_GLOBALS();
  triagens::aql::Query query(v8g->_applicationV8, true, vocbase, queryString.c_str(), queryString.size(), parameters, options, triagens::aql::PART_MAIN);
  
  auto queryResult = query.explain();
  
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_FULL(queryResult.code, queryResult.details);
  }
  
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  if (queryResult.json != nullptr) {
    if (query.allPlans()) {
      result->Set(TRI_V8_ASCII_STRING("plans"), TRI_ObjectJson(isolate, queryResult.json));
    }
    else {
      result->Set(TRI_V8_ASCII_STRING("plan"), TRI_ObjectJson(isolate, queryResult.json));
    }
    if (queryResult.clusterplan != nullptr) {
      result->Set(TRI_V8_ASCII_STRING("clusterplans"), TRI_ObjectJson(isolate, queryResult.clusterplan));
    }

    if (queryResult.warnings == nullptr) {
      result->Set(TRI_V8_ASCII_STRING("warnings"), v8::Array::New(isolate));
    }
    else {
      result->Set(TRI_V8_ASCII_STRING("warnings"), TRI_ObjectJson(isolate, queryResult.warnings));
    }
    if (queryResult.stats == nullptr) {
      result->Set(TRI_V8_STRING("stats"), v8::Object::New(isolate));
    }
    else {
      result->Set(TRI_V8_STRING("stats"), TRI_ObjectJson(isolate, queryResult.stats));
    }
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an AQL query
////////////////////////////////////////////////////////////////////////////////

static void JS_ExecuteAqlJson (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  
  if (args.Length() < 1 || args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_EXECUTEJSON(<queryjson>, <options>)");
  }
  
  // return number of total records in cursor?
  bool doCount = false;

  // maximum number of results to return at once
  size_t batchSize = SIZE_MAX;
  
  // ttl for cursor  
  double ttl = 0.0;
  
  if (! args[0]->IsObject()) {
    TRI_V8_THROW_TYPE_ERROR("expecting object for <queryjson>");
  }
  TRI_json_t* queryjson = TRI_ObjectToJson(isolate, args[0]);
  TRI_json_t* options = nullptr;

  if (args.Length() > 1) {
    // we have options! yikes!
    if (! args[1]->IsUndefined() && ! args[1]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting object for <options>");
    }

    v8::Handle<v8::Object> argValue = v8::Handle<v8::Object>::Cast(args[1]);
      
    v8::Handle<v8::String> optionName = TRI_V8_ASCII_STRING("batchSize");
    if (argValue->Has(optionName)) {
      batchSize = static_cast<decltype(batchSize)>(TRI_ObjectToInt64(argValue->Get(optionName)));
      if (batchSize == 0) {
        TRI_V8_THROW_TYPE_ERROR("expecting non-zero value for <batchSize>");
        // well, this makes no sense
      }
    }
      
    optionName = TRI_V8_ASCII_STRING("count");
    if (argValue->Has(optionName)) {
      doCount = TRI_ObjectToBoolean(argValue->Get(optionName));
    }
      
    optionName = TRI_V8_ASCII_STRING("ttl");
    if (argValue->Has(optionName)) {
      ttl = TRI_ObjectToDouble(argValue->Get(optionName));
      ttl = (ttl <= 0.0 ? 30.0 : ttl);
    }
      
    options = TRI_ObjectToJson(isolate, args[1]);
  }

  TRI_GET_GLOBALS();
  triagens::aql::Query query(v8g->_applicationV8, true, vocbase, Json(TRI_UNKNOWN_MEM_ZONE, queryjson), options, triagens::aql::PART_MAIN);

  auto queryResult = query.execute(static_cast<triagens::aql::QueryRegistry*>(v8g->_queryRegistry));
  
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_FULL(queryResult.code, queryResult.details);
  }
  
  if (TRI_LengthArrayJson(queryResult.json) <= batchSize) {
    // return the array value as it is. this is a performance optimisation
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    if (queryResult.json != nullptr) {
      result->Set(TRI_V8_ASCII_STRING("json"),     TRI_ObjectJson(isolate, queryResult.json));
    }
    if (queryResult.stats != nullptr) {
      result->Set(TRI_V8_ASCII_STRING("stats"),    TRI_ObjectJson(isolate, queryResult.stats));
    }
    if (queryResult.profile != nullptr) {
      result->Set(TRI_V8_ASCII_STRING("profile"),  TRI_ObjectJson(isolate, queryResult.profile));
    }
    if (queryResult.warnings == nullptr) {
      result->Set(TRI_V8_ASCII_STRING("warnings"), v8::Array::New(isolate));
    }
    else {
      result->Set(TRI_V8_ASCII_STRING("warnings"), TRI_ObjectJson(isolate, queryResult.warnings));
    }
    TRI_V8_RETURN(result);
  }
  
  TRI_json_t* extra = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  if (extra == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  if (queryResult.warnings != nullptr) {
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, extra, "warnings", queryResult.warnings);
    queryResult.warnings = nullptr;
  }
  if (queryResult.stats != nullptr) {
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, extra, "stats", queryResult.stats);
    queryResult.stats = nullptr;
  }
  if (queryResult.profile != nullptr) {
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, extra, "profile", queryResult.profile);
    queryResult.profile = nullptr;
  }

  TRI_general_cursor_result_t* cursorResult = TRI_CreateResultGeneralCursor(queryResult.json);
  
  if (cursorResult == nullptr){
    if (extra != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, extra);
    }
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  
  queryResult.json = nullptr;
  
  TRI_general_cursor_t* cursor = TRI_CreateGeneralCursor(vocbase, cursorResult, doCount,
      static_cast<TRI_general_cursor_length_t>(batchSize), ttl, extra);

  if (cursor == nullptr) {
    if (extra != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, extra);
    }
    TRI_FreeCursorResult(cursorResult);
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  TRI_ASSERT(cursor != nullptr);
  
  TRI_WrapGeneralCursor(args, cursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an AQL query
////////////////////////////////////////////////////////////////////////////////

static void JS_ExecuteAql (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  
  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_EXECUTE(<querystring>, <bindvalues>, <options>)");
  }

  // get the query string
  if (! args[0]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <querystring>");
  }

  string const&& queryString = TRI_ObjectToString(args[0]);

  // bind parameters
  TRI_json_t* parameters = nullptr;
  
  // return number of total records in cursor?
  bool doCount = false;

  // maximum number of results to return at once
  size_t batchSize = SIZE_MAX;
  
  // ttl for cursor  
  double ttl = 0.0;
  
  // options
  TRI_json_t* options = nullptr;

  if (args.Length() > 1) {
    if (! args[1]->IsUndefined() && ! args[1]->IsNull() && ! args[1]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting object for <bindvalues>");
    }
    if (args[1]->IsObject()) {
      parameters = TRI_ObjectToJson(isolate, args[1]);
    }
  }
    
  if (args.Length() > 2) {
    // we have options! yikes!
    if (! args[2]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting object for <options>");
    }

    v8::Handle<v8::Object> argValue = v8::Handle<v8::Object>::Cast(args[2]);
      
    v8::Handle<v8::String> optionName = TRI_V8_ASCII_STRING("batchSize");
    if (argValue->Has(optionName)) {
      batchSize = static_cast<decltype(batchSize)>(TRI_ObjectToInt64(argValue->Get(optionName)));
      if (batchSize == 0) {
        TRI_V8_THROW_TYPE_ERROR("expecting non-zero value for <batchSize>");
        // well, this makes no sense
      }
    }
      
    optionName = TRI_V8_ASCII_STRING("count");
    if (argValue->Has(optionName)) {
      doCount = TRI_ObjectToBoolean(argValue->Get(optionName));
    }
    optionName = TRI_V8_ASCII_STRING("ttl");
    if (argValue->Has(optionName)) {
      ttl = TRI_ObjectToDouble(argValue->Get(optionName));
      ttl = (ttl <= 0.0 ? 30.0 : ttl);
    }
      
    options = TRI_ObjectToJson(isolate, args[2]);
  }
      
  // bind parameters will be freed by the query later
  TRI_GET_GLOBALS();
  triagens::aql::Query query(v8g->_applicationV8, true, vocbase, queryString.c_str(), queryString.size(), parameters, options, triagens::aql::PART_MAIN);

  auto queryResult = query.executeV8(isolate, static_cast<triagens::aql::QueryRegistry*>(v8g->_queryRegistry));
  
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED) {
      TRI_GET_GLOBALS();
      v8g->_canceled = true;
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    TRI_V8_THROW_EXCEPTION_FULL(queryResult.code, queryResult.details);
  }
  
  if (queryResult.result->Length() <= batchSize) {
    // return the array value as it is. this is a performance optimisation
    v8::Handle<v8::Object> result = v8::Object::New(isolate);

    result->Set(TRI_V8_ASCII_STRING("json"), queryResult.result);

    if (queryResult.stats != nullptr) {
      result->Set(TRI_V8_ASCII_STRING("stats"),    TRI_ObjectJson(isolate, queryResult.stats));
    }
    if (queryResult.profile != nullptr) {
      result->Set(TRI_V8_ASCII_STRING("profile"),  TRI_ObjectJson(isolate, queryResult.profile));
    }
    if (queryResult.warnings == nullptr) {
      result->Set(TRI_V8_ASCII_STRING("warnings"), v8::Array::New(isolate));
    }
    else {
      result->Set(TRI_V8_ASCII_STRING("warnings"), TRI_ObjectJson(isolate, queryResult.warnings));
    }
    TRI_V8_RETURN(result);
  }
  
  TRI_json_t* extra = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  if (extra == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  if (queryResult.warnings != nullptr) {
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, extra, "warnings", queryResult.warnings);
    queryResult.warnings = nullptr;
  }
  if (queryResult.stats != nullptr) {
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, extra, "stats", queryResult.stats);
    queryResult.stats = nullptr;
  }
  if (queryResult.profile != nullptr) {
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, extra, "profile", queryResult.profile);
    queryResult.profile = nullptr;
  }

  TRI_general_cursor_result_t* cursorResult = TRI_CreateResultGeneralCursor(isolate, queryResult.result);
  
  if (cursorResult == nullptr) {
    if (extra != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, extra);
    }
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  
  TRI_general_cursor_t* cursor = TRI_CreateGeneralCursor(vocbase, cursorResult, doCount,
      static_cast<TRI_general_cursor_length_t>(batchSize), ttl, extra);

  if (cursor == nullptr) {
    if (extra != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, extra);
    }
    TRI_FreeCursorResult(cursorResult);
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_ASSERT(cursor != nullptr);
  
  TRI_WrapGeneralCursor(args, cursor);
}

// -----------------------------------------------------------------------------
// --SECTION--                                           TRI_VOCBASE_T FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapVocBase (v8::Isolate *isolate, TRI_vocbase_t const* database) {

  TRI_GET_GLOBALS();
  
  v8::Handle<v8::Object> result = WrapClass(isolate,
                                            v8g->VocbaseTempl,
                                            WRP_VOCBASE_TYPE,
                                            const_cast<TRI_vocbase_t*>(database));
  return result;                                                       
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a collection from the vocbase
/// @startDocuBlock collectionDatabaseCollectionName
/// `db.collection-name`
///
/// Returns the collection with the given *collection-name*. If no such
/// collection exists, create a collection named *collection-name* with the
/// default properties.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCollectionName}
/// ~ db._create("example");
///   db.example;
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// 
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void MapGetVocBase (v8::Local<v8::String> const name,
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

  if (strcmp(key, "hasOwnProperty") == 0 ||  // this prevents calling the property getter again (i.e. recursion!)
      strcmp(key, "toString") == 0 ||
      strcmp(key, "toJSON") == 0) {
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }

  TRI_vocbase_col_t* collection = nullptr;

  // generate a name under which the cached property is stored
  string cacheKey(key, keyLength);
  cacheKey.push_back('*');

  v8::Local<v8::String> cacheName = TRI_V8_STD_STRING(cacheKey);
  v8::Handle<v8::Object> holder   = args.Holder()->ToObject();

  if (*key == '_') {
    // special treatment for all properties starting with _
    v8::Local<v8::String> const l = TRI_V8_PAIR_STRING(key, (int) keyLength);

    if (holder->HasRealNamedProperty(l)) {
      // some internal function inside db
      TRI_V8_RETURN(v8::Handle<v8::Value>());
    }

    // something in the prototype chain?
    v8::Local<v8::Value> v = holder->GetRealNamedPropertyInPrototypeChain(l);

    if (! v.IsEmpty()) {
      if (! v->IsExternal()) {
        // something but an external... this means we can directly return this
        TRI_V8_RETURN(v8::Handle<v8::Value>());
      }
    }
  }

  if (holder->HasRealNamedProperty(cacheName)) {
    v8::Handle<v8::Object> value = holder->GetRealNamedProperty(cacheName)->ToObject();

    collection = TRI_UnwrapClass<TRI_vocbase_col_t>(value, WRP_VOCBASE_COL_TYPE);

    // check if the collection is from the same database
    if (collection != nullptr && collection->_vocbase == vocbase) {
      TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
      TRI_vocbase_col_status_e status = collection->_status;
      TRI_voc_cid_t cid = collection->_cid;
      uint32_t internalVersion = collection->_internalVersion;
      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

      // check if the collection is still alive
      if (status != TRI_VOC_COL_STATUS_DELETED && cid > 0 && collection->_isLocal) {
        TRI_GET_GLOBALS();

        TRI_GET_GLOBAL_STRING(_IdKey);
        TRI_GET_GLOBAL_STRING(VersionKey);
        if (value->Has(_IdKey)) {
          TRI_voc_cid_t cachedCid = static_cast<TRI_voc_cid_t>(TRI_ObjectToUInt64(value->Get(_IdKey), true));
          uint32_t cachedVersion = (uint32_t) TRI_ObjectToInt64(value->Get(VersionKey));

          if (cachedCid == cid && cachedVersion == internalVersion) {
            // cache hit
            TRI_V8_RETURN(value);
          }

          // store the updated version number in the object for future comparisons
          value->ForceSet(VersionKey, v8::Number::New(isolate, (double) internalVersion), v8::DontEnum);

          // cid has changed (i.e. collection has been dropped and re-created) or version has changed
        }
      }
    }

    // cache miss
    holder->Delete(cacheName);
  }

  if (ServerState::instance()->isCoordinator()) {
    shared_ptr<CollectionInfo> const& ci
        = ClusterInfo::instance()->getCollection(vocbase->_name, std::string(key));

    if ((*ci).empty()) {
      collection = nullptr;
    }
    else {
      collection = CoordinatorCollection(vocbase, *ci);

      if (collection != nullptr && collection->_cid == 0) {
        FreeCoordinatorCollection(collection);
        TRI_V8_RETURN(v8::Handle<v8::Value>());
      }
    }
  }
  else {
    collection = TRI_LookupCollectionByNameVocBase(vocbase, key);
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

  // TODO: caching the result makes subsequent results much faster, but
  // prevents physical removal of the collection or database
  holder->ForceSet(cacheName, result, v8::DontEnum);

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the server version string
/// @startDocuBlock databaseVersion
/// `db._version()`
///
/// Returns the server version string. Note that this is not the version of the
/// database.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_VersionServer (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_V8_RETURN(TRI_V8_ASCII_STRING(TRI_VERSION));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the path to database files
/// @startDocuBlock databasePath
/// `db._path()`
///
/// Returns the filesystem path of the current database as a string.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_PathDatabase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_V8_RETURN_STRING(vocbase->_path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database id
/// @startDocuBlock databaseId
/// `db._id()`
///
/// Returns the id of the current database as a string.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_IdDatabase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_V8_RETURN(V8TickId(isolate, vocbase->_id));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database name
/// @startDocuBlock databaseName
/// `db._name()`
///
/// Returns the name of the current database as a string.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_NameDatabase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_V8_RETURN_STRING(vocbase->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database type
/// @startDocuBlock databaseIsSystem
/// `db._isSystem()`
///
/// Returns whether the currently used database is the *_system* database.
/// The system database has some special privileges and properties, for example,
/// database management operations such as create or drop can only be executed
/// from within this database. Additionally, the *_system* database itself
/// cannot be dropped.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_IsSystemDatabase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_V8_RETURN(v8::Boolean::New(isolate, TRI_IsSystemVocBase(vocbase)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief change the current database
/// @startDocuBlock databaseUseDatabase
/// `db._useDatabase(name)`
///
/// Changes the current database to the database specified by *name*. Note
/// that the database specified by *name* must already exist.
///
/// Changing the database might be disallowed in some contexts, for example
/// server-side actions (including Foxx).
///
/// When performing this command from arangosh, the current credentials (username
/// and password) will be re-used. These credentials might not be valid to
/// connect to the database specified by *name*. Additionally, the database
/// only be accessed from certain endpoints only. In this case, switching the
/// database might not work, and the connection / session should be closed and
/// restarted with different username and password credentials and/or
/// endpoint data.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_UseDatabase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._useDatabase(<name>)");
  }

  TRI_GET_GLOBALS();

  if (! v8g->_allowUseDatabase) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  string const name = TRI_ObjectToString(args[0]);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  if (TRI_EqualString(name.c_str(), vocbase->_name)) {
    // same database. nothing to do
    TRI_V8_RETURN(WrapVocBase(isolate, vocbase));
  }

  if (ServerState::instance()->isCoordinator()) {
    vocbase = TRI_UseCoordinatorDatabaseServer(static_cast<TRI_server_t*>(v8g->_server), name.c_str());
  }
  else {
    // check if the other database exists, and increase its refcount
    vocbase = TRI_UseDatabaseServer(static_cast<TRI_server_t*>(v8g->_server), name.c_str());
  }

  if (vocbase != nullptr) {
    // switch databases
    void* orig = v8g->_vocbase;
    TRI_ASSERT(orig != nullptr);

    v8g->_vocbase = vocbase;

    if (orig != vocbase) {
      TRI_ReleaseDatabaseServer(static_cast<TRI_server_t*>(v8g->_server), (TRI_vocbase_t*) orig);
    }

    TRI_V8_RETURN(WrapVocBase(isolate, vocbase));
  }

  TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all existing databases in a coordinator
////////////////////////////////////////////////////////////////////////////////

static void ListDatabasesCoordinator (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // Arguments are already checked, there are 0 or 3.

  ClusterInfo* ci = ClusterInfo::instance();

  if (args.Length() == 0) {
    vector<DatabaseID> list = ci->listDatabases(true);
    v8::Handle<v8::Array> result = v8::Array::New(isolate);
    for (size_t i = 0;  i < list.size();  ++i) {
      result->Set((uint32_t) i, TRI_V8_STD_STRING(list[i]));
    }
    TRI_V8_RETURN(result);
  }
  else {
    // We have to ask a DBServer, any will do:
    int tries = 0;
    vector<ServerID> DBServers;
    while (++tries <= 2) {
      DBServers = ci->getCurrentDBServers();

      if (! DBServers.empty()) {
        ServerID sid = DBServers[0];
        ClusterComm* cc = ClusterComm::instance();
        ClusterCommResult* res;
        map<string, string> headers;
        headers["Authentication"] = TRI_ObjectToString(args[2]);
        res = cc->syncRequest("", 0, "server:" + sid,
                              triagens::rest::HttpRequest::HTTP_REQUEST_GET,
                              "/_api/database/user", string(""), headers, 0.0);

        if (res->status == CL_COMM_SENT) {
          // We got an array back as JSON, let's parse it and build a v8
          StringBuffer& body = res->result->getBody();

          TRI_json_t* json = JsonHelper::fromString(body.c_str());
          delete res;

          if (json != 0 && JsonHelper::isObject(json)) {
            TRI_json_t const* dotresult = JsonHelper::getObjectElement(json, "result");

            if (dotresult != 0) {
              vector<string> list = JsonHelper::stringArray(dotresult);
              TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
              v8::Handle<v8::Array> result = v8::Array::New(isolate);
              for (size_t i = 0;  i < list.size();  ++i) {
                result->Set((uint32_t) i, TRI_V8_STD_STRING(list[i]));
              }
              TRI_V8_RETURN(result);
            }
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          }
        }
        else {
          delete res;
        }
      }
      ci->loadCurrentDBServers();   // just in case some new have arrived
    }
    // Give up:
    TRI_V8_RETURN_UNDEFINED();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all existing databases
/// @startDocuBlock databaseListDatabase
/// `db._listDatabases()`
///
/// Returns the list of all databases. This method can only be used from within
/// the *_system* database.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_ListDatabases (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  const uint32_t argc = args.Length();
  if (argc != 0 && argc != 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._listDatabases()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (argc == 0 &&
      ! TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
    ListDatabasesCoordinator(args);
    return;
  }

  TRI_GET_GLOBALS();

  TRI_vector_string_t names;
  TRI_InitVectorString(&names, TRI_UNKNOWN_MEM_ZONE);

  int res;

  if (argc == 0) {
    // return all databases
    res = TRI_GetDatabaseNamesServer(static_cast<TRI_server_t*>(v8g->_server), &names);
  }
  else {
    // return all databases for a specific user
    string username = TRI_ObjectToString(args[0]);
    string password = TRI_ObjectToString(args[1]);
    res = TRI_GetUserDatabasesServer((TRI_server_t*) v8g->_server, username.c_str(), password.c_str(), &names);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVectorString(&names);
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Array> result = v8::Array::New(isolate);
  for (size_t i = 0;  i < names._length;  ++i) {
    result->Set((uint32_t) i, TRI_V8_STRING((char const*) TRI_AtVectorString(&names, i)));
  }

  TRI_DestroyVectorString(&names);

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

static void CreateDatabaseCoordinator (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // First work with the arguments to create a JSON entry:
  string const name = TRI_ObjectToString(args[0]);

  if (! TRI_IsAllowedNameVocBase(false, name.c_str())) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
  }

  TRI_json_t* json = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);

  if (nullptr == json) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  uint64_t const id = ClusterInfo::instance()->uniqid();
  std::string const idString(StringUtils::itoa(id));

  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "id",
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE,
                               idString.c_str(), idString.size()));

  std::string const valueString(TRI_ObjectToString(args[0]));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "name",
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE,
                               valueString.c_str(), valueString.size()));

  if (args.Length() > 1) {
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "options",
                         TRI_ObjectToJson(isolate, args[1]));
  }

  std::string const serverId(ServerState::instance()->getId());
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "coordinator",
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, serverId.c_str(), serverId.size()));

  ClusterInfo* ci = ClusterInfo::instance();
  string errorMsg;

  int res = ci->createDatabaseCoordinator( name, json, errorMsg, 120.0);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, errorMsg);
  }

  // database was created successfully in agency

  TRI_GET_GLOBALS();

  // now wait for heartbeat thread to create the database object
  TRI_vocbase_t* vocbase = nullptr;
  int tries = 0;

  while (++tries <= 6000) {
    vocbase = TRI_UseByIdCoordinatorDatabaseServer(static_cast<TRI_server_t*>(v8g->_server), id);

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

    isolate->GetCurrentContext()->Global()->Set(TRI_V8_ASCII_STRING("UPGRADE_ARGS"), users);
  }
  else {
    isolate->GetCurrentContext()->Global()->Set(TRI_V8_ASCII_STRING("UPGRADE_ARGS"), v8::Object::New(isolate));
  }

  // switch databases
  TRI_vocbase_t* orig = v8g->_vocbase;
  TRI_ASSERT(orig != nullptr);

  v8g->_vocbase = vocbase;

  // initalise database
  bool allowUseDatabase = v8g->_allowUseDatabase;
  v8g->_allowUseDatabase = true;

  v8g->_loader->executeGlobalScript(isolate, isolate->GetCurrentContext(), "server/bootstrap/coordinator-database.js");
  
  v8g->_allowUseDatabase = allowUseDatabase;

  // and switch back
  v8g->_vocbase = orig;

  TRI_ReleaseVocBase(vocbase);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new database
/// @startDocuBlock databaseCreateDatabase
/// `db._createDatabase(name, options, users)`
///
/// Creates a new database with the name specified by *name*.
/// There are restrictions for database names
/// (see [DatabaseNames](../NamingConventions/DatabaseNames.md)).
///
/// Note that even if the database is created successfully, there will be no
/// change into the current database to the new database. Changing the current
/// database must explicitly be requested by using the
/// *db._useDatabase* method.
///
/// The *options* attribute currently has no meaning and is reserved for
/// future use.
///
/// The optional *users* attribute can be used to create initial users for
/// the new database. If specified, it must be a list of user objects. Each user
/// object can contain the following attributes:
///
/// * *username*: the user name as a string. This attribute is mandatory.
/// * *passwd*: the user password as a string. If not specified, then it defaults
///   to the empty string.
/// * *active*: a boolean flag indicating whether the user account should be
///   active or not. The default value is *true*.
/// * *extra*: an optional JSON object with extra user information. The data
///   contained in *extra* will be stored for the user but not be interpreted
///   further by ArangoDB.
///
/// If no initial users are specified, a default user *root* will be created
/// with an empty string password. This ensures that the new database will be
/// accessible via HTTP after it is created.
///
/// You can create users in a database if no initial user is specified. Switch 
/// into the new database (username and password must be identical to the current
/// session) and add or modify users with the following commands.
///
/// ```js
///   require("org/arangodb/users").save(username, password, true);
///   require("org/arangodb/users").update(username, password, true);
///   require("org/arangodb/users").remove(username);
/// ```
///
/// This method can only be used from within the *_system* database.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateDatabase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._createDatabase(<name>, <options>, <users>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
  }

  if (! TRI_IsSystemVocBase(vocbase)) {
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
  TRI_GetDatabaseDefaultsServer(static_cast<TRI_server_t*>(v8g->_server), &defaults);

  v8::Local<v8::String> keyDefaultMaximalSize               = TRI_V8_ASCII_STRING("defaultMaximalSize");
  v8::Local<v8::String> keyDefaultWaitForSync               = TRI_V8_ASCII_STRING("defaultWaitForSync");
  v8::Local<v8::String> keyRequireAuthentication            = TRI_V8_ASCII_STRING("requireAuthentication");
  v8::Local<v8::String> keyRequireAuthenticationUnixSockets = TRI_V8_ASCII_STRING("requireAuthenticationUnixSockets");
  v8::Local<v8::String> keyAuthenticateSystemOnly           = TRI_V8_ASCII_STRING("authenticateSystemOnly");
  v8::Local<v8::String> keyForceSyncProperties              = TRI_V8_ASCII_STRING("forceSyncProperties");

  // overwrite database defaults from args[2]
  if (args.Length() > 1 && args[1]->IsObject()) {
    v8::Handle<v8::Object> options = args[1]->ToObject();

    if (options->Has(keyDefaultMaximalSize)) {
      defaults.defaultMaximalSize = (TRI_voc_size_t) options->Get(keyDefaultMaximalSize)->IntegerValue();
    }

    if (options->Has(keyDefaultWaitForSync)) {
      defaults.defaultWaitForSync = options->Get(keyDefaultWaitForSync)->BooleanValue();
    }

    if (options->Has(keyRequireAuthentication)) {
      defaults.requireAuthentication = options->Get(keyRequireAuthentication)->BooleanValue();
    }

    if (options->Has(keyRequireAuthenticationUnixSockets)) {
      defaults.requireAuthenticationUnixSockets = options->Get(keyRequireAuthenticationUnixSockets)->BooleanValue();
    }

    if (options->Has(keyAuthenticateSystemOnly)) {
      defaults.authenticateSystemOnly = options->Get(keyAuthenticateSystemOnly)->BooleanValue();
    }

    if (options->Has(keyForceSyncProperties)) {
      defaults.forceSyncProperties = options->Get(keyForceSyncProperties)->BooleanValue();
    }
    
    TRI_GET_GLOBAL_STRING(IdKey);
    if (options->Has(IdKey)) {
      // only used for testing to create database with a specific id
      id = TRI_ObjectToUInt64(options->Get(IdKey), true);
    }
  }

  string const name = TRI_ObjectToString(args[0]);

  TRI_vocbase_t* database;
  int res = TRI_CreateDatabaseServer(static_cast<TRI_server_t*>(v8g->_server), id, name.c_str(), &defaults, &database, true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(database != nullptr);

  // copy users into context
  if (args.Length() >= 3 && args[2]->IsArray()) {
    v8::Handle<v8::Object> users = v8::Object::New(isolate);
    users->Set(TRI_V8_ASCII_STRING("users"), args[2]);

    isolate->GetCurrentContext()->Global()->Set(TRI_V8_ASCII_STRING("UPGRADE_ARGS"), users);
  }
  else {
    isolate->GetCurrentContext()->Global()->Set(TRI_V8_ASCII_STRING("UPGRADE_ARGS"), v8::Object::New(isolate));
  }

  // switch databases
  TRI_vocbase_t* orig = v8g->_vocbase;
  TRI_ASSERT(orig != nullptr);

  v8g->_vocbase = database;

  // initalise database
  v8g->_loader->executeGlobalScript(isolate, isolate->GetCurrentContext(), "server/bootstrap/local-database.js");

  // and switch back
  v8g->_vocbase = orig;

  // populate the authentication cache. otherwise no one can access the new database
  TRI_ReloadAuthInfo(database);

  // finally decrease the reference-counter
  TRI_ReleaseVocBase(database);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop a database, case of a coordinator in a cluster
////////////////////////////////////////////////////////////////////////////////

static void DropDatabaseCoordinator (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  // Arguments are already checked, there is exactly one argument
  string const name = TRI_ObjectToString(args[0]);
  TRI_vocbase_t* vocbase = TRI_UseCoordinatorDatabaseServer(static_cast<TRI_server_t*>(v8g->_server), name.c_str());

  if (vocbase == nullptr) {
    // no such database
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_voc_tick_t const id = vocbase->_id;
  TRI_ReleaseVocBase(vocbase);


  ClusterInfo* ci = ClusterInfo::instance();
  string errorMsg;

  int res = ci->dropDatabaseCoordinator(name, errorMsg, 120.0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, errorMsg);
  }

  // now wait for heartbeat thread to drop the database object
  int tries = 0;

  while (++tries <= 6000) {
    TRI_vocbase_t* vocbase = TRI_UseByIdCoordinatorDatabaseServer((TRI_server_t*) v8g->_server, id);

    if (vocbase == nullptr) {
      // object has vanished
      break;
    }

    // sleep
    usleep(10000);
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop an existing database
/// @startDocuBlock databaseDropDatabase
/// `db._dropDatabase(name)`
///
/// Drops the database specified by *name*. The database specified by
/// *name* must exist.
///
/// **Note**: Dropping databases is only possible from within the *_system*
/// database. The *_system* database itself cannot be dropped.
///
/// Databases are dropped asynchronously, and will be physically removed if
/// all clients have disconnected and references have been garbage-collected.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_DropDatabase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._dropDatabase(<name>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (! TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
    DropDatabaseCoordinator(args);
    return;
  }

  string const name = TRI_ObjectToString(args[0]);
  TRI_GET_GLOBALS();

  int res = TRI_DropDatabaseServer(static_cast<TRI_server_t*>(v8g->_server), name.c_str(), true, true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8ReloadRouting(isolate);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief configure a new endpoint
///
/// @FUN{CONFIGURE_ENDPOINT}
////////////////////////////////////////////////////////////////////////////////

static void JS_ConfigureEndpoint (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._configureEndpoint(<endpoint>, <databases>)");
  }

  TRI_GET_GLOBALS();
  TRI_server_t* server = static_cast<TRI_server_t*>(v8g->_server);
  ApplicationEndpointServer* s = static_cast<ApplicationEndpointServer*>(server->_applicationEndpointServer);

  if (s == 0) {
    // not implemented in console mode
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == 0) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (! TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  const string endpoint = TRI_ObjectToString(args[0]);

  // register dbNames
  vector<string> dbNames;

  if (args.Length() > 1) {
    if (! args[1]->IsArray()) {
      TRI_V8_THROW_EXCEPTION_PARAMETER("<databases> must be a list");
    }

    v8::Handle<v8::Array> list = v8::Handle<v8::Array>::Cast(args[1]);

    const uint32_t n = list->Length();
    for (uint32_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> name = list->Get(i);

      if (name->IsString()) {
        const string dbName = TRI_ObjectToString(name);

        if (! TRI_IsAllowedNameVocBase(true, dbName.c_str())) {
          TRI_V8_THROW_EXCEPTION_PARAMETER("<databases> must be a list of database names");
        }

        dbNames.push_back(dbName);
      }
      else {
        TRI_V8_THROW_EXCEPTION_PARAMETER("<databases> must be a list of database names");
      }
    }
  }

  bool result = s->addEndpoint(endpoint, dbNames, true);

  if (! result) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "unable to bind to endpoint");
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a new endpoint
///
/// @FUN{REMOVE_ENDPOINT}
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveEndpoint (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._removeEndpoint(<endpoint>)");
  }

  TRI_GET_GLOBALS();
  TRI_server_t* server = static_cast<TRI_server_t*>(v8g->_server);
  ApplicationEndpointServer* s = static_cast<ApplicationEndpointServer*>(server->_applicationEndpointServer);

  if (s == nullptr) {
    // not implemented in console mode
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (! TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  bool result = s->removeEndpoint(TRI_ObjectToString(args[0]));

  if (! result) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_ENDPOINT_NOT_FOUND);
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list of all endpoints
///
/// @FUN{LIST_ENDPOINTS}
////////////////////////////////////////////////////////////////////////////////

static void JS_ListEndpoints (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("db._listEndpoints()");
  }

  TRI_GET_GLOBALS();
  TRI_server_t* server = static_cast<TRI_server_t*>(v8g->_server);
  ApplicationEndpointServer* s = static_cast<ApplicationEndpointServer*>(server->_applicationEndpointServer);

  if (s == nullptr) {
    // not implemented in console mode
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (! TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  auto const& endpoints = s->getEndpoints();

  v8::Handle<v8::Array> result = v8::Array::New(isolate);
  uint32_t j = 0;

  map<string, vector<string> >::const_iterator it;
  for (it = endpoints.begin(); it != endpoints.end(); ++it) {
    v8::Handle<v8::Array> dbNames = v8::Array::New(isolate);

    for (uint32_t i = 0; i < (*it).second.size(); ++i) {
      dbNames->Set(i, TRI_V8_STD_STRING((*it).second.at(i)));
    }

    v8::Handle<v8::Object> item = v8::Object::New(isolate);
    item->Set(TRI_V8_ASCII_STRING("endpoint"), TRI_V8_STD_STRING((*it).first));
    item->Set(TRI_V8_ASCII_STRING("databases"), dbNames);

    result->Set(j++, item);
  }

  TRI_V8_RETURN(result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parse vertex handle from a v8 value (string | object)
////////////////////////////////////////////////////////////////////////////////

int TRI_ParseVertex (const v8::FunctionCallbackInfo<v8::Value>& args,
                     CollectionNameResolver const* resolver,
                     TRI_voc_cid_t& cid,
                     std::unique_ptr<char[]>& key,
                     v8::Handle<v8::Value> const val) {

  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_ASSERT(key.get() == nullptr);

  // reset everything
  string collectionName;
  TRI_voc_rid_t rid = 0;

  // try to extract the collection name, key, and revision from the object passed
  if (! ExtractDocumentHandle(isolate, val, collectionName, key, rid)) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  // we have at least a key, we also might have a collection name
  TRI_ASSERT(key.get() != nullptr);

  if (collectionName.empty()) {
    // we do not know the collection
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  if (ServerState::instance()->isDBserver()) {
    cid = resolver->getCollectionIdCluster(collectionName);
  }
  else {
    cid = resolver->getCollectionId(collectionName);
  }

  if (cid == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the private WRP_VOCBASE_COL_TYPE value
////////////////////////////////////////////////////////////////////////////////

int32_t TRI_GetVocBaseColType () {
  return WRP_VOCBASE_COL_TYPE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run version check
////////////////////////////////////////////////////////////////////////////////

bool TRI_UpgradeDatabase (TRI_vocbase_t* vocbase,
                          JSLoader* startupLoader,
                          v8::Handle<v8::Context> context) {
  TRI_ASSERT(startupLoader != nullptr);
  auto isolate = context->GetIsolate();

  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();
  TRI_vocbase_t* orig = v8g->_vocbase;
  v8g->_vocbase = vocbase;

  v8::Handle<v8::Value> result = startupLoader->executeGlobalScript(isolate, isolate->GetCurrentContext(), "server/upgrade-database.js");
  bool ok = TRI_ObjectToBoolean(result);

  if (! ok) {
    ((TRI_vocbase_t*) vocbase)->_state = (sig_atomic_t) TRI_VOCBASE_STATE_FAILED_VERSION;
  }

  v8g->_vocbase = orig;

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run upgrade check
////////////////////////////////////////////////////////////////////////////////

int TRI_CheckDatabaseVersion (TRI_vocbase_t* vocbase,
                              JSLoader* startupLoader,
                              v8::Handle<v8::Context> context) {
  TRI_ASSERT(startupLoader != nullptr);

  auto isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();
  TRI_vocbase_t* orig = v8g->_vocbase;
  v8g->_vocbase = vocbase;

  v8::Handle<v8::Value> result = startupLoader->executeGlobalScript(isolate, isolate->GetCurrentContext(), "server/check-version.js");
  int code = (int) TRI_ObjectToInt64(result);

  v8g->_vocbase = orig;

  return code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads routing
////////////////////////////////////////////////////////////////////////////////

void TRI_V8ReloadRouting (v8::Isolate *isolate) {
  TRI_ExecuteJavaScriptString(isolate,
                              isolate->GetCurrentContext(),
                              TRI_V8_ASCII_STRING("require('internal').executeGlobalContextFunction('reloadRouting')"),
                              TRI_V8_ASCII_STRING("reload routing"),
                              false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8VocBridge (v8::Isolate* isolate, 
                          triagens::arango::ApplicationV8* applicationV8,
                          v8::Handle<v8::Context> context,
                          triagens::aql::QueryRegistry* queryRegistry,
                          TRI_server_t* server,
                          TRI_vocbase_t* vocbase,
                          JSLoader* loader,
                          size_t threadNumber) {
  v8::HandleScope scope(isolate);

  // check the isolate
  TRI_v8_global_t* v8g = TRI_CreateV8Globals(isolate);

  TRI_ASSERT(v8g->_transactionContext == nullptr);
  v8g->_transactionContext = new V8TransactionContext(true);
  static_cast<V8TransactionContext*>(v8g->_transactionContext)->makeGlobal();

  // register the query registry
  v8g->_queryRegistry = queryRegistry;

  // register the server
  v8g->_server = server;

  // register the database
  v8g->_vocbase = vocbase;

  // register the startup loader
  v8g->_loader = loader;
  
  // register the context dealer
  v8g->_applicationV8 = applicationV8;

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

  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_version"), JS_VersionServer);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_id"), JS_IdDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_isSystem"), JS_IsSystemDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_name"), JS_NameDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_path"), JS_PathDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_createDatabase"), JS_CreateDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_dropDatabase"), JS_DropDatabase);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_listDatabases"), JS_ListDatabases);
  TRI_AddMethodVocbase(isolate, ArangoNS, TRI_V8_ASCII_STRING("_useDatabase"), JS_UseDatabase);

  TRI_InitV8indexArangoDB(isolate, ArangoNS);

  TRI_InitV8collection(context, server, vocbase, loader, threadNumber, v8g, isolate, ArangoNS);

  v8g->VocbaseTempl.Reset(isolate, ArangoNS);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("ArangoDatabase"), ft->GetFunction());

  TRI_InitV8ShapedJson(isolate, context, threadNumber, v8g);

  TRI_InitV8cursor(context, v8g);

  // .............................................................................
  // generate global functions
  // .............................................................................

  // AQL functions. not intended to be used directly by end users
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("AQL_EXECUTE"), JS_ExecuteAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("AQL_EXECUTEJSON"), JS_ExecuteAqlJson, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("AQL_EXPLAIN"), JS_ExplainAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("AQL_PARSE"), JS_ParseAql, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("AQL_WARNING"), JS_WarningAql, true);

  TRI_InitV8replication(isolate, context, server, vocbase, loader, threadNumber, v8g);

  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("COMPARE_STRING"), JS_compare_string);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("NORMALIZE_STRING"), JS_normalize_string);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("TIMEZONES"), JS_getIcuTimezones);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("LOCALES"), JS_getIcuLocales);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("FORMAT_DATETIME"), JS_formatDatetime);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("PARSE_DATETIME"), JS_parseDatetime);

  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("CONFIGURE_ENDPOINT"), JS_ConfigureEndpoint, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("REMOVE_ENDPOINT"), JS_RemoveEndpoint, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("LIST_ENDPOINTS"), JS_ListEndpoints, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("RELOAD_AUTH"), JS_ReloadAuth, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("TRANSACTION"), JS_Transaction, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("WAL_FLUSH"), JS_FlushWal, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("WAL_PROPERTIES"), JS_PropertiesWal, true);
  
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("ENABLE_NATIVE_BACKTRACES"), JS_EnableNativeBacktraces, true);

  // .............................................................................
  // create global variables
  // .............................................................................

  v8::Handle<v8::Object> v = WrapVocBase(isolate, vocbase);
  if (v.IsEmpty()) {
    // TODO: raise an error here
    LOG_ERROR("out of memory when initialising VocBase");
  }
  else {
    TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("db"), v);
  }
  
  // current thread number
  context->Global()->ForceSet(TRI_V8_ASCII_STRING("THREAD_NUMBER"), v8::Number::New(isolate, (double) threadNumber), v8::ReadOnly);
  
  // whether or not statistics are enabled
  context->Global()->ForceSet(TRI_V8_ASCII_STRING("ENABLE_STATISTICS"), v8::Boolean::New(isolate, TRI_ENABLE_STATISTICS), v8::ReadOnly);
  
  // a thread-global variable that will is supposed to contain the AQL module
  // do not remove this, otherwise AQL queries will break
  context->Global()->ForceSet(TRI_V8_ASCII_STRING("_AQL"), v8::Undefined(isolate), v8::DontEnum);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
