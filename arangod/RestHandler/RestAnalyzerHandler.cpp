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

RestAnalyzerHandler::RestAnalyzerHandler(
    arangodb::GeneralRequest* request,
    arangodb::GeneralResponse* response
): RestVocbaseBaseHandler(request, response) {
}

void RestAnalyzerHandler::createAnalyzer( // create
    IResearchAnalyzerFeature& analyzers
) {
  TRI_ASSERT(_request); // ensured by execute()

  if (_request->payload().isEmptyObject()) {
    generateError(
      arangodb::rest::ResponseCode::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON
    );
    return;
  }

  bool success = false;
  auto body = parseVPackBody(success);

  if (!success) {
    return; // parseVPackBody(...) calls generateError(...)
  }

  if (!body.isObject()) {
    generateError(arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      "expecting body to be of the form { name: <string>, type: <string>[, properties: <object|string>[, features: <string-array>]] }"
    ));

    return;
  }

  irs::string_ref name;
  auto nameSlice = body.get(StaticStrings::AnalyzerNameField);

  if (!nameSlice.isString()) {
    generateError(arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      "invalid 'name', expecting body to be of the form { name: <string>, type: <string>[, properties: <object|string>[, features: <string-array>]] }"
    ));

    return;
  }
  auto splittedAnalyzerName = IResearchAnalyzerFeature::splitAnalyzerName(getStringRef(nameSlice));
  if (!IResearchAnalyzerFeature::analyzerReachableFromDb(splittedAnalyzerName.first, 
                                                           _vocbase.name())) { 
      generateError(arangodb::Result(
        TRI_ERROR_FORBIDDEN,
        "Database in analyzer name does not match current database"));
      return;
  }

  name = splittedAnalyzerName.second;
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
    generateError(arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
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
    generateError(arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
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
          generateError(arangodb::Result(
            TRI_ERROR_BAD_PARAMETER,
            "invalid value in 'features', expecting body to be of the form { name: <string>, type: <string>[, properties: <object>[, features: <string-array>]] }"
          ));

          return;
        }

        auto* feature = irs::attribute::type_id::get(getStringRef(value));

        if (!feature) {
          generateError(arangodb::Result(
            TRI_ERROR_BAD_PARAMETER,
            "unknown value in 'features', expecting body to be of the form { name: <string>, type: <string>[, properties: <object>[, features: <string-array>]] }"
          ));

          return;
        }

        features.add(*feature);
      }
    } else {
      generateError(arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        "invalid 'features', expecting body to be of the form { name: <string>, type: <string>[, properties: <object>[, features: <string-array>]] }"
      ));

      return;
    }
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!IResearchAnalyzerFeature::canUse(name, auth::Level::RW)) {
    generateError(
      arangodb::rest::ResponseCode::FORBIDDEN,
      TRI_ERROR_FORBIDDEN,
      std::string("insufficient rights while creating analyzer: ") + body.toString()
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
    generateError(
      arangodb::rest::ResponseCode::BAD,
      TRI_errno(),
      std::string("failure while creating analyzer: ") + body.toString()
    );
    return;
  }

  auto pool = result.first;
  arangodb::velocypack::Builder builder;

  pool->toVelocyPack(builder);

  generateResult(
    result.second // new analyzer v.s. existing analyzer
    ? arangodb::rest::ResponseCode::CREATED : arangodb::rest::ResponseCode::OK,
    builder.slice() // analyzer definition
  );
}

