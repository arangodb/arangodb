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

#include "v8-actions.h"
#include "Actions/ActionFeature.h"
#include "Actions/actions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServer.h"
#include "Logger/Logger.h"
#include "Rest/GeneralRequest.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

static TRI_action_result_t ExecuteActionVocbase(
    TRI_vocbase_t*, v8::Isolate*, TRI_action_t const*,
    v8::Handle<v8::Function> callback, GeneralRequest*, GeneralResponse*);

////////////////////////////////////////////////////////////////////////////////
/// @brief action description for V8
////////////////////////////////////////////////////////////////////////////////

class v8_action_t final : public TRI_action_t {
 public:
  v8_action_t() : TRI_action_t(), _callbacks(), _callbacksLock() {}

  void visit(void* data) override {
    v8::Isolate* isolate = static_cast<v8::Isolate*>(data);
    
    WRITE_LOCKER(writeLocker, _callbacksLock);

    auto it = _callbacks.find(isolate);

    if (it != _callbacks.end()) {
      (*it).second.Reset(); // dispose persistent
      _callbacks.erase(it); // remove entry from map
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates callback for a context
  //////////////////////////////////////////////////////////////////////////////

  void createCallback(v8::Isolate* isolate, v8::Handle<v8::Function> callback) {
    WRITE_LOCKER(writeLocker, _callbacksLock);

    auto it = _callbacks.find(isolate);

    if (it == _callbacks.end()) {
      _callbacks[isolate].Reset(isolate, callback);
    } else {
      LOG_TOPIC(ERR, Logger::V8) << "cannot recreate callback for '" << _url << "'";
    }
  }

  TRI_action_result_t execute(TRI_vocbase_t* vocbase,
                              GeneralRequest* request, GeneralResponse* response,
                              Mutex* dataLock, void** data) override {
    TRI_action_result_t result;

    // allow use datase execution in rest calls
    bool allowUseDatabaseInRestActions =
        ActionFeature::ACTION->allowUseDatabase();

    if (_allowUseDatabase) {
      allowUseDatabaseInRestActions = true;
    }

    // this is for TESTING / DEBUGGING only - do not document this feature
    ssize_t forceContext = -1;
    bool found;

    std::string const& c = request->header("x-arango-v8-context", found);

    if (found && !c.empty()) {
      forceContext = StringUtils::int32(c);
    }

    // get a V8 context
    V8Context* context = V8DealerFeature::DEALER->enterContext(vocbase,
                            allowUseDatabaseInRestActions, forceContext);

    // note: the context might be nullptr in case of shut-down
    if (context == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_RESOURCE_LIMIT, "unable to acquire V8 context in time");
    }

    TRI_DEFER(V8DealerFeature::DEALER->exitContext(context));

    // locate the callback
    READ_LOCKER(readLocker, _callbacksLock);

    {
      auto it = _callbacks.find(context->_isolate);

      if (it == _callbacks.end()) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "no callback function for JavaScript action '" << _url
                  << "'";

        result.isValid = true;
        response->setResponseCode(rest::ResponseCode::NOT_FOUND);

        return result;
      }

      // and execute it
      {
        MUTEX_LOCKER(mutexLocker, *dataLock);

        if (*data != nullptr) {
          result.canceled = true;
          return result;
        }

        *data = (void*)context->_isolate;
      }
      v8::HandleScope scope(context->_isolate);
      auto localFunction =
          v8::Local<v8::Function>::New(context->_isolate, it->second);

      // we can release the lock here already as no other threads will
      // work in our isolate at this time
      readLocker.unlock();

      try {
        result = ExecuteActionVocbase(vocbase, context->_isolate, this,
                                      localFunction, request, response);
      } catch (...) {
        result.isValid = false;
      }

      {
        MUTEX_LOCKER(mutexLocker, *dataLock);
        *data = nullptr;
      }
    }

    return result;
  }

