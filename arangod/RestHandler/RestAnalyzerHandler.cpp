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
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "utils/string.hpp"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

namespace {

std::string ANALYZER_FEAURES_FIELD("features");
std::string ANALYZER_NAME_FIELD("name");
std::string ANALYZER_PROPERTIES_FIELD("properties");
std::string ANALYZER_TYPE_FIELD("type");

void serializeAnalyzer( // serialize analyzer
    arangodb::velocypack::Builder& builder, // builder
    arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool const& analyzer // analyzer
) {
  builder.openObject();
    arangodb::iresearch::addStringRef( // add value
      builder, ANALYZER_NAME_FIELD, analyzer.name() // args
    );
    arangodb::iresearch::addStringRef( // add value
      builder, ANALYZER_TYPE_FIELD, analyzer.type() // args
    );

    try {
      auto& properties = analyzer.properties();
      auto json = arangodb::velocypack::Parser::fromJson( // json properties object
        properties.c_str(), properties.size() // args
      );
      TRI_ASSERT(json); // exception expected on error
      auto slice = json->slice();

      if (slice.isObject()) {
        builder.add(ANALYZER_PROPERTIES_FIELD, slice); // add as json
      } else {
        arangodb::iresearch::addStringRef( // add value verbatim
          builder, ANALYZER_PROPERTIES_FIELD, properties // args
        );
      }
    } catch(...) { // parser may throw exceptions for valid properties
      arangodb::iresearch::addStringRef( // add value verbatim
        builder, ANALYZER_PROPERTIES_FIELD, analyzer.properties()
      );
    }

    builder.add( // add features
      ANALYZER_FEAURES_FIELD, // key
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Array) // value
    );

      for (auto& feature: analyzer.features()) {
        TRI_ASSERT(feature); // has to be non-nullptr
        arangodb::iresearch::addStringRef(builder, feature->name());
      }

    builder.close();
  builder.close();
}

}

