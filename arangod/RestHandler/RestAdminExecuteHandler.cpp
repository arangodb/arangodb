////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Actions/ActionFeature.h"
#include "Actions/actions.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "V8/JavaScriptSecurityContext.h"
#include "V8/v8-globals.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8Executor.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-actions.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>

using namespace arangodb;
using namespace arangodb::rest;

RestAdminExecuteHandler::RestAdminExecuteHandler(ArangodServer& server,
                                                 GeneralRequest* request,
                                                 GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestAdminExecuteHandler::execute() {
  if (!server().isEnabled<V8DealerFeature>()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                  TRI_ERROR_NOT_IMPLEMENTED,
                  "JavaScript operations are disabled");
    return RestStatus::DONE;
  }

  TRI_ASSERT(server().getFeature<V8DealerFeature>().allowAdminExecute());

  std::string_view payload;

  // interpret request body
  bool handled = false;
  if (_request->contentType() == rest::ContentType::VPACK ||
      _request->contentType() == rest::ContentType::JSON) {
    // if content-type is set to JSON or VPACK, we expect
    // a proper string value
    try {
      payload = _request->payload(false).stringView();
      handled = true;
    } catch (...) {
      if (_request->contentType() == rest::ContentType::VPACK) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                      "expecting string value with command to execute");
        return RestStatus::DONE;
      }
      // downwards-compatibility hack: we previously always
      // interpreted the body content as a string, regardless of
      // which Content-Type was set in the request. In order to
      // make this still work, fall back to the old-behavior here
      // if we got a parse error.
      // note that we never supported receiving VPack before, so
      // the hack only needs to be here for requests with a JSON
      // Content-Type
    }
  }

  if (!handled) {
    // if content type is plain text or unset, interpret verbatim
    TRI_ASSERT(payload.empty());
    payload = _request->rawPayload();
  }

  if (payload.empty()) {
    // nothing to execute. return an empty response
    generateOk(rest::ResponseCode::OK, VPackSlice::noneSlice());
    return RestStatus::DONE;
  }

  try {
    LOG_TOPIC("c838e", DEBUG, Logger::SECURITY)
        << "about to execute: '" << payload << "'";

    // get a V8 context
    bool const allowUseDatabase =
        server().getFeature<ActionFeature>().allowUseDatabase();
    JavaScriptSecurityContext securityContext =
        JavaScriptSecurityContext::createRestAdminScriptActionContext(
            allowUseDatabase);
    V8ExecutorGuard guard(&_vocbase, securityContext);

    guard.runInContext([&](v8::Isolate* isolate) -> Result {
      v8::HandleScope scope(isolate);

      v8::Handle<v8::Context> context = isolate->GetCurrentContext();
      v8::Handle<v8::Object> current = context->Global();
      v8::TryCatch tryCatch(isolate);

      // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
      v8::Local<v8::Function> ctor = v8::Local<v8::Function>::Cast(
          current->Get(context, TRI_V8_ASCII_STRING(isolate, "Function"))
              .FromMaybe(v8::Handle<v8::Value>()));
      v8::Handle<v8::Value> args[1] = {
          TRI_V8_PAIR_STRING(isolate, payload.data(), payload.size())};
      v8::Local<v8::Object> function = ctor->NewInstance(context, 1, args)
                                           .FromMaybe(v8::Local<v8::Object>());
      v8::Handle<v8::Function> action = v8::Local<v8::Function>::Cast(function);

      v8::Handle<v8::Value> rv;

      if (!action.IsEmpty()) {
        action->SetName(TRI_V8_ASCII_STRING(isolate, "source"));

        TRI_GET_GLOBALS();

        TRI_fake_action_t adminExecuteAction("_admin/execute", 2);

        v8::Handle<v8::Object> req = TRI_RequestCppToV8(
            isolate, v8g, _request.get(), &adminExecuteAction);
        v8g->_currentRequest.Reset(isolate, req);

        v8::Handle<v8::Object> res = v8::Object::New(isolate);
        v8g->_currentResponse.Reset(isolate, res);

        auto guard = scopeGuard([&v8g]() noexcept {
          v8g->_currentRequest.Reset();
          v8g->_currentResponse.Reset();
        });

        v8::Handle<v8::Value> args[] = {v8::Null(isolate)};
        rv = action->Call(TRI_IGETC, current, 0, args)
                 .FromMaybe(v8::Local<v8::Value>());
      }

      if (tryCatch.HasCaught()) {
        // got an error
        std::string errorMessage;

        auto stacktraceV8 =
            tryCatch.StackTrace(TRI_IGETC).FromMaybe(v8::Local<v8::Value>());
        v8::String::Utf8Value tryCatchStackTrace(isolate, stacktraceV8);
        if (*tryCatchStackTrace != nullptr) {
          errorMessage = *tryCatchStackTrace;
        } else if (!tryCatch.Message().IsEmpty()) {
          v8::String::Utf8Value tryCatchMessage(isolate,
                                                tryCatch.Message()->Get());
          if (*tryCatchMessage != nullptr) {
            errorMessage = *tryCatchMessage;
          }
        }

        _response->setResponseCode(rest::ResponseCode::SERVER_ERROR);
        switch (_response->transportType()) {
          case Endpoint::TransportType::HTTP: {
            _response->setContentType(rest::ContentType::TEXT);
            _response->addRawPayload(errorMessage);
            break;
          }
          case Endpoint::TransportType::VST: {
            VPackBuffer<uint8_t> buffer;
            VPackBuilder builder(buffer);
            builder.add(VPackValuePair(
                reinterpret_cast<uint8_t const*>(errorMessage.data()),
                errorMessage.size()));
            _response->setContentType(rest::ContentType::VPACK);
            _response->setPayload(std::move(buffer));
            break;
          }
        }
      } else {
        // all good!
        bool returnAsJSON = _request->parsedValue("returnAsJSON", false);
        if (returnAsJSON) {
          // if the result is one of the following type, we return it as is
          returnAsJSON &=
              (rv->IsString() || rv->IsStringObject() || rv->IsNumber() ||
               rv->IsNumberObject() || rv->IsBoolean());
        }

        VPackBuilder result;
        bool handled = false;

        if (returnAsJSON) {
          result.openObject(true);
          result.add(StaticStrings::Error, VPackValue(false));
          result.add(StaticStrings::Code,
                     VPackValue(static_cast<int>(rest::ResponseCode::OK)));
          if (rv->IsObject()) {
            TRI_V8ToVPack(isolate, result, rv, false);
            handled = true;
          }
          result.close();
        }

        if (!handled) {
          result.clear();
          TRI_V8ToVPack(isolate, result, rv, false);
        }

        generateResult(rest::ResponseCode::OK, result.slice());
      }
      return {};
    });

  } catch (basics::Exception const& ex) {
    generateError(GeneralResponse::responseCode(ex.code()), ex.code(),
                  ex.what());
  } catch (std::exception const& ex) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  ex.what());
  }

  return RestStatus::DONE;
}
