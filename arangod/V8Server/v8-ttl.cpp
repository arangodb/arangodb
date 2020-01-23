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

#include "v8-ttl.h"

#include "Basics/Result.h"
#include "RestServer/TtlFeature.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "VocBase/Methods/Ttl.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief returns the TTL properties
static void JS_TtlProperties(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("properties(<object>)");
  }

  VPackBuilder builder;
  Result result;

  TRI_GET_GLOBALS();
  if (args.Length() == 0) {
    // get properties
    result = methods::Ttl::getProperties(v8g->_server.getFeature<TtlFeature>(), builder);
  } else {
    // set properties
    VPackBuilder properties;
    
    int res = TRI_V8ToVPack(isolate, properties, args[0], false);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    result = methods::Ttl::setProperties(v8g->_server.getFeature<TtlFeature>(),
                                         properties.slice(), builder);
  }
  
  if (result.fail()) {
    THROW_ARANGO_EXCEPTION(result);
  }

  v8::Handle<v8::Value> obj = TRI_VPackToV8(isolate, builder.slice());

  TRI_V8_RETURN(obj);
  TRI_V8_TRY_CATCH_END
}

/// @brief returns the TTL statistics
static void JS_TtlStatistics(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  VPackBuilder builder;
  TRI_GET_GLOBALS();
  Result result =
      methods::Ttl::getStatistics(v8g->_server.getFeature<TtlFeature>(), builder);

  if (result.fail()) {
    THROW_ARANGO_EXCEPTION(result);
  }

  v8::Handle<v8::Value> obj = TRI_VPackToV8(isolate, builder.slice());

  TRI_V8_RETURN(obj);
  TRI_V8_TRY_CATCH_END
}

/// @brief initializes the ttl functions
void TRI_InitV8Ttl(v8::Isolate* isolate) {
  v8::HandleScope scope(isolate);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_TTL_STATISTICS"),
                               JS_TtlStatistics);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_TTL_PROPERTIES"),
                               JS_TtlProperties);
}