namespace arangodb {
namespace iresearch {

RestAnalyzerHandler::RestAnalyzerHandler( // constructor
    arangodb::GeneralRequest* request, // request
    arangodb::GeneralResponse* response // response
): RestVocbaseBaseHandler(request, response) {
}

void RestAnalyzerHandler::createAnalyzer( // create
    IResearchAnalyzerFeature& analyzers, // analyzer feature
    TRI_vocbase_t const* sysVocbase // system vocbase
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
  std::string nameBuf;
  auto nameSlice = body.get(ANALYZER_NAME_FIELD);

  if (nameSlice.isString()) {
    name = getStringRef(nameSlice);
  } else if (!nameSlice.isNull()) {
    generateError(arangodb::Result( // generate error
      TRI_ERROR_BAD_PARAMETER, // code
      "invalid 'name', expecting body to be of the form { name: <string>, type: <string>[, properties: <object|string>[, features: <string-array>]] }"
    ));

    return;
  }

  if (sysVocbase) {
    nameBuf = IResearchAnalyzerFeature::normalize(name, _vocbase, *sysVocbase); // normalize
    name = nameBuf;
  };

  irs::string_ref type;
  auto typeSlice = body.get(ANALYZER_TYPE_FIELD);

  if (typeSlice.isString()) {
    type = getStringRef(typeSlice);
  } else if (!typeSlice.isNull()) {
    generateError(arangodb::Result( // generate error
      TRI_ERROR_BAD_PARAMETER, // code
      "invalid 'type', expecting body to be of the form { name: <string>, type: <string>[, properties: <object|string>[, features: <string-array>]] }"
    ));

    return;
  }

  irs::string_ref properties;
  std::string propertiesBuf;

  if (body.hasKey(ANALYZER_PROPERTIES_FIELD)) { // optional parameter
    auto propertiesSlice = body.get(ANALYZER_PROPERTIES_FIELD);

    if (propertiesSlice.isObject()) {
      propertiesBuf = propertiesSlice.toString();
      properties = propertiesBuf;
    } else if (propertiesSlice.isString()) {
      properties = getStringRef(propertiesSlice);
    } else if (!propertiesSlice.isNull()) {
      generateError(arangodb::Result( // generate error
        TRI_ERROR_BAD_PARAMETER, // code
        "invalid 'properties', expecting body to be of the form { name: <string>, type: <string>[, properties: <object>[, features: <string-array>]] }"
      ));

      return;
    }
  }

  irs::flags features;

  if (body.hasKey(ANALYZER_FEAURES_FIELD)) { // optional parameter
    auto featuresSlice = body.get(ANALYZER_FEAURES_FIELD);

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

  serializeAnalyzer(builder, *pool);
  generateResult( // generate result
    result.second // new analyzer v.s. existing analyzer
    ? arangodb::rest::ResponseCode::CREATED : arangodb::rest::ResponseCode::OK,
    builder.slice() // analyzer definition
  );
}

arangodb::RestStatus RestAnalyzerHandler::execute() {
  auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    IResearchAnalyzerFeature // feature type
  >();

  if (!analyzers) {
    generateError( // generate error
      arangodb::rest::ResponseCode::BAD, // HTTP code
      TRI_ERROR_INTERNAL, // code
      std::string("failure to find feature '") + IResearchAnalyzerFeature::name() + "' while executing REST request for: " + ANALYZER_PATH
    );

    return arangodb::RestStatus::DONE;
  }

  auto* sysDatabase = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::SystemDatabaseFeature // featue type
  >();
  auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;

  if (!_request) {
    generateError( // generate error
      arangodb::rest::ResponseCode::METHOD_NOT_ALLOWED, // HTTP code
      TRI_ERROR_HTTP_BAD_PARAMETER // error code
    );

    return arangodb::RestStatus::DONE;
  }

  auto& suffixes = _request->suffixes();

  switch (_request->requestType()) {
   case arangodb::rest::RequestType::DELETE_REQ:
    if (suffixes.size() == 1) {
      auto name = arangodb::basics::StringUtils::urlDecode(suffixes[0]);

      if (!sysVocbase) {
        removeAnalyzer(*analyzers, name); // verbatim (assume already normalized)

        return arangodb::RestStatus::DONE;
      }

      removeAnalyzer( // remove
        *analyzers, // feature
        IResearchAnalyzerFeature::normalize(name, _vocbase, *sysVocbase) // normalize
      );

      return arangodb::RestStatus::DONE;
    }

    generateError( // generate error
      arangodb::rest::ResponseCode::BAD, // HTTP code
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("expecting DELETE ") + ANALYZER_PATH + "/<analyzer-name>[?force=true]" // mesage
    );

    return arangodb::RestStatus::DONE;
   case arangodb::rest::RequestType::GET:
    if (suffixes.empty()) {
      getAnalyzers(*analyzers);

      return arangodb::RestStatus::DONE;
    }

    if (suffixes.size() == 1) {
      auto name = arangodb::basics::StringUtils::urlDecode(suffixes[0]);

      if (!sysVocbase) {
        getAnalyzer(*analyzers, name);

        return arangodb::RestStatus::DONE;
      }

      getAnalyzer( // get
        *analyzers, // feature
        IResearchAnalyzerFeature::normalize(name, _vocbase, *sysVocbase) // normalize
      );

      return arangodb::RestStatus::DONE;
    }

    generateError(arangodb::Result( // generate error
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("expecting GET ") + ANALYZER_PATH + "[/<analyzer-name>]" // mesage
    ));

    return arangodb::RestStatus::DONE;
   case arangodb::rest::RequestType::POST:
    if (suffixes.empty()) {
      createAnalyzer(*analyzers, sysVocbase.get());

      return arangodb::RestStatus::DONE;
    }

    generateError(arangodb::Result( // generate error
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("expecting POST ") + ANALYZER_PATH // mesage
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
    std::string const& name // analyzer name (normalized)
) {
  TRI_ASSERT(_request); // ensured by execute()

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!IResearchAnalyzerFeature::canUse(name, auth::Level::RO)) {
    generateError(arangodb::Result( // generate error
      TRI_ERROR_FORBIDDEN, // code
      std::string("insufficient rights while getting analyzer: ") + name // message
    ));

    return;
  }

  auto pool = analyzers.get(name);

  if (!pool) {
    generateError(arangodb::Result( // generate error
      TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, // code
      std::string("unable to find analyzer: ") + name // mesage
    ));

    return;
  }

  arangodb::velocypack::Builder builder;

  serializeAnalyzer(builder, *pool);

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

    serializeAnalyzer(builder, *analyzer);

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

void RestAnalyzerHandler::removeAnalyzer( // remove
    IResearchAnalyzerFeature& analyzers, // analyzer feature
    std::string const& name // analyzer name (normalized)
) {
  TRI_ASSERT(_request); // ensured by execute()

  auto force = _request->parsedValue("force", false);

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!IResearchAnalyzerFeature::canUse(name, auth::Level::RW)) {
    generateError(arangodb::Result( // generate error
      TRI_ERROR_FORBIDDEN, // code
      std::string("insufficient rights while removing analyzer: ") + name // message
    ));

    return;
  }

  auto res = analyzers.remove(name, force);

  if (!res.ok()) {
    generateError(res);

    return;
  }

  arangodb::velocypack::Builder builder;

  builder.openObject();
  builder.add(ANALYZER_NAME_FIELD, arangodb::velocypack::Value(name));
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