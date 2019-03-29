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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminExecuteHandler.h"

#include "Actions/ActionFeature.h"
#include "Actions/actions.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "V8/v8-globals.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-actions.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestAdminExecuteHandler::RestAdminExecuteHandler(GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestStatus RestAdminExecuteHandler::execute() {
  if (!V8DealerFeature::DEALER) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "JavaScript operations are not available");
    return RestStatus::DONE;
  }

  TRI_ASSERT(V8DealerFeature::DEALER->allowAdminExecute());
      
  arangodb::velocypack::StringRef bodyStr = _request->rawPayload();
  char const* body = bodyStr.data();
  size_t bodySize = bodyStr.size();
  
  if (bodySize == 0) {
    // nothing to execute. return an empty response
    VPackBuilder result;
    result.openObject(true);
    result.add(StaticStrings::Error, VPackValue(false));
    result.add(StaticStrings::Code, VPackValue(static_cast<int>(rest::ResponseCode::OK)));
    result.close();

    generateResult(rest::ResponseCode::OK, result.slice());
    return RestStatus::DONE;
  }

  try {
    LOG_TOPIC("c838e", WARN, Logger::FIXME) << "about to execute: '" << Logger::CHARS(body, bodySize) << "'";
    
    ssize_t forceContext = -1;
    bool found;
    std::string const& c = _request->header("x-arango-v8-context", found);

    if (found && !c.empty()) {
      forceContext = basics::StringUtils::int32(c);
    }
  
    // get a V8 context
    bool const allowUseDatabase = ActionFeature::ACTION->allowUseDatabase();
    V8Context* context = V8DealerFeature::DEALER->enterContext(&_vocbase,  allowUseDatabase, forceContext);

    // note: the context might be nullptr in case of shut-down
    if (context == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_RESOURCE_LIMIT,
                                    "unable to acquire V8 context in time");
    }
    
    TRI_DEFER(V8DealerFeature::DEALER->exitContext(context));
    
    { 
      v8::Isolate* isolate = context->_isolate; 
      v8::HandleScope scope(isolate);

      v8::Handle<v8::Object> current = isolate->GetCurrentContext()->Global();
      v8::TryCatch tryCatch(isolate);

      // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
      v8::Local<v8::Function> ctor = v8::Local<v8::Function>::Cast(
          current->Get(TRI_V8_ASCII_STRING(isolate, "Function")));
      v8::Handle<v8::Value> args[1] = {TRI_V8_PAIR_STRING(isolate, body, bodySize)};
      v8::Local<v8::Object> function = ctor->NewInstance(TRI_IGETC, 1, args).FromMaybe(v8::Local<v8::Object>());
      v8::Handle<v8::Function> action = v8::Local<v8::Function>::Cast(function);
        
      v8::Handle<v8::Value> rv;
      
      if (!action.IsEmpty()) {
        action->SetName(TRI_V8_ASCII_STRING(isolate, "source"));

        TRI_GET_GLOBALS();

        TRI_fake_action_t adminExecuteAction("_admin/execute", 2);

        v8g->_currentRequest = TRI_RequestCppToV8(isolate, v8g, _request.get(), &adminExecuteAction); 
        v8g->_currentResponse = v8::Object::New(isolate);
       
        auto guard = scopeGuard([&v8g, &isolate]() {
          v8g->_currentRequest = v8::Undefined(isolate);
          v8g->_currentResponse = v8::Undefined(isolate);
        });
           
        v8::Handle<v8::Value> args[] = {v8::Null(isolate)};
        rv = action->Call(current, 0, args);
      }
    
      if (tryCatch.HasCaught()) {
        // got an error
        std::string errorMessage;

        auto stacktraceV8 = tryCatch.StackTrace(TRI_IGETC).FromMaybe(v8::Local<v8::Value>());
        v8::String::Utf8Value tryCatchStackTrace(isolate, stacktraceV8);
        if (*tryCatchStackTrace != nullptr) {
          errorMessage = *tryCatchStackTrace;
        } else if (!tryCatch.Message().IsEmpty()) {
          v8::String::Utf8Value tryCatchMessage(isolate, tryCatch.Message()->Get());
          if (*tryCatchMessage != nullptr) {
            errorMessage = *tryCatchMessage;
          }
        }
    
        _response->setResponseCode(rest::ResponseCode::SERVER_ERROR);
        switch (_response->transportType()) {
          case Endpoint::TransportType::HTTP: {
            HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
            if (httpResponse == nullptr) {
              THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to cast response object");
            }
            _response->setContentType(rest::ContentType::TEXT);
            httpResponse->body().appendText(errorMessage.data(), errorMessage.size());
            break;
          } 
          case Endpoint::TransportType::VST: {
            VPackBuffer<uint8_t> buffer;
            VPackBuilder builder(buffer);
            builder.add(VPackValuePair(reinterpret_cast<uint8_t const*>(errorMessage.data()), errorMessage.size()));
            _response->setContentType(rest::ContentType::VPACK);
            _response->setPayload(std::move(buffer), true);
            break;
          }
        }
      } else {
        // all good!
        bool returnAsJSON = _request->parsedValue("returnAsJSON", false);
        if (returnAsJSON) {
          // if the result is one of the following type, we return it as is
          returnAsJSON &= (rv->IsString() || rv->IsStringObject() || 
                           rv->IsNumber() || rv->IsNumberObject() || 
                           rv->IsBoolean());
        }
          
        VPackBuilder result;
        bool handled = false;
        int res = TRI_ERROR_FAILED;

        if (returnAsJSON) {
          result.openObject(true);
          result.add(StaticStrings::Error, VPackValue(false));
          result.add(StaticStrings::Code, VPackValue(static_cast<int>(rest::ResponseCode::OK)));
          if (rv->IsObject()) {
            res = TRI_V8ToVPack(isolate, result, rv, false); 
            handled = true;
          }
          result.close();
        }

        if (!handled) {
          result.clear();
          
          VPackBuilder temp;
          res = TRI_V8ToVPack(isolate, temp, rv, false); 
          result.add(temp.slice());
        } 
          
        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(res);
        }
    
        generateResult(rest::ResponseCode::OK, result.slice());
      }
    }

  } catch (basics::Exception const& ex) {
    generateError(GeneralResponse::responseCode(ex.code()), ex.code(), ex.what());
  } catch (std::exception const& ex) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL, ex.what());
  }

  return RestStatus::DONE;
}
