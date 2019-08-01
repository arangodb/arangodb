////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "RestAnalyzerHandler.h"
#include "Basics/StringUtils.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "utils/string.hpp"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

namespace arangodb {
namespace iresearch {

RestAnalyzerHandler::RestAnalyzerHandler( // constructor
    arangodb::GeneralRequest* request, // request
    arangodb::GeneralResponse* response // response
): RestVocbaseBaseHandler(request, response) {
}

void RestAnalyzerHandler::createAnalyzer( // create
    IResearchAnalyzerFeature& analyzers
) {
  TRI_ASSERT(_request); // ensured by execute()

  if (_request->payload().isEmptyObject()) {
    generateError( // generate error
      arangodb::rest::ResponseCode::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON // args
    );
    return;
  }

  bool success = false;
  auto body = parseVPackBody(success);

  if (!success) {
    return; // parseVPackBody(...) calls generateError(...)
  }

  if (!body.isObject()) {
    generateError(arangodb::Result( // generate error
      TRI_ERROR_BAD_PARAMETER, // code
      "expecting body to be of the form { name: <string>, type: <string>[, properties: <object|string>[, features: <string-array>]] }"
    ));

    return;
  }

  irs::string_ref name;
  auto nameSlice = body.get(StaticStrings::AnalyzerNameField);

  if (!nameSlice.isString()) {
    generateError(arangodb::Result( // generate error
      TRI_ERROR_BAD_PARAMETER, // code
      "invalid 'name', expecting body to be of the form { name: <string>, type: <string>[, properties: <object|string>[, features: <string-array>]] }"
    ));

    return;
  }

  name = getStringRef(nameSlice);

  if (!TRI_vocbase_t::IsAllowedName(false, velocypack::StringRef(name.c_str(), name.size()))) {
    generateError(arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      "invalid characters in analyzer name '" + static_cast<std::string>(name) + "'"
    ));

    return;
  }

  std::string nameBuf;
  auto* sysDatabase = 
    arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;
  if (sysVocbase) {
    nameBuf = IResearchAnalyzerFeature::normalize(name, _vocbase, *sysVocbase); // normalize
    name = nameBuf;
  };

  irs::string_ref type;
  auto typeSlice = body.get(StaticStrings::AnalyzerTypeField);

  if (!typeSlice.isString()) {
    generateError(arangodb::Result( // generate error
      TRI_ERROR_BAD_PARAMETER, // code
      "invalid 'type', expecting body to be of the form { name: <string>, type: <string>[, properties: <object|string>[, features: <string-array>]] }"
    ));

    return;
  }

  type = getStringRef(typeSlice);
  
  std::shared_ptr<VPackBuilder> propertiesFromStringBuilder;
  auto properties = body.get(StaticStrings::AnalyzerPropertiesField);
  if(properties.isString()) { // string still could be parsed to an object
    auto string_ref = getStringRef(properties);
    propertiesFromStringBuilder = arangodb::velocypack::Parser::fromJson(string_ref);
    properties = propertiesFromStringBuilder->slice();
  }
  
  if (!properties.isNone() && !properties.isObject()) { // optional parameter
    generateError(arangodb::Result( // generate error
      TRI_ERROR_BAD_PARAMETER, // code
      "invalid 'properties', expecting body to be of the form { name: <string>, type: <string>[, properties: <object>[, features: <string-array>]] }"
    ));
    return;
  }

  irs::flags features;

  if (body.hasKey(StaticStrings::AnalyzerFeaturesField)) { // optional parameter
    auto featuresSlice = body.get(StaticStrings::AnalyzerFeaturesField);

    if (featuresSlice.isArray()) {
      for (arangodb::velocypack::ArrayIterator itr(featuresSlice);
           itr.valid();
           ++itr
          ) {
        auto value = *itr;

        if (!value.isString()) {
          generateError(arangodb::Result( // generate error
            TRI_ERROR_BAD_PARAMETER, // code
            "invalid value in 'features', expecting body to be of the form { name: <string>, type: <string>[, properties: <object>[, features: <string-array>]] }"
          ));

          return;
        }

        auto* feature = irs::attribute::type_id::get(getStringRef(value));

        if (!feature) {
          generateError(arangodb::Result( // generate error
            TRI_ERROR_BAD_PARAMETER, // code
            "unknown value in 'features', expecting body to be of the form { name: <string>, type: <string>[, properties: <object>[, features: <string-array>]] }"
          ));

          return;
        }

        features.add(*feature);
      }
    } else {
      generateError(arangodb::Result( // generate error
        TRI_ERROR_BAD_PARAMETER, // code
        "invalid 'features', expecting body to be of the form { name: <string>, type: <string>[, properties: <object>[, features: <string-array>]] }"
      ));

      return;
    }
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!IResearchAnalyzerFeature::canUse(name, auth::Level::RW)) {
    generateError( // generate error
      arangodb::rest::ResponseCode::FORBIDDEN, // HTTP code
      TRI_ERROR_FORBIDDEN, // code
      std::string("insufficient rights while creating analyzer: ") + body.toString() // message
    );
    return;
  }

