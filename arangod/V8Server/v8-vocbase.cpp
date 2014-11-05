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
#include "v8-voccursor.h"
#include "v8-collection.h"

#include "VocBase/general-cursor.h"

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
static v8::Handle<v8::Object> WrapClass (v8::Persistent<v8::ObjectTemplate> classTempl,
                                         int32_t type, T* y) {

  // handle scope for temporary handles
  v8::HandleScope scope;

  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = classTempl->NewInstance();

  if (result.IsEmpty()) {
    // error
    return scope.Close(result);
  }

  // set the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(type));
  result->SetInternalField(SLOT_CLASS, v8::External::New(y));

  return scope.Close(result);
}


// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a transaction
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Transaction (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 1 || ! argv[0]->IsObject()) {
    TRI_V8_EXCEPTION_USAGE(scope, "TRANSACTION(<object>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // treat the argument as an object from now on
  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(argv[0]);

  // extract the properties from the object

  // "lockTimeout"
  double lockTimeout = (double) (TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);

  if (object->Has(TRI_V8_SYMBOL("lockTimeout"))) {
    static const string timeoutError = "<lockTimeout> must be a valid numeric value";

    if (! object->Get(TRI_V8_SYMBOL("lockTimeout"))->IsNumber()) {
      TRI_V8_EXCEPTION_PARAMETER(scope, timeoutError);
    }

    lockTimeout = TRI_ObjectToDouble(object->Get(TRI_V8_SYMBOL("lockTimeout")));

    if (lockTimeout < 0.0) {
      TRI_V8_EXCEPTION_PARAMETER(scope, timeoutError);
    }
  }

  // "waitForSync"
  bool waitForSync = false;

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  if (object->Has(v8g->WaitForSyncKey)) {
    if (! object->Get(v8g->WaitForSyncKey)->IsBoolean() &&
        ! object->Get(v8g->WaitForSyncKey)->IsBooleanObject()) {
      TRI_V8_EXCEPTION_PARAMETER(scope, "<waitForSync> must be a boolean value");
    }

    waitForSync = TRI_ObjectToBoolean(v8g->WaitForSyncKey);
  }

  // "collections"
  static string const collectionError = "missing/invalid collections definition for transaction";

  if (! object->Has(TRI_V8_SYMBOL("collections")) || ! object->Get(TRI_V8_SYMBOL("collections"))->IsObject()) {
    TRI_V8_EXCEPTION_PARAMETER(scope, collectionError);
  }

  // extract collections
  v8::Handle<v8::Array> collections = v8::Handle<v8::Array>::Cast(object->Get(TRI_V8_SYMBOL("collections")));

  if (collections.IsEmpty()) {
    TRI_V8_EXCEPTION_PARAMETER(scope, collectionError);
  }

  bool isValid = true;
  vector<string> readCollections;
  vector<string> writeCollections;

  // collections.read
  if (collections->Has(TRI_V8_SYMBOL("read"))) {
    if (collections->Get(TRI_V8_SYMBOL("read"))->IsArray()) {
      v8::Handle<v8::Array> names = v8::Handle<v8::Array>::Cast(collections->Get(TRI_V8_SYMBOL("read")));

      for (uint32_t i = 0 ; i < names->Length(); ++i) {
        v8::Handle<v8::Value> collection = names->Get(i);
        if (! collection->IsString()) {
          isValid = false;
          break;
        }

        readCollections.push_back(TRI_ObjectToString(collection));
      }
    }
    else if (collections->Get(TRI_V8_SYMBOL("read"))->IsString()) {
      readCollections.push_back(TRI_ObjectToString(collections->Get(TRI_V8_SYMBOL("read"))));
    }
    else {
      isValid = false;
    }
  }

  // collections.write
  if (collections->Has(TRI_V8_SYMBOL("write"))) {
    if (collections->Get(TRI_V8_SYMBOL("write"))->IsArray()) {
      v8::Handle<v8::Array> names = v8::Handle<v8::Array>::Cast(collections->Get(TRI_V8_SYMBOL("write")));

      for (uint32_t i = 0 ; i < names->Length(); ++i) {
        v8::Handle<v8::Value> collection = names->Get(i);
        if (! collection->IsString()) {
          isValid = false;
          break;
        }

        writeCollections.push_back(TRI_ObjectToString(collection));
      }
    }
    else if (collections->Get(TRI_V8_SYMBOL("write"))->IsString()) {
      writeCollections.push_back(TRI_ObjectToString(collections->Get(TRI_V8_SYMBOL("write"))));
    }
    else {
      isValid = false;
    }
  }

  if (! isValid) {
    TRI_V8_EXCEPTION_PARAMETER(scope, collectionError);
  }

  // extract the "action" property
  static const string actionError = "missing/invalid action definition for transaction";

  if (! object->Has(TRI_V8_SYMBOL("action"))) {
    TRI_V8_EXCEPTION_PARAMETER(scope, actionError);
  }

  // function parameters
  v8::Handle<v8::Value> params;

  if (object->Has(TRI_V8_SYMBOL("params"))) {
    params = v8::Handle<v8::Array>::Cast(object->Get(TRI_V8_SYMBOL("params")));
  }
  else {
    params = v8::Undefined();
  }

  if (params.IsEmpty()) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }


  v8::Handle<v8::Object> current = v8::Context::GetCurrent()->Global();

  // callback function
  v8::Handle<v8::Function> action;

  if (object->Get(TRI_V8_SYMBOL("action"))->IsFunction()) {
    action = v8::Handle<v8::Function>::Cast(object->Get(TRI_V8_SYMBOL("action")));
  }
  else if (object->Get(TRI_V8_SYMBOL("action"))->IsString()) {
    // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
    v8::Local<v8::Function> ctor = v8::Local<v8::Function>::Cast(current->Get(v8::String::New("Function")));

    // Invoke Function constructor to create function with the given body and no arguments
    string body = TRI_ObjectToString(object->Get(TRI_V8_SYMBOL("action"))->ToString());
    body = "return (" + body + ")(params);";
    v8::Handle<v8::Value> args[2] = { v8::String::New("params"), v8::String::New(body.c_str(), (int) body.size()) };
    v8::Local<v8::Object> function = ctor->NewInstance(2, args);

    action = v8::Local<v8::Function>::Cast(function);
  }
  else {
    TRI_V8_EXCEPTION_PARAMETER(scope, actionError);
  }

  if (action.IsEmpty()) {
    TRI_V8_EXCEPTION_PARAMETER(scope, actionError);
  }


  // start actual transaction
  ExplicitTransaction trx(vocbase,
                          readCollections,
                          writeCollections,
                          lockTimeout,
                          waitForSync);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  v8::Handle<v8::Value> args = params;
  v8::Handle<v8::Value> result = action->Call(current, 1, &args);

  if (tryCatch.HasCaught()) {
    trx.abort();

    if (tryCatch.CanContinue()) {
      return scope.Close(v8::ThrowException(tryCatch.Exception()));
    }
    else {
      v8g->_canceled = true;
      return scope.Close(result);
    }
  }

  res = trx.commit();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  return scope.Close(result);
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

static v8::Handle<v8::Value> JS_PropertiesWal (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() > 1 || (argv.Length() == 1 && ! argv[0]->IsObject())) {
    TRI_V8_EXCEPTION_USAGE(scope, "properties(<object>)");
  }
  
  auto l = triagens::wal::LogfileManager::instance();

  if (argv.Length() == 1) {
    // set the properties
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(argv[0]);
    if (object->Has(TRI_V8_STRING("allowOversizeEntries"))) {
      bool value = TRI_ObjectToBoolean(object->Get(TRI_V8_STRING("allowOversizeEntries")));
      l->allowOversizeEntries(value);
    }
    
    if (object->Has(TRI_V8_STRING("logfileSize"))) {
      uint32_t value = static_cast<uint32_t>(TRI_ObjectToUInt64(object->Get(TRI_V8_STRING("logfileSize")), true));
      l->filesize(value);
    }

    if (object->Has(TRI_V8_STRING("historicLogfiles"))) {
      uint32_t value = static_cast<uint32_t>(TRI_ObjectToUInt64(object->Get(TRI_V8_STRING("historicLogfiles")), true));
      l->historicLogfiles(value);
    }
    
    if (object->Has(TRI_V8_STRING("reserveLogfiles"))) {
      uint32_t value = static_cast<uint32_t>(TRI_ObjectToUInt64(object->Get(TRI_V8_STRING("reserveLogfiles")), true));
      l->reserveLogfiles(value);
    }
    
    if (object->Has(TRI_V8_STRING("throttleWait"))) {
      uint64_t value = TRI_ObjectToUInt64(object->Get(TRI_V8_STRING("throttleWait")), true);
      l->maxThrottleWait(value);
    }
  
    if (object->Has(TRI_V8_STRING("throttleWhenPending"))) {
      uint64_t value = TRI_ObjectToUInt64(object->Get(TRI_V8_STRING("throttleWhenPending")), true);
      l->throttleWhenPending(value);
    }
  }

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(TRI_V8_STRING("allowOversizeEntries"), v8::Boolean::New(l->allowOversizeEntries()));
  result->Set(TRI_V8_STRING("logfileSize"), v8::Number::New(l->filesize()));
  result->Set(TRI_V8_STRING("historicLogfiles"), v8::Number::New(l->historicLogfiles()));
  result->Set(TRI_V8_STRING("reserveLogfiles"), v8::Number::New(l->reserveLogfiles()));
  result->Set(TRI_V8_STRING("syncInterval"), v8::Number::New((double) l->syncInterval()));
  result->Set(TRI_V8_STRING("throttleWait"), v8::Number::New((double) l->maxThrottleWait()));
  result->Set(TRI_V8_STRING("throttleWhenPending"), v8::Number::New((double) l->throttleWhenPending()));

  return scope.Close(result);
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

static v8::Handle<v8::Value> JS_FlushWal (v8::Arguments const& argv) {
  v8::HandleScope scope;

  bool waitForSync = false;
  if (argv.Length() > 0) {
    waitForSync = TRI_ObjectToBoolean(argv[0]);
  }

  bool waitForCollector = false;
  if (argv.Length() > 1) {
    waitForCollector = TRI_ObjectToBoolean(argv[1]);
  }

  int res;

  if (ServerState::instance()->isCoordinator()) {
    res = flushWalOnAllDBServers( waitForSync, waitForCollector );
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_EXCEPTION(scope, res);
    }
    return scope.Close(v8::True());
  }

  res = triagens::wal::LogfileManager::instance()->flush(waitForSync, waitForCollector, false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_normalize_string (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "NORMALIZE_STRING(<string>)");
  }

  return scope.Close(TRI_normalize_V8_Obj(argv[0]));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_compare_string (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "COMPARE_STRING(<left string>, <right string>)");
  }

  v8::String::Value left(argv[0]);
  v8::String::Value right(argv[1]);

  // ..........................................................................
  // Take note here: we are assuming that the ICU type UChar is two bytes.
  // There is no guarantee that this will be the case on all platforms and
  // compilers.
  // ..........................................................................
  int result = Utf8Helper::DefaultUtf8Helper.compareUtf16(*left, left.length(), *right, right.length());

  return scope.Close(v8::Integer::New(result));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of timezones
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_getIcuTimezones (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "TIMEZONES()");
  }

  v8::Handle<v8::Array> result = v8::Array::New();

  UErrorCode status = U_ZERO_ERROR;

  StringEnumeration* timeZones = TimeZone::createEnumeration();
  if (timeZones) {
    int32_t idsCount = timeZones->count(status);

    for (int32_t i = 0; i < idsCount && U_ZERO_ERROR == status; ++i) {
      int32_t resultLength;
      const char* str = timeZones->next(&resultLength, status);
      result->Set(i, v8::String::New(str, resultLength));
    }

    delete timeZones;
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of locales
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_getIcuLocales (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "LOCALES()");
  }

  v8::Handle<v8::Array> result = v8::Array::New();

  int32_t count = 0;
  const Locale* locales = Locale::getAvailableLocales(count);
  if (locales) {

    for (int32_t i = 0; i < count; ++i) {
      const Locale* l = locales + i;
      const char* str = l->getBaseName();

      result->Set(i, v8::String::New(str, (int) strlen(str)));
    }
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief format datetime
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_formatDatetime (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "FORMAT_DATETIME(<datetime in sec>, <pattern>, [<timezone>, [<locale>]])");
  }

  int64_t datetime = TRI_ObjectToInt64(argv[0]);
  v8::String::Value pattern(argv[1]);

  TimeZone* tz = 0;
  if (argv.Length() > 2) {
    v8::String::Value value(argv[2]);

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
  if (argv.Length() > 3) {
    string name = TRI_ObjectToString(argv[3]);
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

  return scope.Close(v8::String::New(resultString.c_str(), (int) resultString.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse datetime
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_parseDatetime (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "PARSE_DATETIME(<datetime string>, <pattern>, [<timezone>, [<locale>]])");
  }

  v8::String::Value datetimeString(argv[0]);
  v8::String::Value pattern(argv[1]);

  TimeZone* tz = 0;
  if (argv.Length() > 2) {
    v8::String::Value value(argv[2]);

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
  if (argv.Length() > 3) {
    string name = TRI_ObjectToString(argv[3]);
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

  return scope.Close(v8::Number::New(udate / 1000));
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

static v8::Handle<v8::Value> JS_ReloadAuth (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "RELOAD_AUTH()");
  }

  bool result;
  if (ServerState::instance()->isCoordinator()) {
    result = ReloadAuthCoordinator(vocbase);
  }
  else {
    result = TRI_ReloadAuthInfo(vocbase);
  }

  return scope.Close(v8::Boolean::New(result));
}


// -----------------------------------------------------------------------------
// --SECTION--                                                               AQL
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an AQL query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ParseAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "AQL_PARSE(<querystring>)");
  }

  // get the query string
  if (! argv[0]->IsString()) {
    TRI_V8_TYPE_ERROR(scope, "expecting string for <querystring>");
  }

  string const&& queryString = TRI_ObjectToString(argv[0]);

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  triagens::aql::Query query(v8g->_applicationV8, true, vocbase, queryString.c_str(), queryString.size(), nullptr, nullptr, triagens::aql::PART_MAIN);

  auto parseResult = query.parse();

  if (parseResult.code != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_FULL(scope, parseResult.code, parseResult.details);
  }

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8::String::New("parsed"), v8::True());

  {
    v8::Handle<v8::Array> collections = v8::Array::New();
    result->Set(TRI_V8_STRING("collections"), collections);
    uint32_t i = 0;
    for (auto it = parseResult.collectionNames.begin(); it != parseResult.collectionNames.end(); ++it) {
      collections->Set(i++, v8::String::New((*it).c_str()));
    }
  }

  {
    v8::Handle<v8::Array> bindVars = v8::Array::New();
    uint32_t i = 0;
    for (auto it = parseResult.bindParameters.begin(); it != parseResult.bindParameters.end(); ++it) {
      bindVars->Set(i++, v8::String::New((*it).c_str()));
    }
    result->Set(TRI_V8_STRING("parameters"), bindVars); 
  }

  result->Set(TRI_V8_STRING("ast"), TRI_ObjectJson(parseResult.json));
    
  if (parseResult.warnings == nullptr) {
    result->Set(TRI_V8_STRING("warnings"), v8::Array::New());
  }
  else {
    result->Set(TRI_V8_STRING("warnings"), TRI_ObjectJson(parseResult.warnings));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a warning for the currently running AQL query
/// this function is called from aql.js
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WarningAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "AQL_WARNING(<code>, <message>)");
  }

  // get the query string
  if (! argv[1]->IsString()) {
    TRI_V8_TYPE_ERROR(scope, "expecting string for <message>");
  }
  
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  if (v8g->_query != nullptr) {
    // only register the error if we have a query...
    // note: we may not have a query if the AQL functions are called without
    // a query, e.g. during tests
    int code = static_cast<int>(TRI_ObjectToInt64(argv[0]));
    std::string const&& message = TRI_ObjectToString(argv[1]);

    auto query = static_cast<triagens::aql::Query*>(v8g->_query);
    query->registerWarning(code, message.c_str());
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explains an AQL query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExplainAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  
  if (argv.Length() < 1 || argv.Length() > 3) {
    TRI_V8_EXCEPTION_USAGE(scope, "AQL_EXPLAIN(<querystring>, <bindvalues>, <options>)");
  }

  // get the query string
  if (! argv[0]->IsString()) {
    TRI_V8_TYPE_ERROR(scope, "expecting string for <querystring>");
  }

  string const&& queryString = TRI_ObjectToString(argv[0]);

  // bind parameters
  TRI_json_t* parameters = nullptr;
  
  if (argv.Length() > 1) {
    if (! argv[1]->IsUndefined() && ! argv[1]->IsNull() && ! argv[1]->IsObject()) {
      TRI_V8_TYPE_ERROR(scope, "expecting object for <bindvalues>");
    }
    if (argv[1]->IsObject()) {
      parameters = TRI_ObjectToJson(argv[1]);
    }
  }

  TRI_json_t* options = nullptr;

  if (argv.Length() > 2) {
    // handle options
    if (! argv[2]->IsObject()) {
      if (parameters != nullptr) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);
      }
      TRI_V8_TYPE_ERROR(scope, "expecting object for <options>");
    }

    options = TRI_ObjectToJson(argv[2]);
  }

  // bind parameters will be freed by the query later
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  triagens::aql::Query query(v8g->_applicationV8, true, vocbase, queryString.c_str(), queryString.size(), parameters, options, triagens::aql::PART_MAIN);
  
  auto queryResult = query.explain();
  
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_FULL(scope, queryResult.code, queryResult.details);
  }
  
  v8::Handle<v8::Object> result = v8::Object::New();
  if (queryResult.json != nullptr) {
    if (query.allPlans()) {
      result->Set(TRI_V8_STRING("plans"), TRI_ObjectJson(queryResult.json));
    }
    else {
      result->Set(TRI_V8_STRING("plan"), TRI_ObjectJson(queryResult.json));
    }
    if (queryResult.clusterplan != nullptr) {
      result->Set(TRI_V8_STRING("clusterplans"), TRI_ObjectJson(queryResult.clusterplan));
    }

    if (queryResult.warnings == nullptr) {
      result->Set(TRI_V8_STRING("warnings"), v8::Array::New());
    }
    else {
      result->Set(TRI_V8_STRING("warnings"), TRI_ObjectJson(queryResult.warnings));
    }
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an AQL query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExecuteAqlJson (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  
  if (argv.Length() < 1 || argv.Length() > 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "AQL_EXECUTEJSON(<queryjson>, <options>)");
  }
  
  // return number of total records in cursor?
  bool doCount = false;

  // maximum number of results to return at once
  size_t batchSize = SIZE_MAX;
  
  // ttl for cursor  
  double ttl = 0.0;
  
  if (! argv[0]->IsObject()) {
    TRI_V8_TYPE_ERROR(scope, "expecting object for <queryjson>");
  }
  TRI_json_t* queryjson = TRI_ObjectToJson(argv[0]);
  TRI_json_t* options = nullptr;

  if (argv.Length() > 1) {
    // we have options! yikes!
    if (! argv[1]->IsUndefined() && ! argv[1]->IsObject()) {
      TRI_V8_TYPE_ERROR(scope, "expecting object for <options>");
    }

    v8::Handle<v8::Object> argValue = v8::Handle<v8::Object>::Cast(argv[1]);
      
    v8::Handle<v8::String> optionName = v8::String::New("batchSize");
    if (argValue->Has(optionName)) {
      batchSize = static_cast<decltype(batchSize)>(TRI_ObjectToInt64(argValue->Get(optionName)));
      if (batchSize == 0) {
        TRI_V8_TYPE_ERROR(scope, "expecting non-zero value for <batchSize>");
        // well, this makes no sense
      }
    }
      
    optionName = v8::String::New("count");
    if (argValue->Has(optionName)) {
      doCount = TRI_ObjectToBoolean(argValue->Get(optionName));
    }
      
    optionName = v8::String::New("ttl");
    if (argValue->Has(optionName)) {
      ttl = TRI_ObjectToBoolean(argValue->Get(optionName));
      ttl = (ttl <= 0.0 ? 30.0 : ttl);
    }
      
    options = TRI_ObjectToJson(argv[1]);
  }

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  triagens::aql::Query query(v8g->_applicationV8, true, vocbase, Json(TRI_UNKNOWN_MEM_ZONE, queryjson), options, triagens::aql::PART_MAIN);

  auto queryResult = query.execute(static_cast<triagens::aql::QueryRegistry*>(v8g->_queryRegistry));
  
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_FULL(scope, queryResult.code, queryResult.details);
  }
  
  if (TRI_LengthListJson(queryResult.json) <= batchSize) {
    // return the array value as it is. this is a performance optimisation
    v8::Handle<v8::Object> result = v8::Object::New();
    if (queryResult.json != nullptr) {
      result->Set(TRI_V8_STRING("json"), TRI_ObjectJson(queryResult.json));
    }
    if (queryResult.stats != nullptr) {
      result->Set(TRI_V8_STRING("stats"), TRI_ObjectJson(queryResult.stats));
    }
    if (queryResult.profile != nullptr) {
      result->Set(TRI_V8_STRING("profile"), TRI_ObjectJson(queryResult.profile));
    }
    if (queryResult.warnings == nullptr) {
      result->Set(TRI_V8_STRING("warnings"), v8::Array::New());
    }
    else {
      result->Set(TRI_V8_STRING("warnings"), TRI_ObjectJson(queryResult.warnings));
    }
    return scope.Close(result);
  }
  
  TRI_json_t* extra = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  if (extra == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  if (queryResult.warnings != nullptr) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, extra, "warnings", queryResult.warnings);
    queryResult.warnings = nullptr;
  }
  if (queryResult.stats != nullptr) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, extra, "stats", queryResult.stats);
    queryResult.stats = nullptr;
  }
  if (queryResult.profile != nullptr) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, extra, "profile", queryResult.profile);
    queryResult.profile = nullptr;
  }

  TRI_general_cursor_result_t* cursorResult = TRI_CreateResultGeneralCursor(queryResult.json);
  
  if (cursorResult == nullptr){
    if (extra != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, extra);
    }
    TRI_V8_EXCEPTION_MEMORY(scope);
  }
  
  queryResult.json = nullptr;
  
  TRI_general_cursor_t* cursor = TRI_CreateGeneralCursor(vocbase, cursorResult, doCount,
      static_cast<TRI_general_cursor_length_t>(batchSize), ttl, extra);

  if (cursor == nullptr) {
    if (extra != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, extra);
    }
    TRI_FreeCursorResult(cursorResult);
    TRI_V8_EXCEPTION_MEMORY(scope);
  }
  TRI_ASSERT(cursor != nullptr);
  
  v8::Handle<v8::Value> cursorObject = TRI_WrapGeneralCursor(cursor);

  if (cursorObject.IsEmpty()) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  return scope.Close(cursorObject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an AQL query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExecuteAql (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  
  if (argv.Length() < 1 || argv.Length() > 3) {
    TRI_V8_EXCEPTION_USAGE(scope, "AQL_EXECUTE(<querystring>, <bindvalues>, <options>)");
  }

  // get the query string
  if (! argv[0]->IsString()) {
    TRI_V8_TYPE_ERROR(scope, "expecting string for <querystring>");
  }

  string const&& queryString = TRI_ObjectToString(argv[0]);

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

  if (argv.Length() > 1) {
    if (! argv[1]->IsUndefined() && ! argv[1]->IsNull() && ! argv[1]->IsObject()) {
      TRI_V8_TYPE_ERROR(scope, "expecting object for <bindvalues>");
    }
    if (argv[1]->IsObject()) {
      parameters = TRI_ObjectToJson(argv[1]);
    }
  }
    
  if (argv.Length() > 2) {
    // we have options! yikes!
    if (! argv[2]->IsObject()) {
      TRI_V8_TYPE_ERROR(scope, "expecting object for <options>");
    }

    v8::Handle<v8::Object> argValue = v8::Handle<v8::Object>::Cast(argv[2]);
      
    v8::Handle<v8::String> optionName = v8::String::New("batchSize");
    if (argValue->Has(optionName)) {
      batchSize = static_cast<decltype(batchSize)>(TRI_ObjectToInt64(argValue->Get(optionName)));
      if (batchSize == 0) {
        TRI_V8_TYPE_ERROR(scope, "expecting non-zero value for <batchSize>");
        // well, this makes no sense
      }
    }
      
    optionName = v8::String::New("count");
    if (argValue->Has(optionName)) {
      doCount = TRI_ObjectToBoolean(argValue->Get(optionName));
    }
      
    optionName = v8::String::New("ttl");
    if (argValue->Has(optionName)) {
      ttl = TRI_ObjectToBoolean(argValue->Get(optionName));
      ttl = (ttl <= 0.0 ? 30.0 : ttl);
    }
      
    options = TRI_ObjectToJson(argv[2]);
  }
      
  // bind parameters will be freed by the query later
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  triagens::aql::Query query(v8g->_applicationV8, true, vocbase, queryString.c_str(), queryString.size(), parameters, options, triagens::aql::PART_MAIN);

  auto queryResult = query.executeV8(static_cast<triagens::aql::QueryRegistry*>(v8g->_queryRegistry));
  
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED) {
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
      v8g->_canceled = true;
      return scope.Close(TRI_CreateErrorObject(__FILE__, __LINE__, TRI_ERROR_REQUEST_CANCELED));
    }

    TRI_V8_EXCEPTION_FULL(scope, queryResult.code, queryResult.details);
  }
  
  if (queryResult.result->Length() <= batchSize) {
    // return the array value as it is. this is a performance optimisation
    v8::Handle<v8::Object> result = v8::Object::New();

    result->Set(TRI_V8_STRING("json"), queryResult.result);

    if (queryResult.stats != nullptr) {
      result->Set(TRI_V8_STRING("stats"), TRI_ObjectJson(queryResult.stats));
    }
    if (queryResult.profile != nullptr) {
      result->Set(TRI_V8_STRING("profile"), TRI_ObjectJson(queryResult.profile));
    }
    if (queryResult.warnings == nullptr) {
      result->Set(TRI_V8_STRING("warnings"), v8::Array::New());
    }
    else {
      result->Set(TRI_V8_STRING("warnings"), TRI_ObjectJson(queryResult.warnings));
    }
    return scope.Close(result);
  }
  
  TRI_json_t* extra = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  if (extra == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  if (queryResult.warnings != nullptr) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, extra, "warnings", queryResult.warnings);
    queryResult.warnings = nullptr;
  }
  if (queryResult.stats != nullptr) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, extra, "stats", queryResult.stats);
    queryResult.stats = nullptr;
  }
  if (queryResult.profile != nullptr) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, extra, "profile", queryResult.profile);
    queryResult.profile = nullptr;
  }

  TRI_general_cursor_result_t* cursorResult = TRI_CreateResultGeneralCursor(queryResult.result);
  
  if (cursorResult == nullptr) {
    if (extra != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, extra);
    }
    TRI_V8_EXCEPTION_MEMORY(scope);
  }
  
  TRI_general_cursor_t* cursor = TRI_CreateGeneralCursor(vocbase, cursorResult, doCount,
      static_cast<TRI_general_cursor_length_t>(batchSize), ttl, extra);

  if (cursor == nullptr) {
    if (extra != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, extra);
    }
    TRI_FreeCursorResult(cursorResult);
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  TRI_ASSERT(cursor != nullptr);
  
  v8::Handle<v8::Value> cursorObject = TRI_WrapGeneralCursor(cursor);

  if (cursorObject.IsEmpty()) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  return scope.Close(cursorObject);
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

