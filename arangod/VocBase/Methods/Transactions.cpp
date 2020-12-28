#include <v8.h>
#include "Transactions.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "Transaction/V8Context.h"
#include "Utils/CursorRepository.h"
#include "V8/v8-conv.h"
#include "V8/v8-helper.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-vocbaseprivate.h"

#include <velocypack/Slice.h>

namespace arangodb {

Result executeTransaction(v8::Isolate* isolate, basics::ReadWriteLock& lock,
                          std::atomic<bool>& canceled, VPackSlice slice,
                          std::string portType, VPackBuilder& builder) {
  // YOU NEED A TRY CATCH BLOCK like:
  //    TRI_V8_TRY_CATCH_BEGIN(isolate);
  //    TRI_V8_TRY_CATCH_END
  // outside of this function!

  READ_LOCKER(readLock, lock);
  Result rv;
  if (canceled) {
    rv.reset(TRI_ERROR_REQUEST_CANCELED, "handler canceled");
    return rv;
  }

  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;
  v8::Handle<v8::Value> in = TRI_VPackToV8(isolate, slice);

  v8::Handle<v8::Value> result;
  v8::TryCatch tryCatch(isolate);

  v8::Handle<v8::Object> request = v8::Object::New(isolate);
  v8::Handle<v8::Value> jsPortTypeKey =
      TRI_V8_ASCII_STRING(isolate, "portType");
  v8::Handle<v8::Value> jsPortTypeValue = TRI_V8_ASCII_STRING(isolate, portType.c_str());
  if (!request->Set(context, jsPortTypeKey, jsPortTypeValue).FromMaybe(false)) {
    rv.reset(TRI_ERROR_INTERNAL, "could not set portType");
    return rv;
  }
  {
    auto requestVal = v8::Handle<v8::Value>::Cast(request);
    auto responseVal = v8::Handle<v8::Value>::Cast(v8::Undefined(isolate));
    v8gHelper globalVars(isolate, tryCatch, requestVal, responseVal);
    readLock.unlock();  // unlock
    rv = executeTransactionJS(isolate, in, result, tryCatch);
    globalVars.cancel(canceled);
  }

  // do not allow the manipulation of the isolate while we are messing here
  READ_LOCKER(readLock2, lock);

  if (canceled) {  // if it was ok we would already have committed
    if (rv.ok()) {
      rv.reset(TRI_ERROR_REQUEST_CANCELED,
               "handler canceled - result already committed");
    } else {
      rv.reset(TRI_ERROR_REQUEST_CANCELED, "handler canceled");
    }
    return rv;
  }

  if (rv.fail()) {
    return rv;
  }

  if (tryCatch.HasCaught()) {
    // we have some javascript error that is not an arangoError
    std::string msg;
    if (!tryCatch.Message().IsEmpty()) {
      v8::String::Utf8Value m(isolate, tryCatch.Message()->Get());
      if (*m != nullptr) {
        msg = *m;
      }
    }
    rv.reset(TRI_ERROR_HTTP_SERVER_ERROR, msg);
  }

  if (rv.fail()) {
    return rv;
  }

  if (result.IsEmpty() || result->IsUndefined()) {
    // turn undefined to none
    builder.add(VPackSlice::noneSlice());
  } else {
    TRI_V8ToVPack(isolate, builder, result, false);
  }
  return rv;
}

Result executeTransactionJS(v8::Isolate* isolate, v8::Handle<v8::Value> const& arg,
                            v8::Handle<v8::Value>& result, v8::TryCatch& tryCatch) {
  auto context = TRI_IGETC;
  Result rv;
  auto& vocbase = GetContextVocBase(isolate);

  // treat the value as an object from now on
  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(arg);

  // "waitForSync"
  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL_STRING(WaitForSyncKey);

  // do extra sanity checking for user facing APIs, parsing
  // is performed in `transaction::Options::fromVelocyPack`
  if (TRI_HasProperty(context, isolate, object, "lockTimeout")) {
    auto lockTimeout = object->Get(context, TRI_V8_ASCII_STRING(isolate, "lockTimeout"));
    if (!lockTimeout.IsEmpty() &&
        !lockTimeout.FromMaybe(v8::Local<v8::Value>())->IsNumber()) {
      rv.reset(TRI_ERROR_BAD_PARAMETER,
               "<lockTimeout> must be a valid numeric value");
      return rv;
    }
  }
  if (TRI_HasProperty(context, isolate, object, WaitForSyncKey)) {
    auto waitForSync = object->Get(context, WaitForSyncKey);
    if (!waitForSync.IsEmpty() &&
        !waitForSync.FromMaybe(v8::Local<v8::Value>())->IsBoolean() &&
        !waitForSync.FromMaybe(v8::Local<v8::Value>())->IsBooleanObject()) {
      rv.reset(TRI_ERROR_BAD_PARAMETER, "<waitForSync> must be a boolean value");
      return rv;
    }
  }

  // extract the properties from the object
  transaction::Options trxOptions;
  {
    // parse all other options. `allowImplicitCollectionsForRead` will
    // be overwritten later if is contained in `object`
    VPackBuilder builder;
    // we must use "convertFunctionsToNull" here, because "action" is most
    // likely a JavaScript function
    TRI_V8ToVPack(isolate, builder, object, false,
                  /*convertFunctionsToNull*/ true);
    if (!builder.isClosed()) {
      builder.close();
    }
    if (!builder.slice().isObject()) {
      rv.reset(TRI_ERROR_BAD_PARAMETER);
      return rv;
    }
    trxOptions.fromVelocyPack(builder.slice());
  }
  if (trxOptions.lockTimeout < 0.0) {
    rv.reset(TRI_ERROR_BAD_PARAMETER, "<lockTimeout> needs to be positive");
    return rv;
  }

  // "collections"
  std::string collectionError;

  auto maybeCollections = object->Get(context, TRI_V8_ASCII_STRING(isolate, "collections"));
  if (!maybeCollections.IsEmpty() &&
      !maybeCollections.FromMaybe(v8::Local<v8::Value>())->IsObject()) {
    collectionError = "missing/invalid collections definition for transaction";
    rv.reset(TRI_ERROR_BAD_PARAMETER, collectionError);
    return rv;
  }

  // extract collections
  v8::Handle<v8::Object> collections = 
    v8::Handle<v8::Object>::Cast(maybeCollections.FromMaybe(v8::Local<v8::Value>()));

  if (collections.IsEmpty()) {
    collectionError = "empty collections definition for transaction";
    rv.reset(TRI_ERROR_BAD_PARAMETER, collectionError);
    return rv;
  }

  std::vector<std::string> readCollections;
  std::vector<std::string> writeCollections;
  std::vector<std::string> exclusiveCollections;

  if (TRI_HasProperty(context, isolate, collections, "allowImplicit")) {
    trxOptions.allowImplicitCollectionsForRead =
        TRI_ObjectToBoolean(isolate,
                            collections->Get(context, 
                                             TRI_V8_ASCII_STRING(isolate, "allowImplicit"))
                            .FromMaybe(v8::Local<v8::Value>())
                            );
  }

  auto getCollections =
    [&isolate, &context](v8::Handle<v8::Object> obj, std::vector<std::string>& collections,
                         char const* attributeName, std::string& collectionError) -> bool {
      if (TRI_HasProperty(context, isolate, obj, attributeName)) {
        auto localAttr = obj->Get(context, TRI_V8_ASCII_STRING(isolate, attributeName)).FromMaybe(v8::Local<v8::Value>());
        if (localAttr->IsArray()) {
          v8::Handle<v8::Array> names =
            v8::Handle<v8::Array>::Cast(localAttr);
          

          for (uint32_t i = 0; i < names->Length(); ++i) {
            v8::Handle<v8::Value> collection = names->Get(context, i).FromMaybe(v8::Local<v8::Value>());
            if (!collection->IsString()) {
              collectionError += std::string(" Collection name #") +
                std::to_string(i) + " in array '" +
                attributeName + std::string("' is not a string");
              return false;
            }

            collections.emplace_back(TRI_ObjectToString(isolate, collection));
          }
        } else if (localAttr->IsString()) {
          collections.emplace_back(TRI_ObjectToString(isolate, localAttr));
        } else {
          collectionError +=
            std::string(" There is no array in '") + attributeName + "'";
          return false;
        }
        // intentionally falls through
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

  if (!TRI_HasProperty(context, isolate, object, "action")) {
    rv.reset(TRI_ERROR_BAD_PARAMETER, actionError);
    return rv;
  }

  // function parameters
  v8::Handle<v8::Value> params;

  if (TRI_HasProperty(context, isolate, object, "params")) {
    params = v8::Handle<v8::Array>::Cast(object->Get(context,
                                                     TRI_V8_ASCII_STRING(isolate, "params")
                                                     ).FromMaybe(v8::Local<v8::Value>()));
  } else {
    params = v8::Undefined(isolate);
  }

  if (params.IsEmpty()) {
    rv.reset(TRI_ERROR_BAD_PARAMETER, "unable to decode function parameters");
    return rv;
  }

  bool embed = false;
  auto maybeEmbed = object->Get(context, TRI_V8_ASCII_STRING(isolate, "embed"));
  if (!maybeEmbed.IsEmpty()) {
    v8::Handle<v8::Value> v =
      v8::Handle<v8::Object>::Cast(maybeEmbed.FromMaybe(v8::Local<v8::Value>()));
    embed = TRI_ObjectToBoolean(isolate, v);
  }

  v8::Handle<v8::Object> current = isolate->GetCurrentContext()->Global();

  // callback function
  v8::Handle<v8::Function> action;
  auto maybeAction = object->Get(context, TRI_V8_ASCII_STRING(isolate, "action"));
  if (!maybeAction.IsEmpty()) {
    if (maybeAction.FromMaybe(v8::Local<v8::Value>())->IsFunction()) {
      action = v8::Handle<v8::Function>::Cast(maybeAction.FromMaybe(v8::Local<v8::Value>()));
      v8::Local<v8::Value> v8_fnname = action->GetName();
      std::string fnname = TRI_ObjectToString(isolate, v8_fnname);
      if (fnname.length() == 0) {
        action->SetName(TRI_V8_ASCII_STRING(isolate, "userTransactionFunction"));
      }
    } else if (maybeAction.FromMaybe(v8::Local<v8::Value>())->IsString()) {
      // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
      v8::Local<v8::Function> ctor =
        v8::Local<v8::Function>::Cast(current->Get(context, TRI_V8_ASCII_STRING(isolate, "Function")
                                                   ).FromMaybe(v8::Local<v8::Value>()));
      
      // Invoke Function constructor to create function with the given body and the
      // arguments
      std::string body =
        TRI_ObjectToString(isolate,
                           TRI_GetProperty(context, isolate, object, "action"));
      body = "return (" + body + ")(params);";
      v8::Handle<v8::Value> args[2] = {TRI_V8_ASCII_STRING(isolate, "params"),
                                       TRI_V8_STD_STRING(isolate, body)};
      v8::Local<v8::Object> function = ctor->NewInstance(TRI_IGETC, 2, args).FromMaybe(v8::Local<v8::Object>());

      action = v8::Local<v8::Function>::Cast(function);
      if (tryCatch.HasCaught()) {
        if (!tryCatch.Message().IsEmpty()) {
          v8::String::Utf8Value tryCatchMessage(isolate, tryCatch.Message()->Get());
          if (*tryCatchMessage != nullptr) {
            actionError += " - ";
            actionError += *tryCatchMessage;
          }
        }
        auto stacktraceV8 = tryCatch.StackTrace(TRI_IGETC).FromMaybe(v8::Local<v8::Value>());
        v8::String::Utf8Value tryCatchStackTrace(isolate, stacktraceV8);
        if (*tryCatchStackTrace != nullptr) {
          actionError += " - ";
          actionError += *tryCatchStackTrace;
        }
        rv.reset(TRI_ERROR_BAD_PARAMETER, actionError);
        tryCatch.Reset();  // reset as we have transferred the error message into
        // the Result
        return rv;
      }
      action->SetName(TRI_V8_ASCII_STRING(isolate, "userTransactionSource"));
    }
  }
  if (action.IsEmpty()) {
    rv.reset(TRI_ERROR_BAD_PARAMETER, actionError);
    return rv;
  }

  auto ctx = std::make_shared<transaction::V8Context>(vocbase, embed);

  // start actual transaction
  auto trx = std::make_unique<transaction::Methods>(ctx, readCollections, writeCollections,
                                                    exclusiveCollections, trxOptions);
  trx->addHint(transaction::Hints::Hint::GLOBAL_MANAGED);
  if (ServerState::instance()->isCoordinator()) {
    // No one knows our Transaction ID yet, so we an run FAST_LOCK_ROUND and potentialy reroll it.
    trx->addHint(transaction::Hints::Hint::ALLOW_FAST_LOCK_ROUND_CLUSTER);
  }

  rv = trx->begin();

  if (rv.fail()) {
    return rv;
  }

  try {
    v8::Handle<v8::Value> arguments = params;

    result = action->Call(TRI_IGETC, current, 1, &arguments).FromMaybe(v8::Local<v8::Value>());

    if (tryCatch.HasCaught()) {
      trx->abort();

      std::tuple<bool, bool, Result> rvTuple =
          extractArangoError(isolate, tryCatch, TRI_ERROR_TRANSACTION_INTERNAL);

      if (std::get<1>(rvTuple)) {
        rv = std::get<2>(rvTuple);
      } else {
        // some general error we don't know about
        rv = Result(TRI_ERROR_TRANSACTION_INTERNAL,
                    "an unknown error occured while executing the transaction");
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

  rv = trx->finish(rv);

  // if we do not remove unused V8Cursors, V8Context might not reset global
  // state
  vocbase.cursorRepository()->garbageCollect(/*force*/ false);

  return rv;
}

}  // namespace arangodb