  bool cancel(Mutex* dataLock, void** data) override {
    {
      MUTEX_LOCKER(mutexLocker, *dataLock);

      // either we have not yet reached the execute above or we are already done
      if (*data == nullptr) {
        *data = (void*)1;  // mark as canceled
      }

      // data is set, cancel the execution
      else {
        if (!v8::V8::IsExecutionTerminating((v8::Isolate*)*data)) {
          v8::V8::TerminateExecution((v8::Isolate*)*data);
        }
      }
    }

    return true;
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief callback dictionary
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<v8::Isolate*, v8::Persistent<v8::Function>> _callbacks;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lock for the callback dictionary
  //////////////////////////////////////////////////////////////////////////////

  ReadWriteLock _callbacksLock;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the action options
////////////////////////////////////////////////////////////////////////////////

static void ParseActionOptions(v8::Isolate* isolate, TRI_v8_global_t* v8g,
                               TRI_action_t* action,
                               v8::Handle<v8::Object> options) {
  TRI_GET_GLOBAL_STRING(PrefixKey);
  // check the "prefix" field
  if (options->Has(PrefixKey)) {
    action->_isPrefix = TRI_ObjectToBoolean(options->Get(PrefixKey));
  } else {
    action->_isPrefix = false;
  }

  // check the "allowUseDatabase" field
  TRI_GET_GLOBAL_STRING(AllowUseDatabaseKey);
  if (options->Has(AllowUseDatabaseKey)) {
    action->_allowUseDatabase =
        TRI_ObjectToBoolean(options->Get(AllowUseDatabaseKey));
  } else {
    action->_allowUseDatabase = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add cookie
////////////////////////////////////////////////////////////////////////////////

static void AddCookie(v8::Isolate* isolate, TRI_v8_global_t const* v8g,
                      HttpResponse* response, v8::Handle<v8::Object> data) {
  std::string name;
  std::string value;
  int lifeTimeSeconds = 0;
  std::string path = "/";
  std::string domain = "";
  bool secure = false;
  bool httpOnly = false;

  TRI_GET_GLOBAL_STRING(NameKey);
  if (data->Has(NameKey)) {
    v8::Handle<v8::Value> v = data->Get(NameKey);
    name = TRI_ObjectToString(v);
  } else {
    // something is wrong here
    return;
  }
  TRI_GET_GLOBAL_STRING(ValueKey);
  if (data->Has(ValueKey)) {
    v8::Handle<v8::Value> v = data->Get(ValueKey);
    value = TRI_ObjectToString(v);
  } else {
    // something is wrong here
    return;
  }
  TRI_GET_GLOBAL_STRING(LifeTimeKey);
  if (data->Has(LifeTimeKey)) {
    v8::Handle<v8::Value> v = data->Get(LifeTimeKey);
    lifeTimeSeconds = (int)TRI_ObjectToInt64(v);
  }
  TRI_GET_GLOBAL_STRING(PathKey);
  if (data->Has(PathKey) && !data->Get(PathKey)->IsUndefined()) {
    v8::Handle<v8::Value> v = data->Get(PathKey);
    path = TRI_ObjectToString(v);
  }
  TRI_GET_GLOBAL_STRING(DomainKey);
  if (data->Has(DomainKey) && !data->Get(DomainKey)->IsUndefined()) {
    v8::Handle<v8::Value> v = data->Get(DomainKey);
    domain = TRI_ObjectToString(v);
  }
  TRI_GET_GLOBAL_STRING(SecureKey);
  if (data->Has(SecureKey)) {
    v8::Handle<v8::Value> v = data->Get(SecureKey);
    secure = TRI_ObjectToBoolean(v);
  }
  TRI_GET_GLOBAL_STRING(HttpOnlyKey);
  if (data->Has(HttpOnlyKey)) {
    v8::Handle<v8::Value> v = data->Get(HttpOnlyKey);
    httpOnly = TRI_ObjectToBoolean(v);
  }

  response->setCookie(name, value, lifeTimeSeconds, path, domain, secure,
                      httpOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a C++ HttpRequest to a V8 request object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> RequestCppToV8(v8::Isolate* isolate,
                                             TRI_v8_global_t const* v8g,
                                             GeneralRequest* request) {
  // setup the request
  v8::Handle<v8::Object> req = v8::Object::New(isolate);

  // Example:
  //      {
  //        path : "/full/path/suffix1/suffix2",
  //
  //        prefix : "/full/path",
  //
  //        "suffix" : [
  //          "suffix1",
  //          "suffix2"
  //        ],
  //
  //        "parameters" : {
  //          "init" : "true"
  //        },
  //
  //        "headers" : {
  //          "accept" : "text/html",
  //          "accept-encoding" : "gzip, deflate",
  //          "accept-language" : "de-de,en-us;q=0.7,en;q=0.3",
  //          "user-agent" : "Mozilla/5.0"
  //        },
  //
  //        "cookies" : {
  //          "ARANGODB_SESSION_ID" : "0cwuzusd23nw3qiwui84uwqwqw23e"
  //        },
  //
  //        "requestType" : "GET",
  //        "requestBody" : "... only for PUT and POST ...",
  //        "user" : "authenticatedUser"
  //      }

  TRI_GET_GLOBAL_STRING(AuthorizedKey);
  if (request->authenticated()) {
    req->ForceSet(AuthorizedKey, v8::True(isolate));
  } else {
    req->ForceSet(AuthorizedKey, v8::False(isolate));
  }

  // create user or null
  std::string const& user = request->user();

  TRI_GET_GLOBAL_STRING(UserKey);
  if (user.empty()) {
    req->ForceSet(UserKey, v8::Null(isolate));
  } else {
    req->ForceSet(UserKey, TRI_V8_STD_STRING(isolate, user));
  }

  // create database attribute
  std::string const& database = request->databaseName();
  TRI_ASSERT(!database.empty());

  TRI_GET_GLOBAL_STRING(DatabaseKey);
  req->ForceSet(DatabaseKey, TRI_V8_STD_STRING(isolate, database));

  // set the full url
  std::string const& fullUrl = request->fullUrl();
  TRI_GET_GLOBAL_STRING(UrlKey);
  req->ForceSet(UrlKey, TRI_V8_STD_STRING(isolate, fullUrl));

  // set the protocol
  char const* protocol = request->protocol();
  TRI_GET_GLOBAL_STRING(ProtocolKey);
  req->ForceSet(ProtocolKey, TRI_V8_ASCII_STRING(isolate, protocol));

  // set the task id
  std::string const taskId(StringUtils::itoa(request->clientTaskId()));

  // set the connection info
  const ConnectionInfo& info = request->connectionInfo();

  v8::Handle<v8::Object> serverArray = v8::Object::New(isolate);
  TRI_GET_GLOBAL_STRING(AddressKey);
  serverArray->ForceSet(AddressKey, TRI_V8_STD_STRING(isolate, info.serverAddress));
  TRI_GET_GLOBAL_STRING(PortKey);
  serverArray->ForceSet(PortKey, v8::Number::New(isolate, info.serverPort));
  TRI_GET_GLOBAL_STRING(EndpointKey);
  serverArray->ForceSet(EndpointKey, TRI_V8_STD_STRING(isolate, Endpoint::uriForm(info.endpoint)));
  TRI_GET_GLOBAL_STRING(ServerKey);
  req->ForceSet(ServerKey, serverArray);

  TRI_GET_GLOBAL_STRING(PortTypeKey);
  req->ForceSet(
      PortTypeKey, TRI_V8_STD_STRING(isolate, info.portType()),
      static_cast<v8::PropertyAttribute>(v8::ReadOnly));

  v8::Handle<v8::Object> clientArray = v8::Object::New(isolate);
  clientArray->ForceSet(AddressKey, TRI_V8_STD_STRING(isolate, info.clientAddress));
  clientArray->ForceSet(PortKey, v8::Number::New(isolate, info.clientPort));
  TRI_GET_GLOBAL_STRING(IdKey);
  clientArray->ForceSet(IdKey, TRI_V8_STD_STRING(isolate, taskId));
  TRI_GET_GLOBAL_STRING(ClientKey);
  req->ForceSet(ClientKey, clientArray);

  req->ForceSet(TRI_V8_ASCII_STRING(isolate, "internals"),
                v8::External::New(isolate, request));

  // copy prefix
  std::string path = request->prefix();
  TRI_GET_GLOBAL_STRING(PrefixKey);
  req->ForceSet(PrefixKey, TRI_V8_STD_STRING(isolate, path));

  // copy header fields
  v8::Handle<v8::Object> headerFields = v8::Object::New(isolate);
  // intentional copy, as we will modify the headers later
  auto headers = request->headers();

  TRI_GET_GLOBAL_STRING(HeadersKey);
  req->ForceSet(HeadersKey, headerFields);
  TRI_GET_GLOBAL_STRING(RequestTypeKey);
  TRI_GET_GLOBAL_STRING(RequestBodyKey);

  auto setRequestBodyJsonOrVPack = [&]() {
    if (rest::ContentType::JSON == request->contentType()) {
      auto httpreq = dynamic_cast<HttpRequest*>(request);
      if (httpreq == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request type");
      }
      std::string const& body = httpreq->body();
      req->ForceSet(RequestBodyKey, TRI_V8_STD_STRING(isolate, body));
      headers[StaticStrings::ContentLength] = StringUtils::itoa(request->contentLength());
    } else if (rest::ContentType::VPACK == request->contentType()) {
      // the VPACK is passed as it is to to Javascript
      // FIXME not every VPack can be converted to JSON
      VPackSlice slice = request->payload();
      std::string jsonString = slice.toJson();

      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "json handed into v8 request:\n"
          << jsonString;

      req->ForceSet(RequestBodyKey, TRI_V8_STD_STRING(isolate, jsonString));
      headers[StaticStrings::ContentLength] = StringUtils::itoa(jsonString.size());
      headers[StaticStrings::ContentTypeHeader] = StaticStrings::MimeTypeJson;
    } else {
      throw std::logic_error("unhandled request type");
    }
  };

  // copy request type
  switch (request->requestType()) {
    case rest::RequestType::POST: {
      TRI_GET_GLOBAL_STRING(PostConstant);
      req->ForceSet(RequestTypeKey, PostConstant);
      setRequestBodyJsonOrVPack();
      break;
    }

    case rest::RequestType::PUT: {
      TRI_GET_GLOBAL_STRING(PutConstant);
      req->ForceSet(RequestTypeKey, PutConstant);
      setRequestBodyJsonOrVPack();
      break;
    }

    case rest::RequestType::PATCH: {
      TRI_GET_GLOBAL_STRING(PatchConstant);
      req->ForceSet(RequestTypeKey, PatchConstant);
      setRequestBodyJsonOrVPack();
      break;
    }
    case rest::RequestType::OPTIONS: {
      TRI_GET_GLOBAL_STRING(OptionsConstant);
      req->ForceSet(RequestTypeKey, OptionsConstant);
      break;
    }
    case rest::RequestType::DELETE_REQ: {
      TRI_GET_GLOBAL_STRING(DeleteConstant);
      req->ForceSet(RequestTypeKey, DeleteConstant);
      break;
    }
    case rest::RequestType::HEAD: {
      TRI_GET_GLOBAL_STRING(HeadConstant);
      req->ForceSet(RequestTypeKey, HeadConstant);
      break;
    }
    case rest::RequestType::GET: {
      default:
        TRI_GET_GLOBAL_STRING(GetConstant);
        req->ForceSet(RequestTypeKey, GetConstant);
        break;
    }
  }
  
  for (auto const& it : headers) {
    headerFields->ForceSet(TRI_V8_STD_STRING(isolate, it.first),
                           TRI_V8_STD_STRING(isolate, it.second));
  }

  // copy request parameter
  v8::Handle<v8::Object> valuesObject = v8::Object::New(isolate);

  for (auto& it : request->values()) {
    valuesObject->ForceSet(TRI_V8_STD_STRING(isolate, it.first),
                           TRI_V8_STD_STRING(isolate, it.second));
  }

  // copy request array parameter (a[]=1&a[]=2&...)
  for (auto& arrayValue : request->arrayValues()) {
    std::string const& k = arrayValue.first;
    std::vector<std::string> const& v = arrayValue.second;

    v8::Handle<v8::Array> list =
        v8::Array::New(isolate, static_cast<int>(v.size()));

    for (size_t i = 0; i < v.size(); ++i) {
      list->Set((uint32_t)i, TRI_V8_STD_STRING(isolate, v[i]));
    }

    valuesObject->ForceSet(TRI_V8_STD_STRING(isolate, k), list);
  }

  TRI_GET_GLOBAL_STRING(ParametersKey);
  req->ForceSet(ParametersKey, valuesObject);

  // copy cookie -- only for http protocol
  if (request->transportType() == Endpoint::TransportType::HTTP) {  // FIXME
    v8::Handle<v8::Object> cookiesObject = v8::Object::New(isolate);

    HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(request);
    if (httpRequest == nullptr) {
      // maybe we can just continue
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request type");
    } else {
      for (auto& it : httpRequest->cookieValues()) {
        cookiesObject->ForceSet(TRI_V8_STD_STRING(isolate, it.first),
                                TRI_V8_STD_STRING(isolate, it.second));
      }
    }
    TRI_GET_GLOBAL_STRING(CookiesKey);
    req->ForceSet(CookiesKey, cookiesObject);
  }

  return req;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a C++ HttpRequest to a V8 request object
////////////////////////////////////////////////////////////////////////////////

// TODO this needs to be generalized
static void ResponseV8ToCpp(v8::Isolate* isolate, TRI_v8_global_t const* v8g,
                            GeneralRequest* request,
                            v8::Handle<v8::Object> const res,
                            GeneralResponse* response) {
  TRI_ASSERT(request != nullptr);

  rest::ResponseCode code = rest::ResponseCode::OK;

  using arangodb::Endpoint;

  // set response code
  TRI_GET_GLOBAL_STRING(ResponseCodeKey);
  if (res->Has(ResponseCodeKey)) {
    // Windows has issues with converting from a double to an enumeration type
    code = (rest::ResponseCode)(
        (int)(TRI_ObjectToDouble(res->Get(ResponseCodeKey))));
  }
  response->setResponseCode(code);

  // string should not be used
  std::string contentType = "application/json";
  bool autoContent = true;
  TRI_GET_GLOBAL_STRING(ContentTypeKey);
  if (res->Has(ContentTypeKey)) {
    contentType = TRI_ObjectToString(res->Get(ContentTypeKey));

    if (contentType.find("application/json") == std::string::npos) {
      autoContent = false;
    }
    switch (response->transportType()) {
      case Endpoint::TransportType::HTTP:
        if (autoContent) {
          response->setContentType(rest::ContentType::JSON);
        } else {
          response->setContentType(contentType);
        }
        break;

      case Endpoint::TransportType::VST:
        response->setHeaderNC(arangodb::StaticStrings::ContentTypeHeader,
                              contentType);
        break;

      default:
        throw std::logic_error("unknown transport type");
    }
  }

  // .........................................................................
  // body
  // .........................................................................
  //

  bool bodySet = false;
  TRI_GET_GLOBAL_STRING(BodyKey);
  if (res->Has(BodyKey)) {
    // check if we should apply result transformations
    // transformations turn the result from one type into another
    // a Javascript action can request transformations by
    // putting a list of transformations into the res.transformations
    // array, e.g. res.transformations = [ "base64encode" ]
    TRI_GET_GLOBAL_STRING(TransformationsKey);
    v8::Handle<v8::Value> transformArray = res->Get(TransformationsKey);

    switch (response->transportType()) {
      case Endpoint::TransportType::HTTP: {
        //  OBI FIXME - vpack
        //  HTTP SHOULD USE vpack interface

        HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(response);
        if (transformArray->IsArray()) {
          TRI_GET_GLOBAL_STRING(BodyKey);
          std::string out(TRI_ObjectToString(res->Get(BodyKey)));
          v8::Handle<v8::Array> transformations =
              transformArray.As<v8::Array>();

          for (uint32_t i = 0; i < transformations->Length(); i++) {
            v8::Handle<v8::Value> transformator =
                transformations->Get(v8::Integer::New(isolate, i));
            std::string name = TRI_ObjectToString(transformator);

            // check available transformations
            if (name == "base64encode") {
              // base64-encode the result
              out = StringUtils::encodeBase64(out);
              // set the correct content-encoding header
              response->setHeaderNC(StaticStrings::ContentEncoding,
                                    StaticStrings::Base64);
            } else if (name == "base64decode") {
              // base64-decode the result
              out = StringUtils::decodeBase64(out);
              // set the correct content-encoding header
              response->setHeaderNC(StaticStrings::ContentEncoding,
                                    StaticStrings::Binary);
            }
          }

          // what type is out? always json?
          httpResponse->body().appendText(out);
        } else {
          TRI_GET_GLOBAL_STRING(BodyKey);
          v8::Handle<v8::Value> b = res->Get(BodyKey);
          if (V8Buffer::hasInstance(isolate, b)) {
            // body is a Buffer
            auto obj = b.As<v8::Object>();
            httpResponse->body().appendText(V8Buffer::data(obj),
                                            V8Buffer::length(obj));
          } else if (autoContent && 
                     request->contentTypeResponse() == rest::ContentType::VPACK) {
            // use velocypack
            try {
              std::string json = TRI_ObjectToString(res->Get(BodyKey));
              VPackBuffer<uint8_t> buffer;
              VPackBuilder builder(buffer);
              VPackParser parser(builder);
              parser.parse(json);
              httpResponse->setContentType(rest::ContentType::VPACK);
              httpResponse->setPayload(std::move(buffer), true);
            } catch (...) {
              httpResponse->body().appendText(
                  TRI_ObjectToString(res->Get(BodyKey)));
            }
          } else {
            // treat body as a string
            httpResponse->body().appendText(
                TRI_ObjectToString(res->Get(BodyKey)));
          }
        }
      } break;

      case Endpoint::TransportType::VST: {
        VPackBuffer<uint8_t> buffer;
        VPackBuilder builder(buffer);

        v8::Handle<v8::Value> v8Body = res->Get(BodyKey);
        std::string out;

        // decode and set out
        if (transformArray->IsArray()) {
          TRI_GET_GLOBAL_STRING(BodyKey);
          out = TRI_ObjectToString(
              res->Get(BodyKey));  // there is one case where we
                                   // do not need a string
          v8::Handle<v8::Array> transformations =
              transformArray.As<v8::Array>();

          for (uint32_t i = 0; i < transformations->Length(); i++) {
            v8::Handle<v8::Value> transformator =
                transformations->Get(v8::Integer::New(isolate, i));
            std::string name = TRI_ObjectToString(transformator);

            // we do not decode in the vst case
            // check available transformations
            if (name == "base64decode") {
              out = StringUtils::decodeBase64(out);
            }
          }
        }

        // out is not set
        if (out.empty()) {
          if (autoContent && !V8Buffer::hasInstance(isolate, v8Body)) {
            if (v8Body->IsString()) {
              out = TRI_ObjectToString(res->Get(BodyKey));  // should get moved
            } else {
              TRI_V8ToVPack(isolate, builder, v8Body, false);
            }
          } else if (V8Buffer::hasInstance(
                         isolate,
                         v8Body)) {  // body form buffer - could
                                     // contain json or not
            // REVIEW (fc) - is this correct?
            auto obj = v8Body.As<v8::Object>();
            out = std::string(V8Buffer::data(obj), V8Buffer::length(obj));
          } else {  // body is text - does not contain json
            out = TRI_ObjectToString(res->Get(BodyKey));  // should get moved
          }
        }

        // there is a text body
        if (!out.empty()) {
          bool gotJson = false;
          if (autoContent) {  // the text body could contain an object
            try {
              VPackParser parser(builder);  // add json as vpack to the builder
              parser.parse(out, false);
              gotJson = true;
            } catch (...) {  // do nothing
                             // json could not be converted
                             // there was no json - change content type?
              LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
                  << "failed to parse json:\n"
                  << out;
            }
          }

          if (!gotJson) {
            builder.add(VPackValue(
                out));  // add output to the builder - when not added via parser
          }
        }

        response->setContentType(rest::ContentType::VPACK);
        response->setPayload(std::move(buffer), true);
        break;
      }

      default: { throw std::logic_error("unknown transport type"); }
    }
    bodySet = true;
  }

  // .........................................................................
  // body from file
  // .........................................................................
  TRI_GET_GLOBAL_STRING(BodyFromFileKey);
  if (!bodySet && res->Has(BodyFromFileKey)) {
    TRI_Utf8ValueNFC filename(res->Get(BodyFromFileKey));
    size_t length;
    char* content = TRI_SlurpFile(*filename, &length);

    if (content == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_FILE_NOT_FOUND,
          std::string("unable to read file '") + *filename + "'");
    }

    switch (response->transportType()) {
      case Endpoint::TransportType::HTTP: {
        HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(response);
        httpResponse->body().appendText(content, length);
        TRI_FreeString(content);
      } break;

      case Endpoint::TransportType::VST: {
        VPackBuffer<uint8_t> buffer;
        VPackBuilder builder(buffer);
        builder.add(
            VPackValuePair(reinterpret_cast<uint8_t const*>(content), length));
        TRI_FreeString(content);

        // create vpack from file
        response->setContentType(rest::ContentType::VPACK);
        response->setPayload(std::move(buffer), true);
      } break;

      default:
        TRI_FreeString(content);
        throw std::logic_error("unknown transport type");
    }
  }

  // .........................................................................
  // headers
  // .........................................................................

  TRI_GET_GLOBAL_STRING(HeadersKey);

  if (res->Has(HeadersKey)) {
    v8::Handle<v8::Value> val = res->Get(HeadersKey);
    v8::Handle<v8::Object> v8Headers = val.As<v8::Object>();

    if (v8Headers->IsObject()) {
      v8::Handle<v8::Array> props = v8Headers->GetPropertyNames();

      for (uint32_t i = 0; i < props->Length(); i++) {
        v8::Handle<v8::Value> key = props->Get(v8::Integer::New(isolate, i));
        response->setHeader(TRI_ObjectToString(key),
                            TRI_ObjectToString(v8Headers->Get(key)));
      }
    }
  }

  // .........................................................................
  // cookies
  // .........................................................................
  
  TRI_GET_GLOBAL_STRING(CookiesKey);
  if (res->Has(CookiesKey)) {
    v8::Handle<v8::Value> val = res->Get(CookiesKey);
    v8::Handle<v8::Object> v8Cookies = val.As<v8::Object>();

    switch (response->transportType()) {
      case Endpoint::TransportType::HTTP: {
        HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(response);
        if (v8Cookies->IsArray()) {
          v8::Handle<v8::Array> v8Array = v8Cookies.As<v8::Array>();

          for (uint32_t i = 0; i < v8Array->Length(); i++) {
            v8::Handle<v8::Value> v8Cookie = v8Array->Get(i);
            if (v8Cookie->IsObject()) {
              AddCookie(isolate, v8g, httpResponse, v8Cookie.As<v8::Object>());
            }
          }
        } else if (v8Cookies->IsObject()) {
          // one cookie
          AddCookie(isolate, v8g, httpResponse, v8Cookies);
        }
      } break;

      case Endpoint::TransportType::VST:
        break;

      default:
        throw std::logic_error("unknown transport type");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

static TRI_action_result_t ExecuteActionVocbase(
    TRI_vocbase_t* vocbase, v8::Isolate* isolate, TRI_action_t const* action,
    v8::Handle<v8::Function> callback, GeneralRequest* request,
    GeneralResponse* response) {
  v8::HandleScope scope(isolate);
  v8::TryCatch tryCatch;

  if (response == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid response");
  }

  TRI_GET_GLOBALS();

  v8::Handle<v8::Object> req = RequestCppToV8(isolate, v8g, request);

  // copy suffix, which comes from the action:
  std::string path = request->prefix();
  std::vector<std::string> const& suffixes = request->decodedSuffixes();
  std::vector<std::string> const& rawSuffixes = request->suffixes();

  uint32_t index = 0;
  char const* sep = "";

  size_t const n = suffixes.size();
  v8::Handle<v8::Array> suffixArray = v8::Array::New(isolate, static_cast<int>(n - action->_urlParts));
  v8::Handle<v8::Array> rawSuffixArray = v8::Array::New(isolate, static_cast<int>(n - action->_urlParts));

  for (size_t s = action->_urlParts; s < n; ++s) {
    suffixArray->Set(index, TRI_V8_STD_STRING(isolate, suffixes[s]));
    rawSuffixArray->Set(index, TRI_V8_STD_STRING(isolate, rawSuffixes[s]));
    ++index;

    path += sep + suffixes[s];
    sep = "/";
  }

  TRI_GET_GLOBAL_STRING(SuffixKey);
  req->ForceSet(SuffixKey, suffixArray);
  TRI_GET_GLOBAL_STRING(RawSuffixKey);
  req->ForceSet(RawSuffixKey, rawSuffixArray);

  // copy full path
  TRI_GET_GLOBAL_STRING(PathKey);
  req->ForceSet(PathKey, TRI_V8_STD_STRING(isolate, path));

  // create the response object
  v8::Handle<v8::Object> res = v8::Object::New(isolate);

  // register request & response in the context
  v8g->_currentRequest = req;
  v8g->_currentResponse = res;

  // execute the callback
  v8::Handle<v8::Value> args[2] = {req, res};

  // handle C++ exceptions that happen during dynamic script execution
  int errorCode;
  std::string errorMessage;

  try {
    callback->Call(callback, 2, args);
    errorCode = TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    errorCode = ex.code();
    errorMessage = ex.what();
  } catch (std::bad_alloc const&) {
    errorCode = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    errorCode = TRI_ERROR_INTERNAL;
  }

  // invalidate request / response objects
  v8g->_currentRequest = v8::Undefined(isolate);
  v8g->_currentResponse = v8::Undefined(isolate);

  // convert the result
  TRI_action_result_t result;
  result.isValid = true;

  if (errorCode != TRI_ERROR_NO_ERROR) {
    result.isValid = false;
    result.canceled = false;

    response->setResponseCode(rest::ResponseCode::SERVER_ERROR);

    if (errorMessage.empty()) {
      errorMessage = TRI_errno_string(errorCode);
    }

    // TODO (obi)
    if (response->transportType() == Endpoint::TransportType::HTTP) {  // FIXME
      ((HttpResponse*)response)->body().appendText(errorMessage);
    }

  }

  else if (v8g->_canceled) {
    result.isValid = false;
    result.canceled = true;
  }

  else if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      response->setResponseCode(rest::ResponseCode::SERVER_ERROR);

      std::string jsError = TRI_StringifyV8Exception(isolate, &tryCatch);
      LOG_TOPIC(WARN, arangodb::Logger::V8) << "Caught an error while executing an action: " << jsError;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      // TODO how to generalize this?
      if (response->transportType() ==
          Endpoint::TransportType::HTTP) {  // FIXME
        ((HttpResponse*)response)
            ->body()
            .appendText(TRI_StringifyV8Exception(isolate, &tryCatch));
      }
#endif
    } else {
      v8g->_canceled = true;
      result.isValid = false;
      result.canceled = true;
    }
  }

  else {
    ResponseV8ToCpp(isolate, v8g, request, res, response);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new action
///
/// @FUN{internal.defineAction(@FA{name}, @FA{callback}, @FA{parameter})}
////////////////////////////////////////////////////////////////////////////////

static void JS_DefineAction(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  if (args.Length() != 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "defineAction(<name>, <callback>, <parameter>)");
  }

  // extract the action name
  TRI_Utf8ValueNFC utf8name(args[0]);

  if (*utf8name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<name> must be an UTF-8 string");
  }

  std::string name = *utf8name;

  // extract the action callback
  if (!args[1]->IsFunction()) {
    TRI_V8_THROW_TYPE_ERROR("<callback> must be a function");
  }

  v8::Handle<v8::Function> callback = v8::Handle<v8::Function>::Cast(args[1]);

  // extract the options
  v8::Handle<v8::Object> options;

  if (args[2]->IsObject()) {
    options = args[2]->ToObject();
  } else {
    options = v8::Object::New(isolate);
  }

  // create an action with the given options
  auto action = std::make_shared<v8_action_t>();
  ParseActionOptions(isolate, v8g, action.get(), options);

  // store an action with the given name
  // note: this may return a previous action for the same name
  std::shared_ptr<TRI_action_t> actionForName = TRI_DefineActionVocBase(name, action);
  
  v8_action_t* v8ActionForName = dynamic_cast<v8_action_t*>(actionForName.get());

  if (v8ActionForName != nullptr) {
    v8ActionForName->createCallback(isolate, callback);
  } else {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "cannot create callback for V8 action";
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief eventually executes a function in all contexts
///
/// @FUN{internal.executeGlobalContextFunction(@FA{function-definition})}
////////////////////////////////////////////////////////////////////////////////

static void JS_ExecuteGlobalContextFunction(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "executeGlobalContextFunction(<function-type>)");
  }

  // extract the action name
  v8::String::Utf8Value utf8def(args[0]);

  if (*utf8def == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<definition> must be a UTF-8 function definition");
  }

  std::string const def = std::string(*utf8def, utf8def.length());

  // and pass it to the V8 contexts
  if (!V8DealerFeature::DEALER->addGlobalContextMethod(def)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid action definition");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current request
///
/// @FUN{internal.getCurrentRequest()}
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCurrentRequest(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getCurrentRequest()");
  }

  TRI_V8_RETURN(v8g->_currentRequest);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the raw body of the current request
///
/// @FUN{internal.rawRequestBody()}
////////////////////////////////////////////////////////////////////////////////

static void JS_RawRequestBody(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("rawRequestBody(req)");
  }

  v8::Handle<v8::Value> current = args[0];
  if (current->IsObject()) {
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(current);
    v8::Handle<v8::Value> property = obj->Get(TRI_V8_ASCII_STRING(isolate, "internals"));
    if (property->IsExternal()) {
      v8::Handle<v8::External> e = v8::Handle<v8::External>::Cast(property);

      GeneralRequest* request = static_cast<GeneralRequest*>(e->Value());

      switch (request->transportType()) {
        case Endpoint::TransportType::HTTP: {
          auto httpRequest = static_cast<arangodb::HttpRequest*>(e->Value());
          if (httpRequest != nullptr) {
            std::string bodyStr;
            if (rest::ContentType::VPACK == request->contentType()) {
              VPackSlice slice = request->payload();
              bodyStr = slice.toJson();
            } else {
              bodyStr = httpRequest->body();
            }
            V8Buffer* buffer =
                V8Buffer::New(isolate, bodyStr.c_str(), bodyStr.size());

            TRI_V8_RETURN(buffer->_handle);
          }
        } break;

        case Endpoint::TransportType::VST: {
          if (request != nullptr) {
            auto slice = request->payload();
            V8Buffer* buffer = nullptr;
            if (slice.isNone()) {
              buffer = V8Buffer::New(isolate, "", 0);
            } else {
              std::string bodyStr = slice.toJson();
              buffer = V8Buffer::New(isolate, bodyStr.c_str(), bodyStr.size());
            }
            TRI_V8_RETURN(buffer->_handle);
          }
        } break;
      }
    }
  }

  // VPackSlice slice(data);
  // v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, slice);
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the raw body of the current request
///
/// @FUN{internal.rawRequestBody()}
////////////////////////////////////////////////////////////////////////////////

static void JS_RequestParts(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("requestParts(req)");
  }

  v8::Handle<v8::Value> current = args[0];
  if (current->IsObject()) {
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(current);
    v8::Handle<v8::Value> property = obj->Get(TRI_V8_ASCII_STRING(isolate, "internals"));
    if (property->IsExternal()) {
      v8::Handle<v8::External> e = v8::Handle<v8::External>::Cast(property);
      auto request = static_cast<arangodb::HttpRequest*>(e->Value());

      std::string const& bodyStr = request->body();
      char const* beg = bodyStr.c_str();
      char const* end = beg + bodyStr.size();

      while (beg < end && (*beg == '\r' || *beg == '\n' || *beg == ' ')) {
        ++beg;
      }

      // find delimiter
      char const* ptr = beg;
      while (ptr < end && *ptr == '-') {
        ++ptr;
      }

      while (ptr < end && *ptr != '\r' && *ptr != '\n') {
        ++ptr;
      }
      if (ptr == beg) {
        // oops
        TRI_V8_THROW_EXCEPTION_PARAMETER("request is no multipart request");
      }

      std::string const delimiter(beg, ptr - beg);
      if (ptr < end && *ptr == '\r') {
        ++ptr;
      }
      if (ptr < end && *ptr == '\n') {
        ++ptr;
      }

      std::vector<std::pair<char const*, size_t>> parts;

      while (ptr < end) {
        char const* p = TRI_IsContainedMemory(ptr, end - ptr, delimiter.c_str(),
                                              delimiter.size());
        if (p == nullptr || p + delimiter.size() + 2 >= end || p - 2 <= ptr) {
          TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data");
        }

        char const* q = p;
        if (*(q - 1) == '\n') {
          --q;
        }
        if (*(q - 1) == '\r') {
          --q;
        }

        parts.push_back(std::make_pair(ptr, q - ptr));
        ptr = p + delimiter.size();
        if (*ptr == '-' && *(ptr + 1) == '-') {
          // eom
          break;
        }
        if (*ptr == '\r') {
          ++ptr;
        }
        if (ptr < end && *ptr == '\n') {
          ++ptr;
        }
      }

      v8::Handle<v8::Array> result = v8::Array::New(isolate);

      uint32_t j = 0;
      for (auto& part : parts) {
        v8::Handle<v8::Object> headersObject = v8::Object::New(isolate);

        auto ptr = part.first;
        auto end = part.first + part.second;
        char const* data = nullptr;

        while (ptr < end) {
          while (ptr < end && *ptr == ' ') {
            ++ptr;
          }
          if (ptr < end && (*ptr == '\r' || *ptr == '\n')) {
            // end of headers
            if (*ptr == '\r') {
              ++ptr;
            }
            if (ptr < end && *ptr == '\n') {
              ++ptr;
            }
            data = ptr;
            break;
          }

          // header line
          char const* eol = TRI_IsContainedMemory(ptr, end - ptr, "\r\n", 2);
          if (eol == nullptr) {
            eol = TRI_IsContainedMemory(ptr, end - ptr, "\n", 1);
          }
          if (eol == nullptr) {
            TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data");
          }
          char const* colon = TRI_IsContainedMemory(ptr, end - ptr, ":", 1);
          if (colon == nullptr) {
            TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data");
          }
          char const* p = colon;
          while (p > ptr && *(p - 1) == ' ') {
            --p;
          }
          ++colon;
          while (colon < eol && *colon == ' ') {
            ++colon;
          }
          char const* q = eol;
          while (q > ptr && *(q - 1) == ' ') {
            --q;
          }

          headersObject->Set(TRI_V8_PAIR_STRING(isolate, ptr, (int)(p - ptr)),
                             TRI_V8_PAIR_STRING(isolate, colon, (int)(eol - colon)));

          ptr = eol;
          if (*ptr == '\r') {
            ++ptr;
          }
          if (ptr < end && *ptr == '\n') {
            ++ptr;
          }
        }

        if (data == nullptr) {
          TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data");
        }

        v8::Handle<v8::Object> partObject = v8::Object::New(isolate);
        partObject->Set(TRI_V8_ASCII_STRING(isolate, "headers"), headersObject);

        V8Buffer* buffer = V8Buffer::New(isolate, data, end - data);
        auto localHandle = v8::Local<v8::Object>::New(isolate, buffer->_handle);

        partObject->Set(TRI_V8_ASCII_STRING(isolate, "data"), localHandle);

        result->Set(j++, partObject);
      }

      TRI_V8_RETURN(result);
    }
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current response
///
/// @FUN{internal.getCurrentRequest()}
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCurrentResponse(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getCurrentResponse()");
  }

  TRI_V8_RETURN(v8g->_currentResponse);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 actions function inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Actions(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  v8::HandleScope scope(isolate);

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_DEFINE_ACTION"),
                               JS_DefineAction);
  TRI_AddGlobalFunctionVocbase(
      isolate,
      TRI_V8_ASCII_STRING(isolate, "SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION"),
      JS_ExecuteGlobalContextFunction);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_GET_CURRENT_REQUEST"),
                               JS_GetCurrentRequest);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING(isolate, "SYS_GET_CURRENT_RESPONSE"),
                               JS_GetCurrentResponse);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_RAW_REQUEST_BODY"),
                               JS_RawRequestBody, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_REQUEST_PARTS"),
                               JS_RequestParts, true);
}

////////////////////////////////////////////////////////////////////////////////
/// Below Debugging Functions. Only compiled in maintainer mode.
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static int clusterSendToAllServers(
    std::string const& dbname,
    std::string const& path,  // Note: Has to be properly encoded!
    arangodb::rest::RequestType const& method, std::string const& body) {
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    return TRI_ERROR_SHUTTING_DOWN;
  }
  std::string url = "/_db/" + StringUtils::urlEncode(dbname) + "/" + path;

  // Have to propagate to DB Servers
  std::vector<ServerID> DBServers;
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  auto reqBodyString = std::make_shared<std::string>(body);

  DBServers = ci->getCurrentDBServers();
  std::unordered_map<std::string, std::string> headers;
  for (auto const& sid : DBServers) {
    cc->asyncRequest(coordTransactionID, "server:" + sid, method, url,
                     reqBodyString, headers, nullptr, 3600.0);
  }

  // Now listen to the results:
  size_t count = DBServers.size();

  for (; count > 0; count--) {
    auto res = cc->wait(coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_TIMEOUT) {
      cc->drop(coordTransactionID, 0, "");
      return TRI_ERROR_CLUSTER_TIMEOUT;
    }
    if (res.status == CL_COMM_ERROR || res.status == CL_COMM_DROPPED ||
        res.status == CL_COMM_BACKEND_UNAVAILABLE) {
      cc->drop(coordTransactionID, 0, "");
      return TRI_ERROR_INTERNAL;
    }
  }
  return TRI_ERROR_NO_ERROR;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief intentionally causes a segfault
///
/// @FUN{internal.debugSegfault(@FA{message})}
///
/// intentionally cause a segmentation violation
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static void JS_DebugSegfault(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("debugSegfault(<message>)");
  }

  std::string const message = TRI_ObjectToString(args[0]);

  TRI_SegfaultDebugging(message.c_str());

  // we may get here if we are in non-maintainer mode

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a failure point
///
/// @FUN{internal.debugSetFailAt(@FA{point})}
///
/// Set a point for an intentional system failure
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static void JS_DebugSetFailAt(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  if (v8g->_vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  std::string dbname(v8g->_vocbase->name());

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("debugSetFailAt(<point>)");
  }

  std::string const point = TRI_ObjectToString(args[0]);

  TRI_AddFailurePointDebugging(point.c_str());

  if (ServerState::instance()->isCoordinator()) {
    int res = clusterSendToAllServers(
        dbname, "_admin/debug/failat/" + StringUtils::urlEncode(point),
        arangodb::rest::RequestType::PUT, "");
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a failure point
///
/// @FUN{internal.debugRemoveFailAt(@FA{point})}
///
/// Remove a point for an intentional system failure
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static void JS_DebugRemoveFailAt(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  if (v8g->_vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  std::string dbname(v8g->_vocbase->name());

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("debugRemoveFailAt(<point>)");
  }

  std::string const point = TRI_ObjectToString(args[0]);

  TRI_RemoveFailurePointDebugging(point.c_str());

  if (ServerState::instance()->isCoordinator()) {
    int res = clusterSendToAllServers(
        dbname, "_admin/debug/failat/" + StringUtils::urlEncode(point),
        arangodb::rest::RequestType::DELETE_REQ, "");
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief clears all failure points
///
/// @FUN{internal.debugClearFailAt()}
///
/// Remove all points for intentional system failures
////////////////////////////////////////////////////////////////////////////////

static void JS_DebugClearFailAt(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("debugClearFailAt()");
  }

// if failure testing is not enabled, this is a no-op
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  TRI_ClearFailurePointsDebugging();

  if (ServerState::instance()->isCoordinator()) {
    TRI_GET_GLOBALS();

    if (v8g->_vocbase == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }
    std::string dbname(v8g->_vocbase->name());

    int res =
        clusterSendToAllServers(dbname, "_admin/debug/failat",
                                arangodb::rest::RequestType::DELETE_REQ, "");
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

#endif

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8DebugUtils(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  // debugging functions
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_DEBUG_CLEAR_FAILAT"),
                               JS_DebugClearFailAt);
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING(isolate, "SYS_DEBUG_SEGFAULT"),
                               JS_DebugSegfault);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_DEBUG_SET_FAILAT"),
                               JS_DebugSetFailAt);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_DEBUG_REMOVE_FAILAT"),
                               JS_DebugRemoveFailAt);
#endif
}