static v8::Handle<v8::Object> WrapVocBase (TRI_vocbase_t const* database) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  v8::Handle<v8::Object> result = WrapClass(v8g->VocbaseTempl,
                                            WRP_VOCBASE_TYPE,
                                            const_cast<TRI_vocbase_t*>(database));

  return scope.Close(result);
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

static v8::Handle<v8::Value> MapGetVocBase (v8::Local<v8::String> const name,
                                            const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
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
    return scope.Close(v8::Handle<v8::Value>());
  }

  if (strcmp(key, "hasOwnProperty") == 0 ||  // this prevents calling the property getter again (i.e. recursion!)
      strcmp(key, "toString") == 0 ||
      strcmp(key, "toJSON") == 0) {
    return scope.Close(v8::Handle<v8::Value>());
  }

  TRI_vocbase_col_t* collection = nullptr;

  // generate a name under which the cached property is stored
  string cacheKey(key, keyLength);
  cacheKey.push_back('*');

  v8::Local<v8::String> cacheName = v8::String::New(cacheKey.c_str(), (int) cacheKey.size());
  v8::Handle<v8::Object> holder = info.Holder()->ToObject();

  if (*key == '_') {
    // special treatment for all properties starting with _
    v8::Local<v8::String> const l = v8::String::New(key, (int) keyLength);

    if (holder->HasRealNamedProperty(l)) {
      // some internal function inside db
      return scope.Close(v8::Handle<v8::Value>());
    }

    // something in the prototype chain?
    v8::Local<v8::Value> v = holder->GetRealNamedPropertyInPrototypeChain(l);

    if (! v.IsEmpty()) {
      if (! v->IsExternal()) {
        // something but an external... this means we can directly return this
        return scope.Close(v8::Handle<v8::Value>());
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
        TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

        if (value->Has(v8g->_IdKey)) {
          TRI_voc_cid_t cachedCid = static_cast<TRI_voc_cid_t>(TRI_ObjectToUInt64(value->Get(v8g->_IdKey), true));
          uint32_t cachedVersion = (uint32_t) TRI_ObjectToInt64(value->Get(v8g->VersionKey));

          if (cachedCid == cid && cachedVersion == internalVersion) {
            // cache hit
            return scope.Close(value);
          }

          // store the updated version number in the object for future comparisons
          value->Set(v8g->VersionKey, v8::Number::New((double) internalVersion), v8::DontEnum);

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
        return scope.Close(v8::Handle<v8::Value>());
      }
    }
  }
  else {
    collection = TRI_LookupCollectionByNameVocBase(vocbase, key);
  }

  if (collection == nullptr) {
    if (*key == '_') {
      return scope.Close(v8::Handle<v8::Value>());
    }

    return scope.Close(v8::Undefined());
  }

  v8::Handle<v8::Value> result = WrapCollection(collection);

  if (result.IsEmpty()) {
    return scope.Close(v8::Undefined());
  }

  // TODO: caching the result makes subsequent results much faster, but
  // prevents physical removal of the collection or database
  holder->Set(cacheName, result, v8::DontEnum);

  return scope.Close(result);
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

static v8::Handle<v8::Value> JS_VersionServer (v8::Arguments const& argv) {
  v8::HandleScope scope;

  return scope.Close(v8::String::New(TRI_VERSION));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the path to database files
/// @startDocuBlock databasePath
/// `db._path()`
///
/// Returns the filesystem path of the current database as a string.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PathDatabase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  return scope.Close(v8::String::New(vocbase->_path));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database id
/// @startDocuBlock databaseId
/// `db._id()`
///
/// Returns the id of the current database as a string.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IdDatabase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  return scope.Close(V8TickId(vocbase->_id));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database name
/// @startDocuBlock databaseName
/// `db._name()`
///
/// Returns the name of the current database as a string.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NameDatabase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  return scope.Close(v8::String::New(vocbase->_name));
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

static v8::Handle<v8::Value> JS_IsSystemDatabase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  return scope.Close(v8::Boolean::New(TRI_IsSystemVocBase(vocbase)));
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

static v8::Handle<v8::Value> JS_UseDatabase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "db._useDatabase(<name>)");
  }

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  if (! v8g->_allowUseDatabase) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_FORBIDDEN);
  }

  string const name = TRI_ObjectToString(argv[0]);

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  if (TRI_EqualString(name.c_str(), vocbase->_name)) {
    // same database. nothing to do
    return scope.Close(WrapVocBase(vocbase));
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

    return scope.Close(WrapVocBase(vocbase));
  }

  TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all existing databases in a coordinator
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ListDatabasesCoordinator (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // Arguments are already checked, there are 0 or 3.

  ClusterInfo* ci = ClusterInfo::instance();

  if (argv.Length() == 0) {
    vector<DatabaseID> list = ci->listDatabases(true);
    v8::Handle<v8::Array> result = v8::Array::New();
    for (size_t i = 0;  i < list.size();  ++i) {
      result->Set((uint32_t) i, v8::String::New(list[i].c_str(),
                                                (int) list[i].size()));
    }
    return scope.Close(result);
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
        headers["Authentication"] = TRI_ObjectToString(argv[2]);
        res = cc->syncRequest("", 0, "server:" + sid,
                              triagens::rest::HttpRequest::HTTP_REQUEST_GET,
                              "/_api/database/user", string(""), headers, 0.0);

        if (res->status == CL_COMM_SENT) {
          // We got an array back as JSON, let's parse it and build a v8
          StringBuffer& body = res->result->getBody();

          TRI_json_t* json = JsonHelper::fromString(body.c_str());
          delete res;

          if (json != 0 && JsonHelper::isArray(json)) {
            TRI_json_t const* dotresult = JsonHelper::getArrayElement(json, "result");

            if (dotresult != 0) {
              vector<string> list = JsonHelper::stringList(dotresult);
              TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
              v8::Handle<v8::Array> result = v8::Array::New();
              for (size_t i = 0;  i < list.size();  ++i) {
                result->Set((uint32_t) i, v8::String::New(list[i].c_str(),
                                                          (int) list[i].size()));
              }
              return scope.Close(result);
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
    return scope.Close(v8::Undefined());
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

static v8::Handle<v8::Value> JS_ListDatabases (v8::Arguments const& argv) {
  v8::HandleScope scope;

  const uint32_t argc = argv.Length();
  if (argc != 0 && argc != 3) {
    TRI_V8_EXCEPTION_USAGE(scope, "db._listDatabases()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (argc == 0 &&
      ! TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(ListDatabasesCoordinator(argv));
  }

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  TRI_vector_string_t names;
  TRI_InitVectorString(&names, TRI_UNKNOWN_MEM_ZONE);

  int res;

  if (argc == 0) {
    // return all databases
    res = TRI_GetDatabaseNamesServer(static_cast<TRI_server_t*>(v8g->_server), &names);
  }
  else {
    // return all databases for a specific user
    string username = TRI_ObjectToString(argv[0]);
    string password = TRI_ObjectToString(argv[1]);
    res = TRI_GetUserDatabasesServer((TRI_server_t*) v8g->_server, username.c_str(), password.c_str(), &names);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVectorString(&names);
    TRI_V8_EXCEPTION(scope, res);
  }

  v8::Handle<v8::Array> result = v8::Array::New();
  for (size_t i = 0;  i < names._length;  ++i) {
    result->Set((uint32_t) i, v8::String::New((char const*) TRI_AtVectorString(&names, i)));
  }

  TRI_DestroyVectorString(&names);

  return scope.Close(result);
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

static v8::Handle<v8::Value> CreateDatabaseCoordinator (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // First work with the arguments to create a JSON entry:
  string const name = TRI_ObjectToString(argv[0]);

  if (! TRI_IsAllowedNameVocBase(false, name.c_str())) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
  }

  TRI_json_t* json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (nullptr == json) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  uint64_t const id = ClusterInfo::instance()->uniqid();

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "id",
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE,
                               StringUtils::itoa(id).c_str()));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "name",
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE,
                               TRI_ObjectToString(argv[0]).c_str()));
  if (argv.Length() > 1) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "options",
                         TRI_ObjectToJson(argv[1]));
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "coordinator",
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, ServerState::instance()->getId().c_str()));

  ClusterInfo* ci = ClusterInfo::instance();
  string errorMsg;

  int res = ci->createDatabaseCoordinator( name, json, errorMsg, 120.0);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, errorMsg);
  }

  // database was created successfully in agency

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

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
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  // now run upgrade and copy users into context
  if (argv.Length() >= 3 && argv[2]->IsArray()) {
    v8::Handle<v8::Object> users = v8::Object::New();
    users->Set(v8::String::New("users"), argv[2]);

    v8::Context::GetCurrent()->Global()->Set(v8::String::New("UPGRADE_ARGS"), users);
  }
  else {
    v8::Context::GetCurrent()->Global()->Set(v8::String::New("UPGRADE_ARGS"), v8::Object::New());
  }

  // switch databases
  TRI_vocbase_t* orig = v8g->_vocbase;
  TRI_ASSERT(orig != nullptr);

  v8g->_vocbase = vocbase;

  // initalise database
  bool allowUseDatabase = v8g->_allowUseDatabase;
  v8g->_allowUseDatabase = true;

  v8g->_loader->executeGlobalScript(v8::Context::GetCurrent(), "server/bootstrap/coordinator-database.js");
  
  v8g->_allowUseDatabase = allowUseDatabase;

  // and switch back
  v8g->_vocbase = orig;

  TRI_ReleaseVocBase(vocbase);

  return scope.Close(v8::True());
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

