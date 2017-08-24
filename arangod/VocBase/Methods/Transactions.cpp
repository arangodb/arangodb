#include "Transactions.h"
#include <v8.h>

#include "Transaction/Options.h"
#include "Transaction/UserTransaction.h"
#include "Transaction/V8Context.h"
#include "V8/v8-conv.h"
#include "V8/v8-vpack.h"
#include "V8/v8-helper.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "Logger/Logger.h"
#include <velocypack/Slice.h>
#include "Basics/WriteLocker.h"
#include "Basics/ReadLocker.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Transaction/IgnoreNoAccessMethods.h"
#endif

namespace arangodb {


Result executeTransaction(
    v8::Isolate* isolate,
    basics::ReadWriteLock& lock,
    std::atomic<bool>& canceled,
    VPackSlice slice,
    std::string portType,
    VPackBuilder& builder){

  // YOU NEED A TRY CATCH BLOCK like:
  //    TRI_V8_TRY_CATCH_BEGIN(isolate);
  //    TRI_V8_TRY_CATCH_END
  // outside of this function!

  READ_LOCKER(readLock, lock);
  Result rv;
  if(canceled){
    rv.reset(TRI_ERROR_REQUEST_CANCELED,"handler canceled");
    return rv;
  }

  v8::HandleScope scope(isolate);
  v8::Handle<v8::Value> in = TRI_VPackToV8(isolate, slice);

  v8::Handle<v8::Value> result;
  v8::TryCatch tryCatch;

  v8::Handle<v8::Object> request = v8::Object::New(isolate);
  v8::Handle<v8::Value> jsPortTypeKey= TRI_V8_ASCII_STRING("portType");
  v8::Handle<v8::Value> jsPortTypeValue = TRI_V8_ASCII_STRING(portType.c_str());
  if (!request->Set(jsPortTypeKey, jsPortTypeValue)){
    rv.reset(TRI_ERROR_INTERNAL, "could not set portType");
    return rv;
  }
  {
    auto requestVal = v8::Handle<v8::Value>::Cast(request);
    auto responseVal = v8::Handle<v8::Value>::Cast(v8::Undefined(isolate));
    v8gHelper globalVars(isolate, tryCatch, requestVal, responseVal);
    readLock.unlock(); //unlock
    rv = executeTransactionJS(isolate, in, result, tryCatch);
    globalVars.cancel(canceled);
  }

  //do not allow the manipulation of the isolate while we are messing here
  READ_LOCKER(readLock2, lock);

  if (canceled) { //if it was ok we would already have committed
    if(rv.ok()) {
      rv.reset(TRI_ERROR_REQUEST_CANCELED,"handler canceled - result already committed");
    } else {
      rv.reset(TRI_ERROR_REQUEST_CANCELED,"handler canceled");
    }
    return rv;
  }

  if (rv.fail()) { 
    return rv; 
  }

  if (tryCatch.HasCaught()) {
    //we have some javascript error that is not an arangoError
    std::string msg = *v8::String::Utf8Value(tryCatch.Message()->Get());
    rv.reset(TRI_ERROR_HTTP_SERVER_ERROR, msg);
  }

  if (rv.fail()) { 
    return rv; 
  }

  if(result.IsEmpty() || result->IsUndefined()) {
    // turn undefined to none
    builder.add(VPackSlice::noneSlice());
  } else {
    TRI_V8ToVPack(isolate, builder, result, false);
  }
  return rv;
}

Result executeTransactionJS(
    v8::Isolate* isolate,
    v8::Handle<v8::Value> const& arg,
    v8::Handle<v8::Value>& result,
    v8::TryCatch& tryCatch
    )
{
  Result rv;
  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);
  if (vocbase == nullptr) {
    rv.reset(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return rv;
  }

  // treat the value as an object from now on
  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(arg);

  // "waitForSync"
  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL_STRING(WaitForSyncKey);
  
  // do extra sanity checking for user facing APIs, parsing
  // is performed in `transaction::Options::fromVelocyPack`
  if (object->Has(TRI_V8_ASCII_STRING("lockTimeout")) &&
      !object->Get(TRI_V8_ASCII_STRING("lockTimeout"))->IsNumber()) {
    rv.reset(TRI_ERROR_BAD_PARAMETER,
             "<lockTimeout> must be a valid numeric value");
    return rv;
  }
  if (object->Has(WaitForSyncKey) &&
      !object->Get(WaitForSyncKey)->IsBoolean() &&
      !object->Get(WaitForSyncKey)->IsBooleanObject()) {
    rv.reset(TRI_ERROR_BAD_PARAMETER,
             "<waitForSync> must be a boolean value");
    return rv;
  }
  
  // extract the properties from the object
  transaction::Options trxOptions;
  {
    // parse all other options. `allowImplicitCollections` will
    // be overwritten later if is contained in `object`
    VPackBuilder builder;
    TRI_V8ToVPack(isolate, builder, object, false);
    if (!builder.isClosed()) builder.close();
    if (!builder.slice().isObject()) {
      rv.reset(TRI_ERROR_BAD_PARAMETER);
      return rv;
    }
    trxOptions.fromVelocyPack(builder.slice());
  }
  if (trxOptions.lockTimeout < 0.0) {
    rv.reset(TRI_ERROR_BAD_PARAMETER, "<lockTiemout> needs to be positive");
    return rv;
  }

  // "collections"
  std::string collectionError;

  if (!object->Has(TRI_V8_ASCII_STRING("collections")) ||
      !object->Get(TRI_V8_ASCII_STRING("collections"))->IsObject()) {
    collectionError = "missing/invalid collections definition for transaction";
    rv.reset(TRI_ERROR_BAD_PARAMETER, collectionError);
    return rv;
  }

  // extract collections
  v8::Handle<v8::Object> collections = v8::Handle<v8::Object>::Cast(
      object->Get(TRI_V8_ASCII_STRING("collections")));

  if (collections.IsEmpty()) {
    collectionError =
      "empty collections definition for transaction";
    rv.reset(TRI_ERROR_BAD_PARAMETER, collectionError);
    return rv;
  }

  std::vector<std::string> readCollections;
  std::vector<std::string> writeCollections;
  std::vector<std::string> exclusiveCollections;

  if (collections->Has(TRI_V8_ASCII_STRING("allowImplicit"))) {
    trxOptions.allowImplicitCollections = TRI_ObjectToBoolean(
        collections->Get(TRI_V8_ASCII_STRING("allowImplicit")));
  }
  
  auto getCollections = [&isolate](v8::Handle<v8::Object> obj,
                                   std::vector<std::string>& collections,
                                   char const* attributeName,
                                   std::string &collectionError) -> bool {
    if (obj->Has(TRI_V8_ASCII_STRING(attributeName))) {
      if (obj->Get(TRI_V8_ASCII_STRING(attributeName))->IsArray()) {
        v8::Handle<v8::Array> names = v8::Handle<v8::Array>::Cast(
            obj->Get(TRI_V8_ASCII_STRING(attributeName)));

        for (uint32_t i = 0; i < names->Length(); ++i) {
          v8::Handle<v8::Value> collection = names->Get(i);
          if (!collection->IsString()) {
            collectionError += std::string(" Collection name #") +
              std::to_string(i) + " in array '"+ attributeName +
              std::string("' is not a string");
            return false;
          }

          collections.emplace_back(TRI_ObjectToString(collection));
        }
      } else if (obj->Get(TRI_V8_ASCII_STRING(attributeName))->IsString()) {
        collections.emplace_back(
          TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING(attributeName))));
      } else {
        collectionError += std::string(" There is no array in '") + attributeName + "'";
        return false;
      }
      // fallthrough intentional
    }
    return true;
  };

  collectionError = "invalid collection definition for transaction: ";
  // collections.read
  bool isValid =
    (getCollections(collections, readCollections, "read", collectionError) &&
     getCollections(collections, writeCollections, "write", collectionError) &&
     getCollections(collections, exclusiveCollections, "exclusive", collectionError));

  if (!isValid) {
    rv.reset(TRI_ERROR_BAD_PARAMETER, collectionError);
    return rv;
  }

  // extract the "action" property
  static std::string const actionErrorPrototype =
      "missing/invalid action definition for transaction";
  std::string actionError = actionErrorPrototype;

  if (!object->Has(TRI_V8_ASCII_STRING("action"))) {
    rv.reset(TRI_ERROR_BAD_PARAMETER, actionError);
    return rv;
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
    rv.reset(TRI_ERROR_BAD_PARAMETER, "unable to decode function parameters");
    return rv;
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
    v8::Local<v8::Value> v8_fnname = action->GetName();
    std::string fnname = TRI_ObjectToString(v8_fnname);
    if (fnname.length() == 0) {
      action->SetName(TRI_V8_ASCII_STRING("userTransactionFunction"));
    }
  } else if (object->Get(TRI_V8_ASCII_STRING("action"))->IsString()) {
    // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
    v8::Local<v8::Function> ctor = v8::Local<v8::Function>::Cast(
        current->Get(TRI_V8_ASCII_STRING("Function")));

    // Invoke Function constructor to create function with the given body and no
    // arguments
    std::string body = TRI_ObjectToString( object->Get(TRI_V8_ASCII_STRING("action"))->ToString());
    body = "return (" + body + ")(params);";
    v8::Handle<v8::Value> args[2] = {TRI_V8_ASCII_STRING("params"), TRI_V8_STD_STRING(body)};
    v8::Local<v8::Object> function = ctor->NewInstance(2, args);

    action = v8::Local<v8::Function>::Cast(function);
    if (tryCatch.HasCaught()) {
      actionError += " - ";
      actionError += *v8::String::Utf8Value(tryCatch.Message()->Get());
      actionError += " - ";
      actionError += *v8::String::Utf8Value(tryCatch.StackTrace());
      rv.reset(TRI_ERROR_BAD_PARAMETER, actionError);
      tryCatch.Reset(); //reset as we have transferd the error message into the Result
      return rv;
    }
    action->SetName(TRI_V8_ASCII_STRING("userTransactionSource"));
  } else {
    rv.reset(TRI_ERROR_BAD_PARAMETER, actionError);
    return rv;
  }

  if (action.IsEmpty()) {
    rv.reset(TRI_ERROR_BAD_PARAMETER, actionError);
    return rv;
  }

  auto transactionContext =
      std::make_shared<transaction::V8Context>(vocbase, embed);
  // start actual transaction
  std::unique_ptr<transaction::Methods> trx(new transaction::UserTransaction(transactionContext, readCollections,
                                             writeCollections, exclusiveCollections,
                                             trxOptions));
    
  rv = trx->begin();
  if (rv.fail()) {
    return rv;
  }
  
  try {
    v8::Handle<v8::Value> arguments = params;
    result = action->Call(current, 1, &arguments);
    if (tryCatch.HasCaught()) {
      trx->abort();
      std::tuple<bool,bool,Result> rvTuple = extractArangoError(isolate, tryCatch);
      if (std::get<1>(rvTuple)){
        rv = std::get<2>(rvTuple);
      }
    }
  } catch (arangodb::basics::Exception const& ex) {
    rv.reset(ex.code(), ex.what());
  } catch (std::bad_alloc const&) {
    rv.reset(TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    rv.reset(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    rv.reset(TRI_ERROR_INTERNAL, "caught unknown exception during transaction");
  }

  if (rv.fail()) {
    return rv;
  }

  rv = trx->commit();
  return rv;
}

} // arangodb