  IResearchAnalyzerFeature::EmplaceResult result;
  auto res = analyzers.emplace(result, name, type, properties, features);

  if (!res.ok()) {
    generateError(res);

    return;
  }

  if (!result.first) {
    generateError( // generate error
      arangodb::rest::ResponseCode::BAD, // HTTP code
      TRI_errno(), // code
      std::string("failure while creating analyzer: ") + body.toString() // message
    );
    return;
  }

  auto pool = result.first;
  arangodb::velocypack::Builder builder;

  pool->toVelocyPack(builder);

  generateResult( // generate result
    result.second // new analyzer v.s. existing analyzer
    ? arangodb::rest::ResponseCode::CREATED : arangodb::rest::ResponseCode::OK,
    builder.slice() // analyzer definition
  );
}

arangodb::RestStatus RestAnalyzerHandler::execute() {
  if (!_request) {
    generateError( // generate error
      arangodb::rest::ResponseCode::METHOD_NOT_ALLOWED, // HTTP code
      TRI_ERROR_HTTP_BAD_PARAMETER // error code
    );
    return arangodb::RestStatus::DONE;
  }

  auto* analyzers = 
    arangodb::application_features::ApplicationServer::lookupFeature<IResearchAnalyzerFeature>();
  if (!analyzers) {
    generateError( 
      arangodb::rest::ResponseCode::SERVER_ERROR, // HTTP code
      TRI_ERROR_INTERNAL, // code
      std::string("Failure to find feature '").append(IResearchAnalyzerFeature::name())
        .append("' while executing REST request for: ").append(ANALYZER_PATH));
    return arangodb::RestStatus::DONE;
  }

  auto& suffixes = _request->suffixes();

  switch (_request->requestType()) {
   case arangodb::rest::RequestType::DELETE_REQ:
    if (suffixes.size() == 1) {
      auto name = arangodb::basics::StringUtils::urlDecode(suffixes[0]);
      auto force = _request->parsedValue("force", false);
      removeAnalyzer(*analyzers, name, force);
      return arangodb::RestStatus::DONE;
    }
    generateError( // generate error
      arangodb::rest::ResponseCode::BAD, // HTTP code
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("expecting DELETE ").append(ANALYZER_PATH).append("/<analyzer-name>[?force=true]") 
    );
    return arangodb::RestStatus::DONE;

   case arangodb::rest::RequestType::GET:
    if (suffixes.empty()) {
      getAnalyzers(*analyzers);
      return arangodb::RestStatus::DONE;
    }
    if (suffixes.size() == 1) {
      auto name = arangodb::basics::StringUtils::urlDecode(suffixes[0]);
      getAnalyzer(*analyzers, name);
      return arangodb::RestStatus::DONE;
    }
    generateError(arangodb::Result( // generate error
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("expecting GET ").append(ANALYZER_PATH).append("[/<analyzer-name>]") // mesage
    ));
    return arangodb::RestStatus::DONE;

   case arangodb::rest::RequestType::POST:
    if (suffixes.empty()) {
      createAnalyzer(*analyzers);
      return arangodb::RestStatus::DONE;
    }
    generateError(arangodb::Result( // generate error
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("expecting POST ").append(ANALYZER_PATH) // mesage
    ));
    return arangodb::RestStatus::DONE;

   default:
    generateError(arangodb::Result( // generate error
      TRI_ERROR_HTTP_METHOD_NOT_ALLOWED // error code
    ));
  }
  return arangodb::RestStatus::DONE;
}

void RestAnalyzerHandler::getAnalyzer( // get analyzer
    IResearchAnalyzerFeature& analyzers, // analyzer feature
    std::string const& name // analyzer name 
) {
  TRI_ASSERT(_request); // ensured by execute()

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................
  auto* sysDatabase = 
    arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;
  auto normalizedName = sysVocbase ?
    IResearchAnalyzerFeature::normalize(name, _vocbase, *sysVocbase) : name;

  // need to check if analyzer is from current database or from system database
  const auto analyzerVocbase = IResearchAnalyzerFeature::extractVocbaseName(normalizedName);
  if (!analyzerVocbase.empty() && analyzerVocbase != _vocbase.name() && 
    analyzerVocbase != arangodb::StaticStrings::SystemDatabase) {
    std::string errorMessage("Analyzer '");
    errorMessage.append(normalizedName)
      .append("' is not accessible. Only analyzers from current database ('")
      .append(_vocbase.name())
      .append("')");
    if (_vocbase.name() != arangodb::StaticStrings::SystemDatabase) {
      errorMessage.append(" or system database");
    }
    errorMessage.append(" are available");
    generateError(arangodb::Result( // generate error
      TRI_ERROR_FORBIDDEN, // code
      errorMessage
    ));
    return;
  }

  if (!IResearchAnalyzerFeature::canUse(normalizedName, auth::Level::RO)) {
    generateError(arangodb::Result( // generate error
      TRI_ERROR_FORBIDDEN, // code
      std::string("insufficient rights while getting analyzer: ").append(normalizedName) 
    ));
    return;
  }

  auto pool = analyzers.get(normalizedName);
  if (!pool) {
    generateError(arangodb::Result( // generate error
      TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, // code
      std::string("unable to find analyzer: ").append(normalizedName)
    ));
    return;
  }

  arangodb::velocypack::Builder builder;
  pool->toVelocyPack(builder);

  // generate result + 'error' field + 'code' field
  // 2nd param must be Builder and not Slice
  generateOk(arangodb::rest::ResponseCode::OK, builder);
}

void RestAnalyzerHandler::getAnalyzers( // get all analyzers
    IResearchAnalyzerFeature& analyzers // analyzer feature
) {
  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  typedef arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr AnalyzerPoolPtr;
  arangodb::velocypack::Builder builder;
  auto visitor = [&builder](AnalyzerPoolPtr const& analyzer)->bool {
    if (!analyzer) {
      return true; // continue with next analyzer
    }

    analyzer->toVelocyPack(builder);

    return true; // continue with next analyzer
  };

  builder.openArray();
  analyzers.visit(visitor, nullptr); // include static analyzers

  if (IResearchAnalyzerFeature::canUse(_vocbase, auth::Level::RO)) {
    analyzers.visit(visitor, &_vocbase);
  }

  auto* sysDBFeature = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::SystemDatabaseFeature // feature type
  >();

  // include analyzers from the system vocbase if possible
  if (sysDBFeature) {
    auto sysVocbase = sysDBFeature->use();

    if (sysVocbase // have system vocbase
        && sysVocbase->name() != _vocbase.name() // not same vocbase as current
        && IResearchAnalyzerFeature::canUse(*sysVocbase, auth::Level::RO)) {
      analyzers.visit(visitor, sysVocbase.get());
    }
  }

  builder.close();

  // generate result (wrapped inside 'result') + 'error' field + 'code' field
  // 2nd param must be Slice and not Builder
  generateOk(arangodb::rest::ResponseCode::OK, builder.slice());
}

void RestAnalyzerHandler::removeAnalyzer( 
    IResearchAnalyzerFeature& analyzers, 
    std::string const& name, // analyzer name 
    bool force
) {
  if (!TRI_vocbase_t::IsAllowedName(false, velocypack::StringRef(name.c_str(), name.size()))) {
    generateError(arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("Invalid characters in analyzer name '").append(name)
        .append("'. Analyzer name should be specified without database prefix.")
    ));
    return;
  }

  auto* sysDatabase = 
    arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;
  auto normalizedName = sysVocbase ?
    IResearchAnalyzerFeature::normalize(name, _vocbase, *sysVocbase) : name;
  if (!IResearchAnalyzerFeature::canUse(normalizedName, auth::Level::RW)) {
    generateError(arangodb::Result( // generate error
      TRI_ERROR_FORBIDDEN, // code
      std::string("insufficient rights while removing analyzer: ").append(normalizedName) 
    ));
    return;
  }
  auto res = analyzers.remove(normalizedName, force);

  if (!res.ok()) {
    generateError(res);
    return;
  }
  arangodb::velocypack::Builder builder;
  builder.openObject();
  builder.add(StaticStrings::AnalyzerNameField, arangodb::velocypack::Value(normalizedName));
  builder.close();

  // generate result + 'error' field + 'code' field
  // 2nd param must be Builder and not Slice
  generateOk(arangodb::rest::ResponseCode::OK, builder);
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