static v8::Handle<v8::Value> JS_CreateDatabase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1 || argv.Length() > 3) {
    TRI_V8_EXCEPTION_USAGE(scope, "db._createDatabase(<name>, <options>, <users>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_READ_ONLY);
  }

  if (! TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(CreateDatabaseCoordinator(argv));
  }


  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  TRI_voc_tick_t id = 0;

  // get database defaults from server
  TRI_vocbase_defaults_t defaults;
  TRI_GetDatabaseDefaultsServer(static_cast<TRI_server_t*>(v8g->_server), &defaults);

  v8::Local<v8::String> keyDefaultMaximalSize = v8::String::New("defaultMaximalSize");
  v8::Local<v8::String> keyDefaultWaitForSync = v8::String::New("defaultWaitForSync");
  v8::Local<v8::String> keyRequireAuthentication = v8::String::New("requireAuthentication");
  v8::Local<v8::String> keyRequireAuthenticationUnixSockets = v8::String::New("requireAuthenticationUnixSockets");
  v8::Local<v8::String> keyAuthenticateSystemOnly = v8::String::New("authenticateSystemOnly");
  v8::Local<v8::String> keyForceSyncProperties = v8::String::New("forceSyncProperties");

  // overwrite database defaults from argv[2]
  if (argv.Length() > 1 && argv[1]->IsObject()) {
    v8::Handle<v8::Object> options = argv[1]->ToObject();

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
    
    if (options->Has(v8g->IdKey)) {
      // only used for testing to create database with a specific id
      id = TRI_ObjectToUInt64(options->Get(v8g->IdKey), true);
    }
  }

  string const name = TRI_ObjectToString(argv[0]);

  TRI_vocbase_t* database;
  int res = TRI_CreateDatabaseServer(static_cast<TRI_server_t*>(v8g->_server), id, name.c_str(), &defaults, &database, true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_ASSERT(database != nullptr);

  // copy users into context
  if (argv.Length() >= 3 && argv[2]->IsArray()) {
    v8::Handle<v8::Object> users = v8::Object::New();
    users->Set(v8::String::New("users"), argv[2]);

    v8::Context::GetCurrent()->Global()->Set(v8::String::New("UPGRADE_ARGS"), users);
  }
  else {
    v8::Context::GetCurrent()->Global()->Set(v8::String::New("UPGRADE_ARGS"), v8::Object::New());
  }

  // switch databases
  TRI_vocbase_t* orig = v8g->_vocbase;
  TRI_ASSERT(orig != nullptr);

  v8g->_vocbase = database;

  // initalise database
  v8g->_loader->executeGlobalScript(v8::Context::GetCurrent(), "server/bootstrap/local-database.js");

  // and switch back
  v8g->_vocbase = orig;

  // populate the authentication cache. otherwise no one can access the new database
  TRI_ReloadAuthInfo(database);

  // finally decrease the reference-counter
  TRI_ReleaseVocBase(database);

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop a database, case of a coordinator in a cluster
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> DropDatabaseCoordinator (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  // Arguments are already checked, there is exactly one argument
  string const name = TRI_ObjectToString(argv[0]);
  TRI_vocbase_t* vocbase = TRI_UseCoordinatorDatabaseServer(static_cast<TRI_server_t*>(v8g->_server), name.c_str());

  if (vocbase == nullptr) {
    // no such database
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_voc_tick_t const id = vocbase->_id;
  TRI_ReleaseVocBase(vocbase);


  ClusterInfo* ci = ClusterInfo::instance();
  string errorMsg;

  int res = ci->dropDatabaseCoordinator(name, errorMsg, 120.0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, errorMsg);
  }

  // now wait for heartbeat thread to drop the database object
  int tries = 0;

  while (++tries <= 6000) {
    TRI_vocbase_t* vocbase = TRI_UseByIdCoordinatorDatabaseServer((TRI_server_t*) v8g->_server, id);

    if (vocbase == 0) {
      // object has vanished
      break;
    }

    // sleep
    usleep(10000);
  }

  return scope.Close(v8::True());
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

static v8::Handle<v8::Value> JS_DropDatabase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "db._dropDatabase(<name>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (! TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(DropDatabaseCoordinator(argv));
  }

  string const name = TRI_ObjectToString(argv[0]);
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  int res = TRI_DropDatabaseServer(static_cast<TRI_server_t*>(v8g->_server), name.c_str(), true, true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_V8ReloadRouting(v8::Context::GetCurrent());

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief configure a new endpoint
///
/// @FUN{CONFIGURE_ENDPOINT}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ConfigureEndpoint (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1 || argv.Length() > 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "db._configureEndpoint(<endpoint>, <databases>)");
  }

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  TRI_server_t* server = static_cast<TRI_server_t*>(v8g->_server);
  ApplicationEndpointServer* s = static_cast<ApplicationEndpointServer*>(server->_applicationEndpointServer);

  if (s == 0) {
    // not implemented in console mode
    TRI_V8_EXCEPTION(scope, TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (! TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  const string endpoint = TRI_ObjectToString(argv[0]);

  // register dbNames
  vector<string> dbNames;

  if (argv.Length() > 1) {
    if (! argv[1]->IsArray()) {
      TRI_V8_EXCEPTION_PARAMETER(scope, "<databases> must be a list");
    }

    v8::Handle<v8::Array> list = v8::Handle<v8::Array>::Cast(argv[1]);

    const uint32_t n = list->Length();
    for (uint32_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> name = list->Get(i);

      if (name->IsString()) {
        const string dbName = TRI_ObjectToString(name);

        if (! TRI_IsAllowedNameVocBase(true, dbName.c_str())) {
          TRI_V8_EXCEPTION_PARAMETER(scope, "<databases> must be a list of database names");
        }

        dbNames.push_back(dbName);
      }
      else {
        TRI_V8_EXCEPTION_PARAMETER(scope, "<databases> must be a list of database names");
      }
    }
  }

  bool result = s->addEndpoint(endpoint, dbNames, true);

  if (! result) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_BAD_PARAMETER, "unable to bind to endpoint");
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a new endpoint
///
/// @FUN{REMOVE_ENDPOINT}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RemoveEndpoint (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1 || argv.Length() > 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "db._removeEndpoint(<endpoint>)");
  }

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  TRI_server_t* server = static_cast<TRI_server_t*>(v8g->_server);
  ApplicationEndpointServer* s = static_cast<ApplicationEndpointServer*>(server->_applicationEndpointServer);

  if (s == nullptr) {
    // not implemented in console mode
    TRI_V8_EXCEPTION(scope, TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (! TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  bool result = s->removeEndpoint(TRI_ObjectToString(argv[0]));

  if (! result) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_ENDPOINT_NOT_FOUND);
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list of all endpoints
///
/// @FUN{LIST_ENDPOINTS}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ListEndpoints (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "db._listEndpoints()");
  }

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  TRI_server_t* server = static_cast<TRI_server_t*>(v8g->_server);
  ApplicationEndpointServer* s = static_cast<ApplicationEndpointServer*>(server->_applicationEndpointServer);

  if (s == nullptr) {
    // not implemented in console mode
    TRI_V8_EXCEPTION(scope, TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (! TRI_IsSystemVocBase(vocbase)) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
  }

  auto const& endpoints = s->getEndpoints();

  v8::Handle<v8::Array> result = v8::Array::New();
  uint32_t j = 0;

  map<string, vector<string> >::const_iterator it;
  for (it = endpoints.begin(); it != endpoints.end(); ++it) {
    v8::Handle<v8::Array> dbNames = v8::Array::New();

    for (uint32_t i = 0; i < (*it).second.size(); ++i) {
      dbNames->Set(i, v8::String::New((*it).second.at(i).c_str()));
    }

    v8::Handle<v8::Object> item = v8::Object::New();
    item->Set(v8::String::New("endpoint"), v8::String::New((*it).first.c_str()));
    item->Set(v8::String::New("databases"), dbNames);

    result->Set(j++, item);
  }

  return scope.Close(result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parse vertex handle from a v8 value (string | object)
////////////////////////////////////////////////////////////////////////////////

int TRI_ParseVertex (CollectionNameResolver const* resolver,
                     TRI_voc_cid_t& cid,
                     std::unique_ptr<char[]>& key,
                     v8::Handle<v8::Value> const val) {

  v8::HandleScope scope;

  TRI_ASSERT(key.get() == nullptr);

  // reset everything
  string collectionName;
  TRI_voc_rid_t rid = 0;

  // try to extract the collection name, key, and revision from the object passed
  if (! ExtractDocumentHandle(val, collectionName, key, rid)) {
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

  v8::HandleScope scope;
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  TRI_vocbase_t* orig = v8g->_vocbase;
  v8g->_vocbase = vocbase;

  v8::Handle<v8::Value> result = startupLoader->executeGlobalScript(context, "server/upgrade-database.js");
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

  v8::HandleScope scope;
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  TRI_vocbase_t* orig = v8g->_vocbase;
  v8g->_vocbase = vocbase;

  v8::Handle<v8::Value> result = startupLoader->executeGlobalScript(context, "server/check-version.js");
  int code = (int) TRI_ObjectToInt64(result);

  v8g->_vocbase = orig;

  return code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads routing
////////////////////////////////////////////////////////////////////////////////

void TRI_V8ReloadRouting (v8::Handle<v8::Context> context) {
  v8::HandleScope scope;

  TRI_ExecuteJavaScriptString(context,
                              v8::String::New("require('internal').executeGlobalContextFunction('reloadRouting')"),
                              v8::String::New("reload routing"),
                              false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8VocBridge (triagens::arango::ApplicationV8* applicationV8,
                          v8::Handle<v8::Context> context,
                          triagens::aql::QueryRegistry* queryRegistry,
                          TRI_server_t* server,
                          TRI_vocbase_t* vocbase,
                          JSLoader* loader,
                          size_t threadNumber) {
  v8::HandleScope scope;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
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

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoDatabase"));

  ArangoNS = ft->InstanceTemplate();
  ArangoNS->SetInternalFieldCount(2);
  ArangoNS->SetNamedPropertyHandler(MapGetVocBase);

  // for any database function added here, be sure to add it to in function
  // JS_CompletionsVocbase, too for the auto-completion

  TRI_AddMethodVocbase(ArangoNS, "_version", JS_VersionServer);
  TRI_AddMethodVocbase(ArangoNS, "_id", JS_IdDatabase);
  TRI_AddMethodVocbase(ArangoNS, "_isSystem", JS_IsSystemDatabase);
  TRI_AddMethodVocbase(ArangoNS, "_name", JS_NameDatabase);
  TRI_AddMethodVocbase(ArangoNS, "_path", JS_PathDatabase);
  TRI_AddMethodVocbase(ArangoNS, "_createDatabase", JS_CreateDatabase);
  TRI_AddMethodVocbase(ArangoNS, "_dropDatabase", JS_DropDatabase);
  TRI_AddMethodVocbase(ArangoNS, "_listDatabases", JS_ListDatabases);
  TRI_AddMethodVocbase(ArangoNS, "_useDatabase", JS_UseDatabase);

  TRI_InitV8indexArangoDB(context, server, vocbase, loader, threadNumber, v8g, ArangoNS);

  TRI_InitV8collection(context, server, vocbase, loader, threadNumber, v8g, isolate, ArangoNS);

  v8g->VocbaseTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, ArangoNS);
  TRI_AddGlobalFunctionVocbase(context, "ArangoDatabase", ft->GetFunction());

  TRI_InitV8ShapedJson(context, server, vocbase, loader, threadNumber, v8g);

  TRI_InitV8cursor(context, server, vocbase, loader, threadNumber, v8g);

  // .............................................................................
  // generate global functions
  // .............................................................................

  // AQL functions. not intended to be used directly by end users
  TRI_AddGlobalFunctionVocbase(context, "AQL_EXECUTE", JS_ExecuteAql, true);
  TRI_AddGlobalFunctionVocbase(context, "AQL_EXECUTEJSON", JS_ExecuteAqlJson, true);
  TRI_AddGlobalFunctionVocbase(context, "AQL_EXPLAIN", JS_ExplainAql, true);
  TRI_AddGlobalFunctionVocbase(context, "AQL_PARSE", JS_ParseAql, true);
  TRI_AddGlobalFunctionVocbase(context, "AQL_WARNING", JS_WarningAql, true);

  TRI_InitV8replication(context, server, vocbase, loader, threadNumber, v8g);

  TRI_AddGlobalFunctionVocbase(context, "COMPARE_STRING", JS_compare_string);
  TRI_AddGlobalFunctionVocbase(context, "NORMALIZE_STRING", JS_normalize_string);
  TRI_AddGlobalFunctionVocbase(context, "TIMEZONES", JS_getIcuTimezones);
  TRI_AddGlobalFunctionVocbase(context, "LOCALES", JS_getIcuLocales);
  TRI_AddGlobalFunctionVocbase(context, "FORMAT_DATETIME", JS_formatDatetime);
  TRI_AddGlobalFunctionVocbase(context, "PARSE_DATETIME", JS_parseDatetime);

  TRI_AddGlobalFunctionVocbase(context, "CONFIGURE_ENDPOINT", JS_ConfigureEndpoint, true);
  TRI_AddGlobalFunctionVocbase(context, "REMOVE_ENDPOINT", JS_RemoveEndpoint, true);
  TRI_AddGlobalFunctionVocbase(context, "LIST_ENDPOINTS", JS_ListEndpoints, true);
  TRI_AddGlobalFunctionVocbase(context, "RELOAD_AUTH", JS_ReloadAuth, true);
  TRI_AddGlobalFunctionVocbase(context, "TRANSACTION", JS_Transaction, true);
  TRI_AddGlobalFunctionVocbase(context, "WAL_FLUSH", JS_FlushWal, true);
  TRI_AddGlobalFunctionVocbase(context, "WAL_PROPERTIES", JS_PropertiesWal, true);

  // .............................................................................
  // create global variables
  // .............................................................................

  v8::Handle<v8::Value> v = WrapVocBase(vocbase);
  if (v.IsEmpty()) {
    // TODO: raise an error here
    LOG_ERROR("out of memory when initialising VocBase");
  }
  else {
    TRI_AddGlobalVariableVocbase(context, "db", v);
  }

  // current thread number
  context->Global()->Set(TRI_V8_SYMBOL("THREAD_NUMBER"), v8::Number::New((double) threadNumber), v8::ReadOnly);
  
  // whether or not statistics are enabled
  context->Global()->Set(TRI_V8_SYMBOL("ENABLE_STATISTICS"), v8::Boolean::New(TRI_ENABLE_STATISTICS), v8::ReadOnly);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