arangodb::RestStatus RestAnalyzerHandler::execute() {
  if (!_request) {
    generateError(
      arangodb::rest::ResponseCode::METHOD_NOT_ALLOWED,
      TRI_ERROR_HTTP_BAD_PARAMETER
    );
    return arangodb::RestStatus::DONE;
  }

  auto* analyzers = 
    arangodb::application_features::ApplicationServer::getFeature<IResearchAnalyzerFeature>();

  auto& suffixes = _request->suffixes();

  switch (_request->requestType()) {
   case arangodb::rest::RequestType::DELETE_REQ:
    if (suffixes.size() == 1) {
      auto name = arangodb::basics::StringUtils::urlDecode(suffixes[0]);
      auto force = _request->parsedValue("force", false);
      removeAnalyzer(*analyzers, name, force);
      return arangodb::RestStatus::DONE;
    }
    generateError(
      arangodb::rest::ResponseCode::BAD,
      TRI_ERROR_BAD_PARAMETER,
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
    generateError(arangodb::Result( 
      TRI_ERROR_BAD_PARAMETER,
      std::string("expecting GET ").append(ANALYZER_PATH).append("[/<analyzer-name>]")
    ));
    return arangodb::RestStatus::DONE;

   case arangodb::rest::RequestType::POST:
    if (suffixes.empty()) {
      createAnalyzer(*analyzers);
      return arangodb::RestStatus::DONE;
    }
    generateError(arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("expecting POST ").append(ANALYZER_PATH)
    ));
    return arangodb::RestStatus::DONE;

   default:
    generateError(arangodb::Result(
      TRI_ERROR_HTTP_METHOD_NOT_ALLOWED
    ));
  }
  return arangodb::RestStatus::DONE;
}

void RestAnalyzerHandler::getAnalyzer( 
    IResearchAnalyzerFeature& analyzers, 
    std::string const& requestedName  
) {
  auto* sysDatabase = 
    arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;
  auto normalizedName = sysVocbase ?
    IResearchAnalyzerFeature::normalize(requestedName, _vocbase, *sysVocbase) : requestedName;

  // need to check if analyzer is from current database or from system database
  const auto analyzerVocbase = IResearchAnalyzerFeature::extractVocbaseName(normalizedName);
  if (!IResearchAnalyzerFeature::analyzerReachableFromDb(analyzerVocbase, _vocbase.name(), true)) {
    std::string errorMessage("Analyzer '");
    errorMessage.append(normalizedName)
      .append("' is not accessible. Only analyzers from current database ('")
      .append(_vocbase.name())
      .append("')");
    if (_vocbase.name() != arangodb::StaticStrings::SystemDatabase) {
      errorMessage.append(" or system database");
    }
    errorMessage.append(" are available");
    generateError(arangodb::Result( 
      TRI_ERROR_FORBIDDEN, 
      errorMessage
    ));
    return;
  }

  if (!IResearchAnalyzerFeature::canUse(normalizedName, auth::Level::RO)) {
    generateError(arangodb::Result( 
      TRI_ERROR_FORBIDDEN, 
      std::string("insufficient rights while getting analyzer: ").append(normalizedName) 
    ));
    return;
  }

  auto pool = analyzers.get(normalizedName);
  if (!pool) {
    generateError(arangodb::Result(
      TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
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
    std::string const& requestedName, 
    bool force
) {
  auto splittedAnalyzerName = IResearchAnalyzerFeature::splitAnalyzerName(requestedName);
  auto name = splittedAnalyzerName.second;

  if (!TRI_vocbase_t::IsAllowedName(false, velocypack::StringRef(name.c_str(), name.size()))) {
    generateError(arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("Invalid characters in analyzer name '").append(name)
        .append("'.")
    ));
    return;
  }

  if (!IResearchAnalyzerFeature::analyzerReachableFromDb(splittedAnalyzerName.first,
                                                          _vocbase.name())) { 
    generateError(arangodb::Result(
      TRI_ERROR_FORBIDDEN,
      "Database in analyzer name does not match current database"));
    return;
  }

  auto* sysDatabase = 
    arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;
  auto normalizedName = sysVocbase ?
    IResearchAnalyzerFeature::normalize(name, _vocbase, *sysVocbase) : std::string(name);
  if (!IResearchAnalyzerFeature::canUse(normalizedName, auth::Level::RW)) {
    generateError(arangodb::Result( 
      TRI_ERROR_FORBIDDEN, 
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
