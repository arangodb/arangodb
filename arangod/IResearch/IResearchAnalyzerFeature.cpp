////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

// otherwise define conflict between 3rdParty\date\include\date\date.h and
// 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
#include "date/date.h"
#endif

#include "velocypack/velocypack-aliases.h"

#include "analysis/analyzers.hpp"
#include "analysis/delimited_token_stream.hpp"
#include "analysis/ngram_token_stream.hpp"
#include "analysis/pipeline_token_stream.hpp"
#include "analysis/text_token_normalizing_stream.hpp"
#include "analysis/text_token_stemming_stream.hpp"
#include "analysis/text_token_stream.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"
#include "index/norm.hpp"
#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

#include <Containers/HashSet.h>
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationServerHelper.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/ExpressionContext.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/FunctionUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/V8FeaturePhase.h"
#include "IResearch/GeoAnalyzer.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchIdentityAnalyzer.h"
#include "IResearch/IResearchLink.h"
#include "IResearchAqlAnalyzer.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "RestHandler/RestVocbaseBaseHandler.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/StandaloneContext.h"
#include "Utilities/NameValidator.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VelocyPackHelper.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/vocbase.h"
#include "frozen/map.h"

namespace {

using namespace std::literals::string_literals;
using namespace arangodb;
using namespace arangodb::iresearch;
namespace StringUtils = arangodb::basics::StringUtils;

char constexpr ANALYZER_PREFIX_DELIM = ':';  // name prefix delimiter (2 chars)
size_t constexpr ANALYZER_PROPERTIES_SIZE_MAX = 1024 * 1024;  // arbitrary value
size_t constexpr DEFAULT_POOL_SIZE = 8;                       // arbitrary value
std::string const FEATURE_NAME("ArangoSearchAnalyzer");
static constexpr frozen::map<irs::string_ref, irs::string_ref, 13>
    STATIC_ANALYZERS_NAMES{{irs::type<IdentityAnalyzer>::name(),
                            irs::type<IdentityAnalyzer>::name()},
                           {"text_de", "de"},
                           {"text_en", "en"},
                           {"text_es", "es"},
                           {"text_fi", "fi"},
                           {"text_fr", "fr"},
                           {"text_it", "it"},
                           {"text_nl", "nl"},
                           {"text_no", "no"},
                           {"text_pt", "pt"},
                           {"text_ru", "ru"},
                           {"text_sv", "sv"},
                           {"text_zh", "zh"}};

REGISTER_ANALYZER_VPACK(IdentityAnalyzer, IdentityAnalyzer::make,
                        IdentityAnalyzer::normalize);
REGISTER_ANALYZER_JSON(IdentityAnalyzer, IdentityAnalyzer::make_json,
                       IdentityAnalyzer::normalize_json);
REGISTER_ANALYZER_VPACK(GeoJSONAnalyzer, GeoJSONAnalyzer::make,
                        GeoJSONAnalyzer::normalize);
REGISTER_ANALYZER_VPACK(GeoPointAnalyzer, GeoPointAnalyzer::make,
                        GeoPointAnalyzer::normalize);
REGISTER_ANALYZER_VPACK(AqlAnalyzer, AqlAnalyzer::make_vpack,
                        AqlAnalyzer::normalize_vpack);
REGISTER_ANALYZER_JSON(AqlAnalyzer, AqlAnalyzer::make_json,
                       AqlAnalyzer::normalize_json);

bool normalize(std::string& out, irs::string_ref const& type,
               VPackSlice const properties) {
  if (type.empty()) {
    // in ArangoSearch we don't allow to have analyzers with empty type string
    return false;
  }

  // for API consistency we only support analyzers configurable via jSON
  return irs::analysis::analyzers::normalize(
      out, type, irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(properties), false);
}

aql::AqlValue aqlFnTokens(aql::ExpressionContext* expressionContext,
                          aql::AstNode const&,
                          aql::VPackFunctionParameters const& args) {
  if (ADB_UNLIKELY(args.empty() || args.size() > 2)) {
    irs::string_ref const message =
        "invalid arguments count while computing result for function 'TOKENS'";
    LOG_TOPIC("740fd", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, message);
  }

  if (args.size() > 1 &&
      !args[1].isString()) {  // second arg must be analyzer name
    irs::string_ref const message =
        "invalid analyzer name argument type while computing result for "
        "function 'TOKENS',"
        " string expected";
    LOG_TOPIC("d0b60", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
  }

  AnalyzerPool::ptr pool;
  // identity now is default analyzer
  auto const name =
      args.size() > 1
          ? arangodb::iresearch::getStringRef(args[1].slice())
          : irs::string_ref(IResearchAnalyzerFeature::identity()->name());

  TRI_ASSERT(expressionContext);
  auto& trx = expressionContext->trx();
  auto& server = expressionContext->vocbase().server();
  if (args.size() > 1) {
    auto& analyzers = server.getFeature<IResearchAnalyzerFeature>();
    pool = analyzers.get(name, trx.vocbase(), trx.state()->analyzersRevision());
  } else {  // do not look for identity, we already have reference)
    pool = IResearchAnalyzerFeature::identity();
  }

  if (!pool) {
    auto const message = "failure to find arangosearch analyzer with name '"s +
                         static_cast<std::string>(name) +
                         "' while computing result for function 'TOKENS'";

    LOG_TOPIC("0d256", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
  }

  auto analyzer = pool->get();

  if (!analyzer) {
    auto const message = "failure to get arangosearch analyzer with name '"s +
                         static_cast<std::string>(name) +
                         "' while computing result for function 'TOKENS'";
    LOG_TOPIC("d7477", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
  }

  auto* token = irs::get<irs::term_attribute>(*analyzer);
  auto* vpack_token = irs::get<VPackTermAttribute>(*analyzer);

  if (ADB_UNLIKELY(!token && !vpack_token)) {
    auto const message =
        "failure to retrieve values from arangosearch analyzer name '"s +
        static_cast<std::string>(name) +
        "' while computing result for function 'TOKENS'";

    LOG_TOPIC("f46f2", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }

  std::unique_ptr<irs::numeric_token_stream> numeric_analyzer;
  const irs::term_attribute* numeric_token = nullptr;

  // to avoid copying Builder's default buffer when initializing AqlValue
  // create the buffer externally and pass ownership directly into AqlValue
  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.openArray();
  std::vector<VPackArrayIterator> arrayIteratorStack;

  auto processNumeric = [&numeric_analyzer, &numeric_token,
                         &builder](VPackSlice value) {
    if (value.isNumber()) {  // there are many "number" types. To adopt all
                             // current and future ones just deal with them all
                             // here in generic way
      if (!numeric_analyzer) {
        numeric_analyzer = std::make_unique<irs::numeric_token_stream>();
        numeric_token = irs::get<irs::term_attribute>(*numeric_analyzer);
        if (ADB_UNLIKELY(!numeric_token)) {
          auto const message =
              "failure to retrieve values from arangosearch numeric analyzer "
              "while computing result for function 'TOKENS'";
          LOG_TOPIC("7d5df", WARN, arangodb::iresearch::TOPIC) << message;
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
        }
      }
      // we read all numers as doubles because ArangoSearch indexes
      // all numbers as doubles, so do we there, as our goal is to
      // return same tokens as will be in index for this specific number
      numeric_analyzer->reset(value.getNumber<double>());
      while (numeric_analyzer->next()) {
        builder.add(
            arangodb::iresearch::toValuePair(basics::StringUtils::encodeBase64(
                irs::ref_cast<char>(numeric_token->value))));
      }
    } else {
      auto const message = "unexpected parameter type '"s + value.typeName() +
                           "' while computing result for function 'TOKENS'";
      LOG_TOPIC("45a2e", WARN, arangodb::iresearch::TOPIC) << message;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
    }
  };

  auto processBool = [&builder](VPackSlice value) {
    builder.add(arangodb::iresearch::toValuePair(
        basics::StringUtils::encodeBase64(irs::ref_cast<char>(
            irs::boolean_token_stream::value(value.getBoolean())))));
  };

  auto current = args[0].slice();
  do {
    // stack opening non-empty arrays
    while (current.isArray() && !current.isEmptyArray()) {
      arrayIteratorStack.emplace_back(current);
      builder.openArray();
      current = arrayIteratorStack.back().value();
    }
    // process current item
    switch (current.type()) {
      case VPackValueType::Object:
      case VPackValueType::String: {
        irs::string_ref value;
        AnalyzerValueType valueType{AnalyzerValueType::Undefined};
        if (current.isObject()) {
          valueType = AnalyzerValueType::Object;
          value = arangodb::iresearch::ref<char>(current);
        } else {
          valueType = AnalyzerValueType::String;
          value = arangodb::iresearch::getStringRef(current);
        }

        if (!pool->accepts(valueType)) {
          auto const message = "unexpected parameter type '"s +
                               current.typeName() +
                               "' while computing result for function 'TOKENS'";
          LOG_TOPIC("45a21", WARN, arangodb::iresearch::TOPIC) << message;
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
        }

        if (!analyzer->reset(value)) {
          auto const message = "failure to reset arangosearch analyzer: '"s +
                               static_cast<std::string>(name) +
                               "' while computing result for function 'TOKENS'";
          LOG_TOPIC("45a2d", WARN, arangodb::iresearch::TOPIC) << message;
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
        }
        switch (pool->returnType()) {
          case AnalyzerValueType::String:
            TRI_ASSERT(token);
            while (analyzer->next()) {
              builder.add(arangodb::iresearch::toValuePair(
                  irs::ref_cast<char>(token->value)));
            }
            break;
          case AnalyzerValueType::Number:
            TRI_ASSERT(vpack_token);
            while (analyzer->next()) {
              VPackArrayBuilder arr(&builder);
              TRI_ASSERT(vpack_token->value.isNumber());
              processNumeric(vpack_token->value);
            }
            break;
          case AnalyzerValueType::Bool:
            TRI_ASSERT(vpack_token);
            while (analyzer->next()) {
              VPackArrayBuilder arr(&builder);
              TRI_ASSERT(vpack_token->value.isBool());
              processBool(vpack_token->value);
            }
            break;
          default:
            TRI_ASSERT(false);
            LOG_TOPIC("1c838", WARN, arangodb::iresearch::TOPIC)
                << "Unexpected custom analyzer return type "
                << static_cast<std::underlying_type_t<AnalyzerValueType>>(
                       pool->returnType());
            break;
        }
      } break;
      case VPackValueType::Bool:
        processBool(current);
        break;
      case VPackValueType::Null:
        builder.add(
            arangodb::iresearch::toValuePair(basics::StringUtils::encodeBase64(
                irs::ref_cast<char>(irs::null_token_stream::value_null()))));
        break;
      case VPackValueType::Array:  // we get there only when empty array
                                   // encountered
        TRI_ASSERT(current.isEmptyArray());
        // empty array in = empty array out
        builder.openArray();
        builder.close();
        break;
      default:
        processNumeric(current);
    }
    // de-stack all closing arrays
    while (!arrayIteratorStack.empty()) {
      auto& currentArrayIterator = arrayIteratorStack.back();
      if (!currentArrayIterator.isLast()) {
        currentArrayIterator.next();
        current = currentArrayIterator.value();
        // next array for next item
        builder.close();
        builder.openArray();
        break;
      } else {
        arrayIteratorStack.pop_back();
        builder.close();
      }
    }
  } while (!arrayIteratorStack.empty());

  builder.close();

  return aql::AqlValue(std::move(buffer));
}

void addFunctions(aql::AqlFunctionFeature& functions) {
  arangodb::iresearch::addFunction(
      functions,
      aql::Function{
          "TOKENS",  // name
          ".|.",     // positional arguments (data[,analyzer])
          // deterministic (true == called during AST optimization and will be
          // used to calculate values for constant expressions)
          aql::Function::makeFlags(
              aql::Function::Flags::Deterministic,
              aql::Function::Flags::Cacheable,
              aql::Function::Flags::CanRunOnDBServerCluster,
              aql::Function::Flags::CanRunOnDBServerOneShard),
          &aqlFnTokens  // function implementation
      });
}

////////////////////////////////////////////////////////////////////////////////
/// @return pool will generate analyzers as per supplied parameters
////////////////////////////////////////////////////////////////////////////////
bool equalAnalyzer(AnalyzerPool const& pool, irs::string_ref const& type,
                   VPackSlice const properties, Features const& features) {
  std::string normalizedProperties;

  if (!::normalize(normalizedProperties, type, properties)) {
    // failed to normalize definition
    LOG_TOPIC("dfac1", WARN, arangodb::iresearch::TOPIC)
        << "failed to normalize properties for analyzer type '" << type
        << "' properties '" << properties.toString() << "'";
    return false;
  }

  // first check non-normalizeable portion of analyzer definition
  // to rule out need to check normalization of properties
  if (type != pool.type() || features != pool.features()) {
    return false;
  }

  // this check is not final as old-normalized definition may be present in
  // database!
  if (basics::VelocyPackHelper::equal(
          arangodb::iresearch::slice(normalizedProperties), pool.properties(),
          false)) {
    return true;
  }

  // Here could be analyzer definition with old-normalized properties (see Issue
  // #9652) To make sure properties really differ, let`s re-normalize and
  // re-check
  std::string reNormalizedProperties;
  if (ADB_UNLIKELY(!::normalize(reNormalizedProperties, pool.type(),
                                pool.properties()))) {
    // failed to re-normalize definition - strange. It was already normalized
    // once. Some bug in load/store?
    TRI_ASSERT(FALSE);
    LOG_TOPIC("a4073", WARN, arangodb::iresearch::TOPIC)
        << "failed to re-normalize properties for analyzer type '"
        << pool.type() << "' properties '" << pool.properties().toString()
        << "'";
    return false;
  }
  return basics::VelocyPackHelper::equal(
      arangodb::iresearch::slice(normalizedProperties),
      arangodb::iresearch::slice(reNormalizedProperties), false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read analyzers from vocbase
/// @return visitation completed fully
////////////////////////////////////////////////////////////////////////////////
Result visitAnalyzers(TRI_vocbase_t& vocbase,
                      std::function<Result(VPackSlice const&)> const& visitor) {
  static const auto resultVisitor =
      [](std::function<Result(VPackSlice const&)> const& visitor,
         TRI_vocbase_t const& vocbase, VPackSlice const& slice) -> Result {
    if (!slice.isArray()) {
      return {TRI_ERROR_INTERNAL,
              "failed to parse contents of collection '" +
                  arangodb::StaticStrings::AnalyzersCollection +
                  "' in database '" + vocbase.name() +
                  " while visiting analyzers"};
    }

    for (VPackArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const res = visitor(itr.value().resolveExternal());

      if (!res.ok()) {
        return res;
      }
    }

    return {};
  };

  static const auto queryString = aql::QueryString(
      "FOR d IN " + arangodb::StaticStrings::AnalyzersCollection + " RETURN d");

  if (ServerState::instance()->isDBServer()) {
    NetworkFeature const& feature =
        vocbase.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = feature.pool();

    if (!pool) {
      return {TRI_ERROR_SHUTTING_DOWN,
              "failure to find connection pool while visiting Analyzer "
              "collection '" +
                  arangodb::StaticStrings::AnalyzersCollection +
                  "' in vocbase '" + vocbase.name() + "'"};
    }

    auto& server = vocbase.server();

    std::vector<std::string> coords;
    if (server.hasFeature<ClusterFeature>()) {
      coords = server.getFeature<ClusterFeature>()
                   .clusterInfo()
                   .getCurrentCoordinators();
    }

    Result res;
    if (!coords.empty() &&
        !vocbase.isSystem() &&  // System database could be on other server so
                                // OneShard optimization will not work
        (vocbase.server().getFeature<ClusterFeature>().forceOneShard() ||
         vocbase.isOneShard())) {
      auto& clusterInfo = server.getFeature<ClusterFeature>().clusterInfo();
      auto collection = clusterInfo.getCollectionNT(
          vocbase.name(), arangodb::StaticStrings::AnalyzersCollection);
      if (!collection) {
        return {};  // treat missing collection as if there are no analyzers
      }

      auto shards = collection->shardIds();
      if (ADB_UNLIKELY(!shards)) {
        TRI_ASSERT(FALSE);
        return {};  // treat missing collection as if there are no analyzers
      }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      LOG_TOPIC("e07d4", TRACE, arangodb::iresearch::TOPIC)
          << "OneShard optimization found " << shards->size() << " shards "
          << " for server " << ServerState::instance()->getId();
      for (auto const& shard : *shards) {
        LOG_TOPIC("31300", TRACE, arangodb::iresearch::TOPIC)
            << "Shard '" << shard.first << "' on servers:";
        for (auto const& server : shard.second) {
          LOG_TOPIC("ead22", TRACE, arangodb::iresearch::TOPIC)
              << "Shard server '" << server << "'";
        }
      }
#endif

      if (shards->empty()) {
        return {};  // treat missing collection as if there are no analyzers
      }
      // If this is indeed OneShard this should be us!
      TRI_ASSERT(shards->begin()->second.front() ==
                 ServerState::instance()->getId());
      auto oneShardQueryString =
          aql::QueryString("FOR d IN "s + shards->begin()->first + " RETURN d");
      auto query =
          aql::Query::create(transaction::StandaloneContext::Create(vocbase),
                             std::move(oneShardQueryString), nullptr);

      auto result = query->executeSync();

      if (TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
          result.result.errorNumber()) {
        return {};  // treat missing collection as if there are no analyzers
      }

      if (result.result.fail()) {
        return result.result;
      }

      auto slice = result.data->slice();

      return resultVisitor(visitor, vocbase, slice);
    }

    network::RequestOptions reqOpts;
    reqOpts.database = vocbase.name();

    VPackBuffer<uint8_t> buffer;
    {
      VPackBuilder builder(buffer);
      builder.openObject();
      builder.add("query", VPackValue(queryString.string()));
      builder.close();
    }

    for (auto const& coord : coords) {
      auto f = network::sendRequestRetry(
          pool, "server:" + coord, fuerte::RestVerb::Post,
          RestVocbaseBaseHandler::CURSOR_PATH, buffer, reqOpts);

      auto& response = f.get();

      if (response.error == fuerte::Error::RequestTimeout) {
        // timeout, try another coordinator
        res = Result{network::fuerteToArangoErrorCode(response)};
        continue;
      } else if (response.fail()) {  // any other error abort
        return {network::fuerteToArangoErrorCode(response)};
      }

      if (response.statusCode() == fuerte::StatusNotFound) {
        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
      }

      VPackSlice answer = response.slice();
      if (!answer.isObject()) {
        return {TRI_ERROR_INTERNAL,
                "got misformed result while visiting Analyzer collection'" +
                    arangodb::StaticStrings::AnalyzersCollection +
                    "' in vocbase '" + vocbase.name() + "'"};
      }

      auto result = network::resultFromBody(answer, TRI_ERROR_NO_ERROR);
      if (result.fail()) {
        return result;
      }

      if (!answer.hasKey("result")) {
        return {TRI_ERROR_INTERNAL,
                "failed to parse result while visiting Analyzer collection '" +
                    arangodb::StaticStrings::AnalyzersCollection +
                    "' in vocbase '" + vocbase.name() + "'"};
      }

      res = resultVisitor(visitor, vocbase, answer.get("result"));

      break;
    }

    return res;
  }

  auto query = aql::Query::create(
      transaction::StandaloneContext::Create(vocbase), queryString, nullptr);

  auto result = query->executeSync();

  if (TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == result.result.errorNumber()) {
    return {};  // treat missing collection as if there are no analyzers
  }

  if (result.result.fail()) {
    return result.result;
  }

  auto slice = result.data->slice();

  return resultVisitor(visitor, vocbase, slice);
}

//////////////////////////////////////////////////////////////////
/// @brief parses common part of stored analyzers slice. This could be
///        loaded from collection or received via replication api
/// @param slice analyzer definition
/// @param name parsed name
/// @param type parsed type
/// @param features parsed features
/// @param properties slice for properties or null slice if absent
/// @return parse result
//////////////////////////////////////////////////////////////////
Result parseAnalyzerSlice(VPackSlice const& slice, irs::string_ref& name,
                          irs::string_ref& type, Features& features,
                          VPackSlice& properties) {
  TRI_ASSERT(slice.isObject());
  if (!slice.hasKey("name")  // no such field (required)
      || !(slice.get("name").isString() || slice.get("name").isNull())) {
    return {TRI_ERROR_BAD_PARAMETER,
            "failed to find a string value for analyzer 'name' "};
  }

  name = ::arangodb::iresearch::getStringRef(slice.get("name"));

  if (!slice.hasKey("type")  // no such field (required)
      || !(slice.get("type").isString() || slice.get("name").isNull())) {
    return {TRI_ERROR_BAD_PARAMETER,
            "failed to find a string value for analyzer 'type'"};
  }

  type = ::arangodb::iresearch::getStringRef(slice.get("type"));

  if (slice.hasKey("properties")) {
    auto subSlice = slice.get("properties");

    if (subSlice.isArray() || subSlice.isObject()) {
      properties = subSlice;  // format as a jSON encoded string
    } else {
      return {TRI_ERROR_BAD_PARAMETER,
              "failed to find a string value for analyzer 'properties'"};
    }
  }

  if (slice.hasKey("features")) {
    auto subSlice = slice.get("features");

    if (!subSlice.isArray()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "failed to find an array value for analyzer 'features'"};
    }

    for (VPackArrayIterator subItr(subSlice); subItr.valid(); ++subItr) {
      auto subEntry = *subItr;

      if (!subEntry.isString() && !subSlice.isNull()) {
        return {TRI_ERROR_BAD_PARAMETER,
                "failed to find a string value for an entry in analyzer "
                "'features'"};
      }
      const auto featureName = ::arangodb::iresearch::getStringRef(subEntry);
      if (!features.add(featureName)) {
        return {TRI_ERROR_BAD_PARAMETER,
                "failed to find feature '"s.append(std::string(featureName))
                    .append("'")};
      }
    }
  }
  return {};
}

inline std::string normalizedAnalyzerName(std::string database,
                                          irs::string_ref const& analyzer) {
  return database.append(2, ANALYZER_PREFIX_DELIM).append(analyzer);
}

bool analyzerInUse(application_features::ApplicationServer& server,
                   irs::string_ref const& dbName,
                   AnalyzerPool::ptr const& analyzerPtr) {
  TRI_ASSERT(analyzerPtr);

  if (analyzerPtr.use_count() > 1) {  // +1 for ref in '_analyzers'
    return true;
  }

  auto checkDatabase = [analyzer = analyzerPtr.get()](TRI_vocbase_t* vocbase) {
    if (!vocbase) {
      // no database
      return false;
    }

    bool found = false;

    auto visitor = [&found, analyzer](
                       std::shared_ptr<LogicalCollection> const& collection) {
      if (!collection) {
        return;
      }

      for (auto const& index : collection->getIndexes()) {
        if (!index ||
            arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
          continue;  // not an IResearchLink
        }

        // TODO FIXME find a better way to retrieve an iResearch Link
        // cannot use static_cast/reinterpret_cast since Index is not related to
        // IResearchLink
        auto link = std::dynamic_pointer_cast<IResearchLink>(index);

        if (!link) {
          continue;
        }

        if (nullptr != link->findAnalyzer(*analyzer)) {
          // found referenced analyzer
          found = true;
          return;
        }
      }
    };

    methods::Collections::enumerate(vocbase, visitor);

    return found;
  };

  TRI_vocbase_t* vocbase{};

  // check analyzer database

  if (server.hasFeature<DatabaseFeature>()) {
    vocbase = server.getFeature<DatabaseFeature>().lookupDatabase(
        static_cast<std::string>(dbName));  // FIXME: remove cast after C++20

    if (checkDatabase(vocbase)) {
      return true;
    }
  }

  // check system database if necessary
  if (server.hasFeature<SystemDatabaseFeature>()) {
    auto sysVocbase = server.getFeature<SystemDatabaseFeature>().use();

    if (sysVocbase.get() != vocbase && checkDatabase(sysVocbase.get())) {
      return true;
    }
  }

  return false;
}

AnalyzerModificationTransaction::Ptr createAnalyzerModificationTransaction(
    application_features::ApplicationServer& server,
    irs::string_ref const& vocbase) {
  if (ServerState::instance()->isCoordinator() && !vocbase.empty()) {
    TRI_ASSERT(server.hasFeature<ClusterFeature>());
    auto& engine = server.getFeature<ClusterFeature>().clusterInfo();
    return std::make_unique<AnalyzerModificationTransaction>(
        static_cast<std::string>(vocbase), &engine, false);
  }
  return nullptr;
}

// Auto-repair of dangling AnalyzersRevisions
void queueGarbageCollection(std::mutex& mutex, Scheduler::WorkHandle& workItem,
                            std::function<void(bool)>& gcfunc) {
  std::lock_guard<std::mutex> guard(mutex);
  workItem = SchedulerFeature::SCHEDULER->queueDelayed(
      RequestLane::INTERNAL_LOW, std::chrono::seconds(5), gcfunc);
}

// first - input type,
// second - output type
std::tuple<AnalyzerValueType, AnalyzerValueType, AnalyzerPool::StoreFunc>
getAnalyzerMeta(irs::analysis::analyzer const* analyzer) noexcept {
  TRI_ASSERT(analyzer);
  auto const type = analyzer->type();
  if (type == irs::type<GeoJSONAnalyzer>::id()) {
    return {AnalyzerValueType::Object | AnalyzerValueType::Array,
            AnalyzerValueType::String, &GeoJSONAnalyzer::store};
  } else if (type == irs::type<GeoPointAnalyzer>::id()) {
    return {AnalyzerValueType::Object | AnalyzerValueType::Array,
            AnalyzerValueType::String, &GeoPointAnalyzer::store};
  }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  if ("iresearch-vpack-analyzer" == type().name()) {
    return {AnalyzerValueType::Array | AnalyzerValueType::Object,
            AnalyzerValueType::String, nullptr};
  }
#endif
  auto const* value_type = irs::get<AnalyzerValueTypeAttribute>(*analyzer);
  if (value_type) {
    return {AnalyzerValueType::String, value_type->value, nullptr};
  }
  return {AnalyzerValueType::String, AnalyzerValueType::String, nullptr};
}

}  // namespace

namespace arangodb {
namespace iresearch {

void AnalyzerPool::toVelocyPack(VPackBuilder& builder,
                                irs::string_ref const& name) {
  TRI_ASSERT(builder.isOpenObject());
  addStringRef(builder, StaticStrings::AnalyzerNameField, name);
  addStringRef(builder, StaticStrings::AnalyzerTypeField, type());
  builder.add(StaticStrings::AnalyzerPropertiesField, properties());

  // add features
  VPackArrayBuilder featuresScope(&builder,
                                  StaticStrings::AnalyzerFeaturesField);

  features().visit(
      [&builder](std::string_view feature) { addStringRef(builder, feature); });
}

void AnalyzerPool::toVelocyPack(VPackBuilder& builder,
                                TRI_vocbase_t const* vocbase /*= nullptr*/) {
  irs::string_ref name = this->name();
  if (vocbase) {
    auto const split = IResearchAnalyzerFeature::splitAnalyzerName(name);
    if (!split.first.null()) {
      if (split.first == vocbase->name()) {
        name = split.second;
      } else {
        name =
            irs::string_ref(split.second.c_str() - 2, split.second.size() + 2);
      }
    }
  }

  VPackObjectBuilder rootScope(&builder);
  toVelocyPack(builder, name);
}

void AnalyzerPool::toVelocyPack(VPackBuilder& builder,
                                bool forPersistence /*= false*/) {
  irs::string_ref name = this->name();

  VPackObjectBuilder rootScope(&builder);

  if (forPersistence) {
    name = IResearchAnalyzerFeature::splitAnalyzerName(name).second;

    // ensure names are unique
    addStringRef(builder, arangodb::StaticStrings::KeyString, name);

    // only persistence needs revision to be stored
    // link definitions live always without revisions
    // analyzer definitions are stored in link itself
    builder.add(arangodb::StaticStrings::AnalyzersRevision,
                VPackValue(static_cast<uint64_t>(_revision)));
  }

  toVelocyPack(builder, name);
}

/*static*/ AnalyzerPool::Builder::ptr AnalyzerPool::Builder::make(
    irs::string_ref const& type, VPackSlice properties) {
  if (type.empty()) {
    // in ArangoSearch we don't allow to have analyzers with empty type string
    return nullptr;
  }

  // for API consistency we only support analyzers configurable via jSON
  return irs::analysis::analyzers::get(
      type, irs::type<irs::text_format::vpack>::get(),
      iresearch::ref<char>(properties), false);
}

AnalyzerPool::AnalyzerPool(irs::string_ref const& name)
    : _cache(DEFAULT_POOL_SIZE), _name(name) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // validation for name - should  be only normalized or static!
  auto splitted = IResearchAnalyzerFeature::splitAnalyzerName(_name);
  if (splitted.first.empty()) {
    TRI_ASSERT(STATIC_ANALYZERS_NAMES.find(name) !=
               STATIC_ANALYZERS_NAMES.end());  // This should be static analyzer
  }
#endif
}

bool AnalyzerPool::operator==(AnalyzerPool const& rhs) const {
  // intentionally do not check revision! Revision does not affects analyzer
  // functionality!
  return _name == rhs._name && _type == rhs._type &&
         _inputType == rhs._inputType && _returnType == rhs._returnType &&
         _features == rhs._features &&
         basics::VelocyPackHelper::equal(_properties, rhs._properties, true);
}

bool AnalyzerPool::init(irs::string_ref const& type,
                        VPackSlice const properties,
                        AnalyzersRevision::Revision revision, Features features,
                        LinkVersion version) {
  try {
    _cache.clear();  // reset for new type/properties
    _config.clear();

    if (!::normalize(_config, type, properties)) {
      // failed to normalize analyzer definition
      _config.clear();

      return false;
    }
    if (_config.empty()) {
      // even empty slice has some bytes in it.
      // zero bytes output while returned true is clearly a bug
      // in analyzer`s normalization function
      TRI_ASSERT(!_config.empty());
      // in non maintainer mode just prevent corrupted analyzer from activating
      return false;
    }

    // ensure no reallocations will happen
    _config.reserve(_config.size() + type.size());

    auto instance = _cache.emplace(type, iresearch::slice(_config));

    if (instance) {
      _type = irs::string_ref::NIL;
      _key = irs::string_ref::NIL;
      _properties = iresearch::slice(_config);

      if (!type.null()) {
        _config.append(type);
        _type = irs::string_ref(_config.c_str() + _properties.byteSize(),
                                type.size());
      }

      if (instance->type() ==
          irs::type<irs::analysis::pipeline_token_stream>::id()) {
        // pipeline needs to validate members compatibility
        std::string error;
        irs::analysis::analyzer const* prev{nullptr};
        AnalyzerValueType prevType{AnalyzerValueType::Undefined};
        auto checker = [&error, &prev,
                        &prevType](const irs::analysis::analyzer& analyzer) {
          AnalyzerValueType currInput;
          AnalyzerValueType currOutput;
          std::tie(currInput, currOutput, std::ignore) =
              getAnalyzerMeta(&analyzer);
          if (prev) {
            if ((currInput & prevType) == AnalyzerValueType::Undefined) {
              error.append("Incompatible pipeline part found. Analyzer type '")
                  .append(prev->type()().name())
                  .append("' emits output not acceptable by analyzer type '")
                  .append(analyzer.type()().name())
                  .append("'");
              return false;
            }
          }
          prev = &analyzer;
          prevType = currOutput;
          return true;
        };
        auto pipeline =
            static_cast<irs::analysis::pipeline_token_stream*>(instance.get());
        if (!pipeline->visit_members(checker)) {
          LOG_TOPIC("753ff", WARN, iresearch::TOPIC)
              << "Failed to validate pipeline analyzer: " << error;
          return false;
        }
        std::tie(_inputType, std::ignore, _storeFunc) =
            getAnalyzerMeta(instance.get());
        _returnType = prevType;  // for pipeline we take last pipe member output
                                 // type as whole pipe output type
      } else {
        std::tie(_inputType, _returnType, _storeFunc) =
            getAnalyzerMeta(instance.get());
      }
      _fieldFeatures = features.fieldFeatures(version);
      _features = features;  // store only requested features
      _revision = revision;
      return true;
    }
  } catch (basics::Exception& e) {
    LOG_TOPIC("62062", WARN, iresearch::TOPIC)
        << "caught exception while initializing an arangosearch analizer type '"
        << _type << "' properties '" << _properties << "': " << e.code() << " "
        << e.what();
  } catch (std::exception& e) {
    LOG_TOPIC("a9196", WARN, iresearch::TOPIC)
        << "caught exception while initializing an arangosearch analizer type '"
        << _type << "' properties '" << _properties << "': " << e.what();
  } catch (...) {
    LOG_TOPIC("7524a", WARN, iresearch::TOPIC)
        << "caught exception while initializing an arangosearch analizer type '"
        << _type << "' properties '" << _properties << "'";
  }

  _config.clear();                        // set as uninitialized
  _key = irs::string_ref::NIL;            // set as uninitialized
  _type = irs::string_ref::NIL;           // set as uninitialized
  _properties = VPackSlice::noneSlice();  // set as uninitialized
  _features.clear();                      // set as uninitialized

  return false;
}

void AnalyzerPool::setKey(irs::string_ref const& key) {
  if (key.null()) {
    _key = irs::string_ref::NIL;

    return;  // nothing more to do
  }

  // since VPackSlice is not blind pointer
  // store properties status before append.
  // After reallocation all methods of Slice will not work!
  const auto propertiesIsNone = _properties.isNone();
  const auto propertiesByteSize = propertiesIsNone ? 0 : _properties.byteSize();

  const auto keyOffset = _config.size();
  _config.append(key.c_str(), key.size());

  // update '_properties' since '_config' might have been reallocated during
  // append(...)
  if (!propertiesIsNone) {
    TRI_ASSERT(propertiesByteSize <= _config.size());
    _properties = iresearch::slice(_config);
  }

  // update '_type' since '_config' might have been reallocated during
  // append(...)
  if (!_type.null()) {
    TRI_ASSERT(_properties.byteSize() + _type.size() <= _config.size());
    _type =
        irs::string_ref(_config.c_str() + _properties.byteSize(), _type.size());
  }

  _key = irs::string_ref(_config.c_str() + keyOffset, key.size());
}

AnalyzerPool::CacheType::ptr AnalyzerPool::get() const noexcept {
  try {
    return _cache.emplace(_type, _properties);
  } catch (basics::Exception const& e) {
    LOG_TOPIC("c9256", WARN, iresearch::TOPIC)
        << "caught exception while instantiating an arangosearch analizer type "
           "'"
        << _type << "' properties '" << _properties << "': " << e.code() << " "
        << e.what();
  } catch (std::exception& e) {
    LOG_TOPIC("93baf", WARN, iresearch::TOPIC)
        << "caught exception while instantiating an arangosearch analizer type "
           "'"
        << _type << "' properties '" << _properties << "': " << e.what();
  } catch (...) {
    LOG_TOPIC("08db9", WARN, iresearch::TOPIC)
        << "caught exception while instantiating an arangosearch analizer type "
           "'"
        << _type << "' properties '" << _properties << "'";
  }

  return {};
}

IResearchAnalyzerFeature::IResearchAnalyzerFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature(server, IResearchAnalyzerFeature::name()) {
  setOptional(true);
  startsAfter<application_features::V8FeaturePhase>();
  // used for registering IResearch analyzer functions
  startsAfter<aql::AqlFunctionFeature>();
  // used for getting the system database
  // containing the persisted configuration
  startsAfter<SystemDatabaseFeature>();
  startsAfter<application_features::CommunicationFeaturePhase>();
  startsAfter<AqlFeature>();
  startsAfter<aql::OptimizerRulesFeature>();
  startsAfter<QueryRegistryFeature>();
  startsAfter<SchedulerFeature>();

  _gcfunc = [this](bool canceled) {
    if (canceled) {
      return;
    }

    auto cleanupTrans = this->server()
                            .getFeature<ClusterFeature>()
                            .clusterInfo()
                            .createAnalyzersCleanupTrans();
    if (cleanupTrans) {
      if (cleanupTrans->start().ok()) {
        cleanupTrans->commit();
      }
    }

    if (!this->server().isStopping()) {
      ::queueGarbageCollection(_workItemMutex, _workItem, _gcfunc);
    }
  };
}

/*static*/ bool IResearchAnalyzerFeature::canUseVocbase(
    irs::string_ref const& vocbaseName, auth::Level const& level) {
  TRI_ASSERT(!vocbaseName.empty());
  auto& ctx = ExecContext::current();
  auto const nameStr = static_cast<std::string>(vocbaseName);
  return ctx.canUseDatabase(nameStr, level) &&  // can use vocbase
         ctx.canUseCollection(nameStr,
                              arangodb::StaticStrings::AnalyzersCollection,
                              level);  // can use analyzers
}

/*static*/ bool IResearchAnalyzerFeature::canUse(TRI_vocbase_t const& vocbase,
                                                 auth::Level const& level) {
  return canUseVocbase(vocbase.name(), level);
}

/*static*/ bool IResearchAnalyzerFeature::canUse(irs::string_ref const& name,
                                                 auth::Level const& level) {
  auto& ctx = ExecContext::current();

  if (ctx.isAdminUser()) {
    return true;  // authentication not enabled
  }

  auto& staticAnalyzers = getStaticAnalyzers();

  if (staticAnalyzers.find(irs::make_hashed_ref(
          name, std::hash<irs::string_ref>())) != staticAnalyzers.end()) {
    return true;  // special case for singleton static analyzers (always
                  // allowed)
  }

  auto split = splitAnalyzerName(name);
  auto const vocbaseName = static_cast<std::string>(split.first);
  return split.first.null()  // static analyzer (always allowed)
         || (ctx.canUseDatabase(vocbaseName, level)  // can use vocbase
             && ctx.canUseCollection(
                    vocbaseName, arangodb::StaticStrings::AnalyzersCollection,
                    level));  // can use analyzers
}

/*static*/ Result IResearchAnalyzerFeature::createAnalyzerPool(
    AnalyzerPool::ptr& pool, irs::string_ref const& name,
    irs::string_ref const& type, VPackSlice const properties,
    AnalyzersRevision::Revision revision, Features features,
    LinkVersion version, bool extendedNames) {
  // check type available
  if (!irs::analysis::analyzers::exists(
          type, irs::type<irs::text_format::vpack>::get(), false)) {
    return {TRI_ERROR_NOT_IMPLEMENTED,
            "Not implemented analyzer type '" + std::string(type) + "'."};
  }

  // validate analyzer name
  auto split = splitAnalyzerName(name);

  if (!AnalyzerNameValidator::isAllowedName(
          extendedNames,
          std::string_view(split.second.c_str(), split.second.size()))) {
    return {TRI_ERROR_BAD_PARAMETER, "invalid characters in analyzer name '"s +
                                         std::string(split.second) + "'"};
  }

  // validate that features are supported by arangod an ensure that their
  // dependencies are met
  const auto validationRes = features.validate();
  if (validationRes.fail()) {
    return validationRes;
  }

  // limit the maximum size of analyzer properties
  if (ANALYZER_PROPERTIES_SIZE_MAX < properties.byteSize()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "analyzer properties size of '" +
                std::to_string(properties.byteSize()) +
                "' exceeds the maximum allowed limit of '" +
                std::to_string(ANALYZER_PROPERTIES_SIZE_MAX) + "'"};
  }

  auto analyzerPool = std::make_shared<AnalyzerPool>(name);

  if (!analyzerPool->init(type, properties, revision, features, version)) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        "Failure initializing an arangosearch analyzer instance for name '" +
            std::string(name) + "' type '" + std::string(type) + "'." +
            (properties.isNone()
                 ? " Init without properties"s
                 : " Properties '"s + properties.toString() + "'") +
            " was rejected by analyzer. Please check documentation for "
            "corresponding analyzer type."};
  }

  // noexcept
  pool = analyzerPool;
  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate analyzer parameters and emplace into map
////////////////////////////////////////////////////////////////////////////////
Result IResearchAnalyzerFeature::emplaceAnalyzer(
    EmplaceAnalyzerResult& result, Analyzers& analyzers,
    irs::string_ref const name, irs::string_ref const type,
    VPackSlice const properties, Features const& features,
    AnalyzersRevision::Revision revision) {
  // check type available
  if (!irs::analysis::analyzers::exists(
          type, irs::type<irs::text_format::vpack>::get(), false)) {
    return {TRI_ERROR_NOT_IMPLEMENTED,
            "Not implemented analyzer type '" + std::string(type) + "'."};
  }

  // validate analyzer name
  auto split = splitAnalyzerName(name);

  bool extendedNames =
      server().getFeature<DatabaseFeature>().extendedNamesForAnalyzers();
  if (!AnalyzerNameValidator::isAllowedName(
          extendedNames,
          std::string_view(split.second.c_str(), split.second.size()))) {
    return {TRI_ERROR_BAD_PARAMETER,
            std::string("invalid characters in analyzer name '") +
                std::string(split.second) + "'"};
  }

  // validate that features are supported by arangod an ensure that their
  // dependencies are met
  auto validationRes = features.validate();
  if (validationRes.fail()) {
    return validationRes;
  }

  // limit the maximum size of analyzer properties
  if (ANALYZER_PROPERTIES_SIZE_MAX < properties.byteSize()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "analyzer properties size of '" +
                std::to_string(properties.byteSize()) +
                "' exceeds the maximum allowed limit of '" +
                std::to_string(ANALYZER_PROPERTIES_SIZE_MAX) + "'"};
  }

  auto generator =
      [](irs::hashed_string_ref const& key,
         AnalyzerPool::ptr const& value) -> irs::hashed_string_ref {
    auto pool = std::make_shared<AnalyzerPool>(key);  // allocate pool
    const_cast<AnalyzerPool::ptr&>(value) =
        pool;  // lazy-instantiate pool to avoid allocation if pool is already
               // present
    return pool ? irs::hashed_string_ref(key.hash(), pool->name())
                : key;  // reuse hash but point ref at value in pool
  };
  auto itr = irs::map_utils::try_emplace_update_key(
      analyzers, generator,
      irs::make_hashed_ref(name, std::hash<irs::string_ref>()));
  auto analyzer = itr.first->second;

  if (!analyzer) {
    return {TRI_ERROR_BAD_PARAMETER,
            "failure creating an arangosearch analyzer instance for name '" +
                std::string(name) + "' type '" + std::string(type) +
                "' properties '" + properties.toString() + "'"};
  }

  // new analyzer creation, validate
  if (itr.second) {
    bool erase = true;  // potentially invalid insertion took place
    auto cleanup = irs::make_finally([&erase, &analyzers, &itr]() -> void {
      // cppcheck-suppress knownConditionTrueFalse
      if (erase) {
        analyzers.erase(
            itr.first);  // ensure no broken analyzers are left behind
      }
    });

    // emplaceAnalyzer is used by Analyzers API where we don't actually use
    // features
    if (!analyzer->init(type, properties, revision, features,
                        LinkVersion::MIN)) {
      return {
          TRI_ERROR_BAD_PARAMETER,
          "Failure initializing an arangosearch analyzer instance for name '" +
              std::string(name) + "' type '" + std::string(type) + "'." +
              (properties.isNone() ? std::string(" Init without properties")
                                   : std::string(" Properties '") +
                                         properties.toString() + "'") +
              " was rejected by analyzer. Please check documentation for "
              "corresponding analyzer type."};
    }

    erase = false;
  } else if (!equalAnalyzer(*analyzer, type, properties,
                            features)) {  // duplicate analyzer with different
                                          // configuration
    std::ostringstream
        errorText;  // make it look more like velocypack toString result
    errorText << "Name collision detected while registering an arangosearch "
                 "analyzer.\n"
              << "Current definition is:\n{\n  name:'" << name << "'\n"
              << "  type: '" << type << "'\n";
    if (!properties.isNone()) {
      errorText << "  properties:'" << properties.toString() << "'\n";
    }
    errorText << "  features: [\n";

    features.visit(
        [&errorText, first = true](std::string_view feature) mutable {
          if (!first) {
            errorText << ",";
          } else {
            first = false;
          }
          errorText << "    '" << feature << "'";
          errorText << "\n";
        });

    VPackBuilder existingDefinition;
    analyzer->toVelocyPack(existingDefinition, false);
    errorText << "  ]\n}\nPrevious definition was:\n"
              << existingDefinition.toString();
    return {TRI_ERROR_BAD_PARAMETER, errorText.str()};
  }

  result = itr;

  return {};
}

Result IResearchAnalyzerFeature::emplace(EmplaceResult& result,
                                         irs::string_ref const& name,
                                         irs::string_ref const& type,
                                         VPackSlice const properties,
                                         Features features /* = {} */) {
  auto const split = splitAnalyzerName(name);

  auto transaction =
      createAnalyzerModificationTransaction(server(), split.first);
  if (transaction) {
    auto startRes = transaction->start();
    if (startRes.fail()) {
      return startRes;
    }
  }

  try {
    if (!split.first
             .null()) {  // do not trigger load for static-analyzer requests
      if (transaction) {
        auto cleanupResult = cleanupAnalyzersCollection(
            split.first, transaction->buildingRevision());
        if (cleanupResult.fail()) {
          return cleanupResult;
        }
      }

      auto res = loadAnalyzers(split.first);

      if (!res.ok()) {
        return res;
      }
    }

    WRITE_LOCKER(lock, _mutex);

    // validate and emplace an analyzer
    EmplaceAnalyzerResult itr;
    auto res = emplaceAnalyzer(
        itr, _analyzers, name, type, properties, features,
        transaction ? transaction->buildingRevision() : AnalyzersRevision::MIN);

    if (!res.ok()) {
      return res;
    }

    auto& engine = server().getFeature<EngineSelectorFeature>().engine();
    bool erase = itr.second;  // an insertion took place
    auto cleanup = irs::make_finally([&erase, this, &itr]() -> void {
      if (erase) {
        _analyzers.erase(
            itr.first);  // ensure no broken analyzers are left behind
      }
    });
    auto pool = itr.first->second;

    // new pool creation
    if (itr.second) {
      if (!pool) {
        return {
            TRI_ERROR_INTERNAL,
            "failure creating an arangosearch analyzer instance for name '" +
                std::string(name) + "' type '" + std::string(type) +
                "' properties '" + properties.toString() + "'"};
      }

      // persist only on coordinator and single-server while not in recovery
      if ((!engine.inRecovery())  // do not persist during recovery
          && (ServerState::instance()->isCoordinator()          // coordinator
              || ServerState::instance()->isSingleServer())) {  // single-server
        res = storeAnalyzer(*pool);
      }

      if (res.fail()) {
        return res;
      }

      // cppcheck-suppress unreadVariable
      if (transaction) {
        res = transaction->commit();
        if (res.fail()) {
          return res;
        }
        auto cached = _lastLoad.find(static_cast<std::string>(
            split.first));  // FIXME: remove cast after C++20
        TRI_ASSERT(cached != _lastLoad.end());
        if (ADB_LIKELY(cached != _lastLoad.end())) {
          cached->second =
              transaction->buildingRevision();  // as we already "updated cache"
                                                // to this revision
        }
      }
      erase = false;
    }
    result = std::make_pair(pool, itr.second);
  } catch (basics::Exception const& e) {
    return {e.code(),
            StringUtils::concatT("caught exception while registering an "
                                 "arangosearch analyzer name '",
                                 name, "' type '", type, "' properties '",
                                 properties.toString(), "': ", e.code(), " ",
                                 e.what())};
  } catch (std::exception const& e) {
    return {
        TRI_ERROR_INTERNAL,
        "caught exception while registering an arangosearch analyzer name '" +
            std::string(name) + "' type '" + std::string(type) +
            "' properties '" + properties.toString() + "': " + e.what()};
  } catch (...) {
    return {
        TRI_ERROR_INTERNAL,  // code
        "caught exception while registering an arangosearch analyzer name '" +
            std::string(name) + "' type '" + std::string(type) +
            "' properties '" + properties.toString() + "'"};
  }

  return {};
}

Result IResearchAnalyzerFeature::removeAllAnalyzers(TRI_vocbase_t& vocbase) {
  auto analyzerModificationTrx =
      createAnalyzerModificationTransaction(server(), vocbase.name());
  if (analyzerModificationTrx) {
    auto startRes = analyzerModificationTrx->start();
    if (startRes.fail()) {
      return startRes;
    }
    auto cleanupResult = cleanupAnalyzersCollection(
        vocbase.name(), analyzerModificationTrx->buildingRevision());
    if (cleanupResult.fail()) {
      return cleanupResult;
    }
  }
  auto& engine = server().getFeature<EngineSelectorFeature>().engine();
  TRI_ASSERT(!engine.inRecovery());
  if (!analyzerModificationTrx) {
    // no modification transaction. Just truncate
    auto ctx = transaction::StandaloneContext::Create(vocbase);
    SingleCollectionTransaction trx(
        ctx, arangodb::StaticStrings::AnalyzersCollection,
        AccessMode::Type::EXCLUSIVE);

    auto res = trx.begin();
    if (res.fail()) {
      return res;
    }

    OperationOptions options;
    trx.truncateAsync(arangodb::StaticStrings::AnalyzersCollection, options)
        .get();
    res = trx.commitAsync().get();
    if (res.ok()) {
      invalidate(vocbase);
    }
    return res;
  } else {
    // as in single analyzer case. First mark for deletion (but all at once)
    auto stringRev =
        std::to_string(analyzerModificationTrx->buildingRevision());
    std::string aql("FOR u IN ");
    aql.append(arangodb::StaticStrings::AnalyzersCollection)
        .append(" UPDATE u.")
        .append(arangodb::StaticStrings::KeyString)
        .append(" WITH { ")
        .append(arangodb::StaticStrings::AnalyzersDeletedRevision)
        .append(": ")
        .append(stringRev)
        .append("} IN ")
        .append(arangodb::StaticStrings::AnalyzersCollection);

    {  // SingleCollectionTransaction scope
      auto ctx = transaction::StandaloneContext::Create(vocbase);
      SingleCollectionTransaction trx(
          ctx, arangodb::StaticStrings::AnalyzersCollection,
          AccessMode::Type::EXCLUSIVE);

      auto res = trx.begin();
      if (res.fail()) {
        return res;
      }
      auto query = aql::Query::create(ctx, aql::QueryString(aql), nullptr);
      aql::QueryResult queryResult = query->executeSync();
      if (queryResult.fail()) {
        return queryResult.result;
      }
      res = trx.commitAsync().get();
      if (!res.ok()) {
        return res;
      }
      res = analyzerModificationTrx->commit();
      if (res.fail()) {
        return res;
      }
    }
    {  // SingleCollectionTransaction scope
      // now let`s do cleanup
      auto ctx = transaction::StandaloneContext::Create(vocbase);
      SingleCollectionTransaction truncateTrx(
          ctx, arangodb::StaticStrings::AnalyzersCollection,
          AccessMode::Type::EXCLUSIVE);

      auto res = truncateTrx.begin();

      if (res.ok()) {
        OperationOptions options;
        truncateTrx
            .truncateAsync(arangodb::StaticStrings::AnalyzersCollection,
                           options)
            .get();
        res = truncateTrx.commitAsync().get();
      }
      if (res.fail()) {
        // failed cleanup is not critical problem. just log it
        LOG_TOPIC("70a8c", WARN, iresearch::TOPIC)
            << " Failed to finalize analyzer truncation"
            << " Error Code:" << res.errorNumber()
            << " Error:" << res.errorMessage();
      }
    }
    invalidate(vocbase);
    return {};
  }
}

Result IResearchAnalyzerFeature::bulkEmplace(TRI_vocbase_t& vocbase,
                                             VPackSlice const dumpedAnalyzers) {
  TRI_ASSERT(dumpedAnalyzers.isArray());
  TRI_ASSERT(!dumpedAnalyzers.isEmptyArray());
  auto transaction =
      createAnalyzerModificationTransaction(server(), vocbase.name());
  if (transaction) {
    auto startRes = transaction->start();
    if (startRes.fail()) {
      return startRes;
    }
  }
  try {
    if (transaction) {
      auto cleanupResult = cleanupAnalyzersCollection(
          vocbase.name(), transaction->buildingRevision());
      if (cleanupResult.fail()) {
        return cleanupResult;
      }
    }
    auto res = loadAnalyzers(vocbase.name());
    if (!res.ok()) {
      return res;
    }

    WRITE_LOCKER(lock, _mutex);

    auto& engine = server().getFeature<EngineSelectorFeature>().engine();
    TRI_ASSERT(!engine.inRecovery());
    bool erase = true;
    std::vector<irs::hashed_string_ref> inserted;
    auto cleanup = irs::make_finally([&erase, &inserted, this]() {
      if (erase) {
        for (auto const& s : inserted) {
          auto itr = _analyzers.find(s);
          if (itr == _analyzers.end()) {
            _analyzers.erase(
                itr);  // ensure no broken analyzers are left behind
          }
        }
      }
    });
    for (auto const& slice : VPackArrayIterator(dumpedAnalyzers)) {
      if (!slice.isObject()) {
        continue;
      }
      Features features;
      irs::string_ref name;
      irs::string_ref type;
      VPackSlice properties;
      auto parseRes =
          parseAnalyzerSlice(slice, name, type, features, properties);
      if (parseRes.fail()) {
        LOG_TOPIC("83638", ERR, iresearch::TOPIC)
            << parseRes.errorMessage() << " while loading analyzer from dump"
            << ", skipping it: " << slice.toString();
        continue;
      }

      EmplaceAnalyzerResult itr;
      auto normalizedName = normalizedAnalyzerName(vocbase.name(), name);
      auto res = emplaceAnalyzer(itr, _analyzers, normalizedName, type,
                                 properties, features,
                                 transaction ? transaction->buildingRevision()
                                             : AnalyzersRevision::MIN);

      if (!res.ok()) {
        LOG_TOPIC("9b095", ERR, arangodb::iresearch::TOPIC)
            << "failed to emplace analyzer from dump"
            << " because of: " << res.errorMessage()
            << ", skipping it: " << slice.toString();
        // here we differs from loadAnalyzers.
        // we allow to import as many analyzers as possible and not break at
        // first emplace failure maybe there is old dump and properties not
        // supported anymore and so on.
        continue;  // skip analyzer
      }

      auto pool = itr.first->second;
      // new pool creation
      if (itr.second) {
        inserted.push_back(itr.first->first);
        if (!pool) {
          // no pool means something heavily broken
          // better to abort
          return {
              TRI_ERROR_INTERNAL,
              "failure creating an arangosearch analyzer instance for name '" +
                  std::string(name) + "' type '" + std::string(type) +
                  "' properties '" + properties.toString() + "'"};
        }
        TRI_ASSERT(!engine.inRecovery());
        // persist only on coordinator and single-server while not in recovery
        if (ServerState::instance()->isCoordinator()         // coordinator
            || ServerState::instance()->isSingleServer()) {  // single-server
          res = storeAnalyzer(*pool);
        }

        if (res.fail()) {
          // storage errors are considered critical
          // as that means database is broken or we have write conflict
          // better to totally abort emplacing
          return res;
        }
      }
    }
    if (ADB_UNLIKELY(inserted.empty())) {
      // nothing changed. So nothing to commit;
      return {};
    }
    // cppcheck-suppress unreadVariable
    if (transaction) {
      res = transaction->commit();
      if (res.fail()) {
        return res;
      }
      auto cached = _lastLoad.find(vocbase.name());
      TRI_ASSERT(cached != _lastLoad.end());
      if (ADB_LIKELY(cached != _lastLoad.end())) {
        cached->second =
            transaction->buildingRevision();  // as we already "updated cache"
                                              // to this revision
      }
    }
    erase = false;
  } catch (basics::Exception const& e) {
    return {
        e.code(),
        StringUtils::concatT(
            "caught exception while registering an arangosearch analyzers: ",
            e.code(), " ", e.what())};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while registering an arangosearch analyzers: "s +
                e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,  // code
            "caught exception while registering an arangosearch analyzers"};
  }

  return {};
}

AnalyzerPool::ptr IResearchAnalyzerFeature::get(
    irs::string_ref const& normalizedName, AnalyzerName const& name,
    AnalyzersRevision::Revision const revision,
    bool onlyCached) const noexcept {
  try {
    if (!name.first.null()) {  // check if analyzer is static
      if (!onlyCached) {
        // load analyzers for database
        auto const endTime =
            TRI_microtime() +
            5.0;  // arbitrary value - give some time to update plan
        do {
          auto const res =
              const_cast<IResearchAnalyzerFeature*>(this)->loadAnalyzers(
                  name.first);
          if (!res.ok()) {
            LOG_TOPIC("36062", WARN, iresearch::TOPIC)
                << "failure to load analyzers for database '" << name.first
                << "' while getting analyzer '" << name
                << "': " << res.errorNumber() << " " << res.errorMessage();
            TRI_set_errno(res.errorNumber());

            return nullptr;
          }
          if (revision == AnalyzersRevision::LATEST) {
            break;  // we don`t care about specific revision.
          }
          {
            READ_LOCKER(lock, _mutex);
            auto itr = _lastLoad.find(static_cast<std::string>(
                name.first));  // FIXME: after C++20 remove cast and use
                               // heterogeneous lookup
            if (itr != _lastLoad.end() && itr->second >= revision) {
              break;  // expected or later revision is loaded
            }
          }
          if (TRI_microtime() > endTime) {
            LOG_TOPIC("6a908", WARN, iresearch::TOPIC)
                << "Failed to update analyzers cache to revision: '" << revision
                << "' in database '" << name.first << "'";
            break;  // do not return error. Maybe requested analyzer was already
                    // presented earlier, so we still may succeed
          }
          LOG_TOPIC("6879a", DEBUG, iresearch::TOPIC)
              << "Failed to update analyzers cache to revision: '" << revision
              << "' in database '" << name.first << "' Retrying...";
        } while (true);
      }
    }

    READ_LOCKER(lock, _mutex);
    auto itr = _analyzers.find(
        irs::make_hashed_ref(normalizedName, std::hash<irs::string_ref>()));

    if (itr == _analyzers.end()) {
      LOG_TOPIC("4049c", WARN, iresearch::TOPIC)
          << "failure to find arangosearch analyzer name '" << normalizedName
          << "'";

      return nullptr;
    }

    auto pool = itr->second;

    if (pool) {
      if (pool->revision() <= revision) {
        return pool;
      } else {
        LOG_TOPIC("c4c20", WARN, iresearch::TOPIC)
            << "invalid analyzer revision. Requested " << revision << " got "
            << pool->revision();
      }
    }

    LOG_TOPIC("1a29c", WARN, iresearch::TOPIC)
        << "failure to get arangosearch analyzer name '" << normalizedName
        << "'";
    TRI_set_errno(TRI_ERROR_INTERNAL);
  } catch (basics::Exception const& e) {
    LOG_TOPIC("29eff", WARN, iresearch::TOPIC)
        << "caught exception while retrieving an arangosearch analizer name '"
        << normalizedName << "': " << e.code() << " " << e.what();
  } catch (std::exception const& e) {
    LOG_TOPIC("ce8d5", WARN, iresearch::TOPIC)
        << "caught exception while retrieving an arangosearch analizer name '"
        << normalizedName << "': " << e.what();
  } catch (...) {
    LOG_TOPIC("5505f", WARN, iresearch::TOPIC)
        << "caught exception while retrieving an arangosearch analizer name '"
        << normalizedName << "'";
  }

  return nullptr;
}

AnalyzerPool::ptr IResearchAnalyzerFeature::get(
    irs::string_ref const& name, TRI_vocbase_t const& activeVocbase,
    QueryAnalyzerRevisions const& revision, bool onlyCached /*= false*/) const {
  auto const normalizedName = normalize(name, activeVocbase.name(), true);

  auto const split = splitAnalyzerName(normalizedName);

  if (!split.first.null() && split.first != activeVocbase.name() &&
      split.first != arangodb::StaticStrings::SystemDatabase) {
    // accessing local analyzer from within another database
    return nullptr;
  }
  // getVocbaseRevision expects vocbase name and this is ensured by
  // normalize with expandVocbasePrefx = true
  TRI_ASSERT(split.first.null() || !split.first.empty());
  return get(normalizedName, split,
             split.first.null()
                 ? AnalyzersRevision::MIN  // built-in analyzers always has MIN
                                           // revision
                 : revision.getVocbaseRevision(split.first),
             onlyCached);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a container of statically defined/initialized analyzers
////////////////////////////////////////////////////////////////////////////////
/*static*/ IResearchAnalyzerFeature::Analyzers const&
IResearchAnalyzerFeature::getStaticAnalyzers() {
  struct Instance {
    Analyzers analyzers;
    Instance() {
      // register the identity analyzer
      {
        Features constexpr extraFeatures{FieldFeatures::NORM,
                                         irs::IndexFeatures::FREQ};
        TRI_ASSERT(extraFeatures.validate().ok());

        auto pool =
            std::make_shared<AnalyzerPool>(IdentityAnalyzer::type_name());

        static_assert(STATIC_ANALYZERS_NAMES.begin()->first ==
                          irs::type<IdentityAnalyzer>::name(),
                      "Identity analyzer is misplaced");
        if (!pool ||
            !pool->init(irs::type<IdentityAnalyzer>::name(),
                        VPackSlice::emptyObjectSlice(), AnalyzersRevision::MIN,
                        extraFeatures, LinkVersion::MIN)) {
          LOG_TOPIC("26de1", WARN, iresearch::TOPIC)
              << "failure creating an arangosearch static analyzer instance "
                 "for name '"
              << IdentityAnalyzer::type_name() << "'";

          // this should never happen, treat as an assertion failure
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL,
              "failed to create arangosearch static analyzer");
        }

        analyzers.try_emplace(
            irs::make_hashed_ref(irs::string_ref(pool->name()),
                                 std::hash<irs::string_ref>()),
            pool);
      }

      // register the text analyzers
      {
        // Note: ArangoDB strings coming from JavaScript user input are UTF-8
        // encoded add norms + frequency/position for by_phrase
        Features constexpr extraFeatures{
            FieldFeatures::NORM,
            irs::IndexFeatures::FREQ | irs::IndexFeatures::POS};
        TRI_ASSERT(extraFeatures.validate().ok());

        irs::string_ref constexpr type("text");

        VPackBuilder properties;
        static_assert(STATIC_ANALYZERS_NAMES.size() > 1,
                      "Static analyzer count too low");
        for (auto staticName = (STATIC_ANALYZERS_NAMES.cbegin() + 1);
             staticName != STATIC_ANALYZERS_NAMES.cend(); ++staticName) {
          // { locale: "<locale>.UTF-8", stopwords: [] }
          {
            properties.clear();
            VPackObjectBuilder rootScope(&properties);
            properties.add(
                "locale",
                VPackValue(std::string(staticName->second) + ".UTF-8"));
            if (staticName->second == "zh") {
              // no stemmer for Chinese
              properties.add("stemming", VPackValue(false));
            }
            VPackArrayBuilder stopwordsArrayScope(&properties, "stopwords");
          }

          auto pool = std::make_shared<AnalyzerPool>(staticName->first);

          if (!pool->init(type, properties.slice(), AnalyzersRevision::MIN,
                          extraFeatures, LinkVersion::MIN)) {
            LOG_TOPIC("e25f5", WARN, iresearch::TOPIC)
                << "failure creating an arangosearch static analyzer instance "
                   "for name '"
                << std::string(staticName->first) << "'";

            // this should never happen, treat as an assertion failure
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_INTERNAL,
                "failed to create arangosearch static analyzer instance");
          }

          analyzers.try_emplace(
              irs::make_hashed_ref(irs::string_ref(pool->name()),
                                   std::hash<irs::string_ref>()),
              pool);
        }
      }
    }
  };
  static const Instance instance;

  // cppcheck-suppress returnReference
  return instance.analyzers;
}

/*static*/ AnalyzerPool::ptr IResearchAnalyzerFeature::identity() noexcept {
  struct Identity {
    AnalyzerPool::ptr instance;
    Identity() {
      // find the 'identity' analyzer pool in the static analyzers
      auto& staticAnalyzers = getStaticAnalyzers();
      auto key = irs::make_hashed_ref(IdentityAnalyzer::type_name(),
                                      std::hash<irs::string_ref>());
      auto itr = staticAnalyzers.find(key);

      if (itr != staticAnalyzers.end()) {
        instance = itr->second;
      }
    }
  };
  static const Identity identity;

  return identity.instance;
}

Result IResearchAnalyzerFeature::cleanupAnalyzersCollection(
    irs::string_ref const& database,
    AnalyzersRevision::Revision buildingRevision) {
  if (ServerState::instance()->isCoordinator()) {
    if (!server().hasFeature<DatabaseFeature>()) {
      return {TRI_ERROR_INTERNAL,
              "failure to find feature 'Database' while loading analyzers for "
              "database '" +
                  std::string(database) + "'"};
    }

    auto& dbFeature = server().getFeature<DatabaseFeature>();
    auto& engine = server().getFeature<EngineSelectorFeature>().engine();
    auto* vocbase = dbFeature.lookupDatabase(
        static_cast<std::string>(database));  // FIXME: after C++20 remove cast
                                              // and use heterogeneous lookup
    if (!vocbase) {
      if (engine.inRecovery()) {
        return {};  // database might not have come up yet
      }
      return {TRI_ERROR_INTERNAL,
              "failure to find feature database while loading analyzers for "
              "database '" +
                  std::string(database) + "'"};
    }
    static const auto queryDeleteString = aql::QueryString(
        "FOR d IN " + arangodb::StaticStrings::AnalyzersCollection +
        " FILTER d." + arangodb::StaticStrings::AnalyzersRevision +
        " >= @rev OR ( HAS(d, '" +
        arangodb::StaticStrings::AnalyzersDeletedRevision + "') AND d." +
        arangodb::StaticStrings::AnalyzersDeletedRevision +
        " < @rev) REMOVE d IN " + arangodb::StaticStrings::AnalyzersCollection);

    auto bindBuilder = std::make_shared<VPackBuilder>();
    bindBuilder->openObject();
    bindBuilder->add("rev", VPackValue(buildingRevision));
    bindBuilder->close();

    auto ctx = transaction::StandaloneContext::Create(*vocbase);
    SingleCollectionTransaction trx(
        ctx, arangodb::StaticStrings::AnalyzersCollection,
        AccessMode::Type::WRITE);
    trx.begin();

    auto queryDelete = aql::Query::create(ctx, queryDeleteString, bindBuilder);

    auto deleteResult = queryDelete->executeSync();
    if (deleteResult.fail()) {
      return {TRI_ERROR_INTERNAL,
              basics::StringUtils::concatT(
                  "failure to remove dangling analyzers from '", database,
                  "' Aql error: (", deleteResult.errorNumber(), " ) ",
                  deleteResult.errorMessage())};
    }

    static const auto queryUpdateString = aql::QueryString(
        "FOR d IN " + arangodb::StaticStrings::AnalyzersCollection +
        " FILTER " + " ( HAS(d, '" +
        arangodb::StaticStrings::AnalyzersDeletedRevision + "') AND d." +
        arangodb::StaticStrings::AnalyzersDeletedRevision + " >= @rev) " +
        "UPDATE d WITH UNSET(d, '" +
        arangodb::StaticStrings::AnalyzersDeletedRevision + "') IN " +
        arangodb::StaticStrings::AnalyzersCollection);
    auto queryUpdate = aql::Query::create(ctx, queryUpdateString, bindBuilder);

    auto updateResult = queryUpdate->executeSync();
    if (updateResult.fail()) {
      return {TRI_ERROR_INTERNAL,
              basics::StringUtils::concatT(
                  "failure to restore dangling analyzers from '", database,
                  "' Aql error: (", updateResult.errorNumber(), " ) ",
                  updateResult.errorMessage())};
    }

    auto commitRes = trx.finish(Result{});
    if (commitRes.fail()) {
      return commitRes;
    }
  }
  return {};
}

Result IResearchAnalyzerFeature::loadAvailableAnalyzers(
    irs::string_ref const& dbName) {
  if (!ServerState::instance()->isCoordinator()) {
    // Single-servers will load analyzers they need in regular get call.
    // DbServer receives analyzers definitions from coordinators in ddl requests
    // and dbservers never should start ddl by themselves.
    return {};
  }
  Result res{};
  if (canUseVocbase(dbName, auth::Level::RO)) {
    res = loadAnalyzers(dbName);
    if (res.fail()) {
      return res;
    }
  }
  if (dbName != arangodb::StaticStrings::SystemDatabase &&
      canUseVocbase(arangodb::StaticStrings::SystemDatabase, auth::Level::RO)) {
    // System is available for all other databases. So reload its analyzers too
    res = loadAnalyzers(arangodb::StaticStrings::SystemDatabase);
  }
  return res;
}

Result IResearchAnalyzerFeature::loadAnalyzers(
    irs::string_ref const& database /*= irs::string_ref::NIL*/) {
  if (!server().hasFeature<DatabaseFeature>()) {
    return {TRI_ERROR_INTERNAL,
            "failure to find feature 'Database' while loading analyzers for "
            "database '" +
                std::string(database) + "'"};
  }

  auto& dbFeature = server().getFeature<DatabaseFeature>();

  try {
    // '_analyzers'/'_lastLoad' can be asynchronously read
    WRITE_LOCKER(lock, _mutex);

    // load all databases
    if (database.null()) {
      Result res;
      std::unordered_set<irs::hashed_string_ref> seen;
      auto visitor = [this, &res, &seen](TRI_vocbase_t& vocbase) -> void {
        auto name = irs::make_hashed_ref(irs::string_ref(vocbase.name()),
                                         std::hash<irs::string_ref>());
        auto result = loadAnalyzers(name);
        auto itr = _lastLoad.find(
            static_cast<std::string>(name));  // FIXME: after C++20 remove cast
                                              // and use heterogeneous lookup

        if (itr != _lastLoad.end()) {
          seen.insert(name);
        } else if (res.ok()) {  // load errors take precedence
          res = {TRI_ERROR_INTERNAL,
                 "failure to find database last load timestamp after loading "
                 "analyzers"};
        }

        if (!result.ok()) {
          res = result;
        }
      };

      dbFeature.enumerateDatabases(visitor);

      std::unordered_set<std::string>
          unseen;  // make copy since original removed

      // remove unseen databases from timestamp list
      for (auto itr = _lastLoad.begin(), end = _lastLoad.end(); itr != end;) {
        auto name = irs::make_hashed_ref(irs::string_ref(itr->first),
                                         std::hash<irs::string_ref>());
        auto seenItr = seen.find(name);

        if (seenItr == seen.end()) {
          unseen.insert(std::move(itr->first));  // reuse key
          itr = _lastLoad.erase(itr);
        } else {
          ++itr;
        }
      }

      // remove no longer valid analyzers (force remove)
      for (auto itr = _analyzers.begin(), end = _analyzers.end(); itr != end;) {
        auto split = splitAnalyzerName(itr->first);
        // ignore static analyzers
        auto unseenItr =
            split.first.null()
                ? unseen.end()
                : unseen.find(static_cast<std::string>(
                      split.first));  // FIXME: remove cast at C++20

        if (unseenItr != unseen.end()) {
          itr = _analyzers.erase(itr);
        } else {
          ++itr;
        }
      }

      return res;
    }

    // .........................................................................
    // after here load analyzers from a specific database
    // .........................................................................

    auto databaseKey =  // database key used in '_lastLoad'
        irs::make_hashed_ref(database, std::hash<irs::string_ref>());
    auto& engine = server().getFeature<EngineSelectorFeature>().engine();
    auto itr = _lastLoad.find(static_cast<std::string>(
        databaseKey));  // find last update timestamp FIXME: after C++20 remove
                        // cast and use heterogeneous lookup

    auto* vocbase = dbFeature.lookupDatabase(
        static_cast<std::string>(database));  // FIXME: after C++20 remove cast
                                              // and use heterogeneous lookup

    if (!vocbase) {
      if (engine.inRecovery()) {
        return {};  // database might not have come up yet
      }
      if (itr != _lastLoad.end()) {
        cleanupAnalyzers(database);  // cleanup any analyzers for 'database'
        _lastLoad.erase(itr);
      }

      return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
              "failed to find database '" + std::string(database) +
                  "' while loading analyzers"};
    }

    AnalyzersRevision::Revision loadingRevision{
        getAnalyzersRevision(*vocbase, true)->getRevision()};

    if (engine.inRecovery()) {
      // always load if inRecovery since collection contents might have changed
      // unless on db-server which does not store analyzer definitions in
      // collections
      if (ServerState::instance()->isDBServer()) {
        return {};  // db-server should not access cluster during inRecovery
      }
    } else if (ServerState::instance()->isSingleServer()) {  // single server
      if (itr != _lastLoad.end()) {
        return {};  // do not reload on single-server
      }
    } else if (itr != _lastLoad.end()                // had a previous load
               && itr->second == loadingRevision) {  // nothing changed
      LOG_TOPIC("47cb8", TRACE, iresearch::TOPIC)
          << "Load skipped. Revision:" << itr->second
          << " Current revision:" << loadingRevision;
      return {};  // reload interval not reached
    }

    Analyzers analyzers;
    auto visitor = [this, &analyzers, &vocbase, &loadingRevision](
                       velocypack::Slice const& slice) -> Result {
      if (!slice.isObject()) {
        LOG_TOPIC("5c7a5", ERR, iresearch::TOPIC)
            << "failed to find an object value for analyzer definition while "
               "loading analyzer form collection '"
            << arangodb::StaticStrings::AnalyzersCollection << "' in database '"
            << vocbase->name() << "', skipping it: " << slice.toString();

        return {};  // skip analyzer
      }

      irs::string_ref key;
      irs::string_ref name;
      irs::string_ref type;
      VPackSlice properties;
      if (!slice.hasKey(
              arangodb::StaticStrings::KeyString)  // no such field (required)
          || !slice.get(arangodb::StaticStrings::KeyString).isString()) {
        LOG_TOPIC("1dc56", ERR, iresearch::TOPIC)
            << "failed to find a string value for analyzer '"
            << arangodb::StaticStrings::KeyString
            << "' while loading analyzer form collection '"
            << arangodb::StaticStrings::AnalyzersCollection << "' in database '"
            << vocbase->name() << "', skipping it: " << slice.toString();

        return {};  // skip analyzer
      }

      key = getStringRef(slice.get(arangodb::StaticStrings::KeyString));

      Features features;
      auto parseRes =
          parseAnalyzerSlice(slice, name, type, features, properties);

      if (parseRes.fail()) {
        LOG_TOPIC("f5920", ERR, iresearch::TOPIC)
            << parseRes.errorMessage()
            << " while loading analyzer form collection '"
            << arangodb::StaticStrings::AnalyzersCollection << "' in database '"
            << vocbase->name() << "', skipping it: " << slice.toString();

        return {};  // skip analyzer
      }

      AnalyzersRevision::Revision revision{AnalyzersRevision::MIN};
      if (slice.hasKey(arangodb::StaticStrings::AnalyzersRevision)) {
        revision = slice.get(arangodb::StaticStrings::AnalyzersRevision)
                       .getNumber<AnalyzersRevision::Revision>();
      }
      if (revision > loadingRevision) {
        LOG_TOPIC("44a5b", DEBUG, iresearch::TOPIC)
            << "analyzer " << name
            << " ignored as not existed. Revision:" << revision
            << " Current revision:" << loadingRevision;
        return {};  // this analyzers is still not exists for our revision
      }
      if (slice.hasKey(arangodb::StaticStrings::AnalyzersDeletedRevision)) {
        auto deletedRevision =
            slice.get(arangodb::StaticStrings::AnalyzersDeletedRevision)
                .getNumber<AnalyzersRevision::Revision>();
        if (deletedRevision <= loadingRevision) {
          LOG_TOPIC("93b34", DEBUG, iresearch::TOPIC)
              << "analyzer " << name
              << " ignored as deleted. Deleted revision:" << deletedRevision
              << " Current revision:" << loadingRevision;
          return {};  // this analyzers already not exists for our revision
        }
      }

      auto normalizedName = normalizedAnalyzerName(vocbase->name(), name);
      EmplaceAnalyzerResult result;
      auto res = emplaceAnalyzer(result, analyzers, normalizedName, type,
                                 properties, features, revision);

      if (!res.ok()) {
        LOG_TOPIC("7cc7f", ERR, iresearch::TOPIC)
            << "analyzer '" << name
            << "' ignored as emplace failed with reason:" << res.errorMessage();
        return {};  // caught error emplacing analyzer  - skip it
      }

      if (result.second && result.first->second) {
        result.first->second->setKey(key);  // update key
      }
      return {};
    };
    auto const res = visitAnalyzers(*vocbase, visitor);
    if (!res.ok()) {
      if (res.errorNumber() == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
        // collection not found, cleanup any analyzers for 'database'
        if (itr != _lastLoad.end()) {
          cleanupAnalyzers(database);
        }
        _lastLoad[static_cast<std::string>(databaseKey)] =
            loadingRevision;  // FIXME: remove cast at C++20
        return {};            // no collection means nothing to load
      }
      return res;
    }

    // copy over relevant analyzers from '_analyzers' and validate no duplicates
    for (auto& entry : _analyzers) {
      if (!entry.second) {
        continue;  // invalid analyzer (should never happen if insertions done
                   // via here)
      }

      auto split = splitAnalyzerName(entry.first);

      Analyzers::iterator itr;
      // different database
      if (split.first != vocbase->name()) {
        auto result = analyzers.emplace(entry.first, entry.second);
        if (!result.second) {  // existing entry
          itr = result.first;
        } else {
          itr = analyzers.end();
        }
      } else {
        itr = analyzers.find(entry.first);
      }
      if (itr == analyzers.end()) {
        continue;  // no conflict or removed analyzer
      }
      if (!itr->second ||
          equalAnalyzer(*(entry.second), itr->second->type(),
                        itr->second->properties(), itr->second->features())) {
        itr->second = entry.second;  // reuse old analyzer pool to avoid
                                     // duplicates in memmory
        const_cast<Analyzers::key_type&>(itr->first) =
            entry.first;  // point key at old pool
      } else if (itr->second->revision() == entry.second->revision()) {
        return {TRI_ERROR_BAD_PARAMETER,
                "name collision detected while re-registering a duplicate "
                "arangosearch analyzer name '" +
                    std::string(itr->second->name()) + "' type '" +
                    std::string(itr->second->type()) + "' properties '" +
                    itr->second->properties().toString() + "', revision " +
                    std::to_string(itr->second->revision()) +
                    ", previous registration type '" +
                    std::string(entry.second->type()) + "' properties '" +
                    entry.second->properties().toString() + "'" +
                    ", revision " + std::to_string(entry.second->revision())};
      }
    }
    _lastLoad[static_cast<std::string>(databaseKey)] =
        loadingRevision;                // FIXME: remove cast at C++20
    _analyzers = std::move(analyzers);  // update mappings
  } catch (basics::Exception const& e) {
    return {e.code(),
            StringUtils::concatT("caught exception while loading configuration "
                                 "for arangosearch analyzers from database '",
                                 database, "': ", e.code(), " ", e.what())};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while loading configuration for arangosearch "
            "analyzers from database '" +
                std::string(database) + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while loading configuration for arangosearch "
            "analyzers from database '" +
                std::string(database) + "'"};
  }

  return {};
}

/*static*/ std::string const& IResearchAnalyzerFeature::name() noexcept {
  return FEATURE_NAME;
}

/*static*/ bool IResearchAnalyzerFeature::analyzerReachableFromDb(
    irs::string_ref const& dbNameFromAnalyzer,
    irs::string_ref const& currentDbName, bool forGetters) noexcept {
  TRI_ASSERT(!currentDbName.empty());
  if (dbNameFromAnalyzer
          .null()) {  // NULL means local db name = always reachable
    return true;
  }
  if (dbNameFromAnalyzer.empty()) {  // empty name with :: means always system
    if (forGetters) {
      return true;  // system database is always accessible for reading analyzer
                    // from any other db
    }
    return currentDbName == arangodb::StaticStrings::SystemDatabase;
  }
  return currentDbName == dbNameFromAnalyzer ||
         (forGetters &&
          dbNameFromAnalyzer == arangodb::StaticStrings::SystemDatabase);
}

/*static*/ std::pair<irs::string_ref, irs::string_ref>
IResearchAnalyzerFeature::splitAnalyzerName(
    irs::string_ref const& analyzer) noexcept {
  // search for vocbase prefix ending with '::'
  for (size_t i = 1, count = analyzer.size(); i < count; ++i) {
    if (analyzer[i] == ANALYZER_PREFIX_DELIM  // current is delim
        &&
        analyzer[i - 1] == ANALYZER_PREFIX_DELIM) {  // previous is also delim
      auto vocbase =
          i > 1  // non-empty prefix, +1 for first delimiter char
              ? irs::string_ref(analyzer.c_str(),
                                i - 1)  // -1 for the first ':' delimiter
              : irs::string_ref::EMPTY;
      auto name =
          i < count - 1  // have suffix
              ? irs::string_ref(analyzer.c_str() + i + 1,
                                count - i - 1)  // +-1 for the suffix after '::'
              : irs::string_ref::EMPTY;  // do not point after end of buffer

      return std::make_pair(vocbase, name);  // prefixed analyzer name
    }
  }

  return std::make_pair(irs::string_ref::NIL,
                        analyzer);  // unprefixed analyzer name
}

/*static*/ std::string IResearchAnalyzerFeature::normalize(  // normalize name
    irs::string_ref const& name,                             // analyzer name
    irs::string_ref const&
        activeVocbase,  // fallback vocbase if not part of name
    bool expandVocbasePrefix /*= true*/) {  // use full vocbase name as prefix
                                            // for active/system v.s. EMPTY/'::'
  auto& staticAnalyzers = getStaticAnalyzers();

  if (staticAnalyzers.find(irs::make_hashed_ref(
          name, std::hash<irs::string_ref>())) != staticAnalyzers.end()) {
    return static_cast<std::string>(
        name);  // special case for singleton static analyzers
  }

  auto split = splitAnalyzerName(name);

  if (expandVocbasePrefix) {
    if (split.first.null()) {
      return normalizedAnalyzerName(static_cast<std::string>(activeVocbase),
                                    split.second);
    }

    if (split.first.empty()) {
      return normalizedAnalyzerName(arangodb::StaticStrings::SystemDatabase,
                                    split.second);
    }
  } else {
    // .........................................................................
    // normalize vocbase such that active vocbase takes precedence over system
    // vocbase i.e. prefer NIL over EMPTY
    // .........................................................................
    if (arangodb::StaticStrings::SystemDatabase == activeVocbase ||
        split.first.null() ||
        (split.first == activeVocbase)) {  // active vocbase
      return static_cast<std::string>(split.second);
    }

    if (split.first.empty() ||
        split.first ==
            arangodb::StaticStrings::SystemDatabase) {  // system vocbase
      return normalizedAnalyzerName("", split.second);
    }
  }

  return static_cast<std::string>(name);  // name prefixed with vocbase (or NIL)
}

AnalyzersRevision::Ptr IResearchAnalyzerFeature::getAnalyzersRevision(
    const irs::string_ref& vocbaseName,
    bool forceLoadPlan /* = false */) const {
  TRI_vocbase_t* vocbase{nullptr};
  auto& dbFeature = server().getFeature<DatabaseFeature>();
  vocbase = dbFeature.useDatabase(vocbaseName.empty()
                                      ? arangodb::StaticStrings::SystemDatabase
                                      : static_cast<std::string>(vocbaseName));
  if (vocbase) {
    return getAnalyzersRevision(*vocbase, forceLoadPlan);
  }
  return AnalyzersRevision::getEmptyRevision();
}

AnalyzersRevision::Ptr IResearchAnalyzerFeature::getAnalyzersRevision(
    const TRI_vocbase_t& vocbase, bool forceLoadPlan /* = false */) const {
  if (ServerState::instance()->isRunningInCluster()) {
    auto const& server = vocbase.server();
    if (server.hasFeature<ClusterFeature>()) {
      auto ptr = server.getFeature<ClusterFeature>()
                     .clusterInfo()
                     .getAnalyzersRevision(vocbase.name(), forceLoadPlan);
      // could be null if plan is still not loaded
      return ptr ? ptr : AnalyzersRevision::getEmptyRevision();
    }
  }
  return AnalyzersRevision::getEmptyRevision();
}

void IResearchAnalyzerFeature::prepare() {
  if (!isEnabled()) {
    return;
  }

  // load all known analyzers
  ::iresearch::analysis::analyzers::init();

  // load all static analyzers
  _analyzers = getStaticAnalyzers();
}

Result IResearchAnalyzerFeature::removeFromCollection(
    irs::string_ref const& name, irs::string_ref const& vocbase) {
  auto& dbFeature = server().getFeature<DatabaseFeature>();
  auto* voc = dbFeature.useDatabase(static_cast<std::string>(
      vocbase));  // FIXME: after C++20 remove cast and use heterogeneous lookup
  if (!voc) {
    return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
            "failure to find vocbase while removing arangosearch analyzer '" +
                std::string(name) + "'"};
  }
  SingleCollectionTransaction trx(transaction::StandaloneContext::Create(*voc),
                                  arangodb::StaticStrings::AnalyzersCollection,
                                  AccessMode::Type::WRITE);
  auto res = trx.begin();

  if (!res.ok()) {
    return res;
  }

  velocypack::Builder builder;
  OperationOptions options;

  builder.openObject();
  addStringRef(builder, arangodb::StaticStrings::KeyString, name);
  builder.close();

  auto result = trx.remove(arangodb::StaticStrings::AnalyzersCollection,
                           builder.slice(), options);

  if (!result.ok()) {
    trx.abort();

    return result.result;
  }

  return trx.commit();
}

Result IResearchAnalyzerFeature::finalizeRemove(
    irs::string_ref const& name, irs::string_ref const& vocbase) {
  TRI_IF_FAILURE("FinalizeAnalyzerRemove") { return Result(TRI_ERROR_DEBUG); }

  try {
    return removeFromCollection(name, vocbase);
  } catch (basics::Exception const& e) {
    return {e.code(), StringUtils::concatT(
                          "caught exception while finalizing removing "
                          "configuration for arangosearch analyzer name '",
                          std::string(name), "': ", e.code(), " ", e.what())};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while finalizing removing configuration for "
            "arangosearch analyzer name '" +
                std::string(name) + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while finalizing removing configuration for "
            "arangosearch analyzer name '" +
                std::string(name) + "'"};
  }
  return {};
}

Result IResearchAnalyzerFeature::remove(irs::string_ref const& name,
                                        bool force /*= false*/) {
  try {
    auto split = splitAnalyzerName(name);

    if (split.first.null()) {
      return {TRI_ERROR_FORBIDDEN, "built-in analyzers cannot be removed"};
    }

    // FIXME: really strange  we don`t have this load in remove
    //        This means if we just start server and call remove - we will fail
    //        Even test
    //        IResearchAnalyzerFeatureTest.test_persistence_remove_existing_records
    //        checks this behaviour Maybe this is to avoid having here something
    //        like onlyCached kludge in get ?
    // if (!split.first.null()) { // do not trigger load for static-analyzer
    // requests
    //  auto res = loadAnalyzers(split.first);

    //  if (!res.ok()) {
    //    return res;
    //  }
    //}

    WRITE_LOCKER(lock, _mutex);

    auto itr = _analyzers.find(
        irs::make_hashed_ref(name, std::hash<irs::string_ref>()));

    if (itr == _analyzers.end()) {
      return {
          TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
          "failure to find analyzer while removing arangosearch analyzer '" +
              std::string(name) + "'"};
    }

    auto& pool = itr->second;

    // this should never happen since analyzers should always be valid, but this
    // will make the state consistent again
    if (!pool) {
      _analyzers.erase(itr);
      return {TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
              "failure to find a valid analyzer while removing arangosearch "
              "analyzer '" +
                  std::string(name) + "'"};
    }

    if (!force && analyzerInUse(server(), split.first,
                                pool)) {  // +1 for ref in '_analyzers'
      return {TRI_ERROR_ARANGO_CONFLICT,
              "analyzer in-use while removing arangosearch analyzer '" +
                  std::string(name) + "'"};
    }

    // on db-server analyzers are not persisted
    // allow removal even inRecovery()
    if (ServerState::instance()->isDBServer()) {
      _analyzers.erase(itr);

      return {};
    }

    auto analyzerModificationTrx =
        createAnalyzerModificationTransaction(server(), split.first);
    if (analyzerModificationTrx) {
      auto startRes = analyzerModificationTrx->start();
      if (startRes.fail()) {
        return startRes;
      }
      auto cleanupResult = cleanupAnalyzersCollection(
          split.first, analyzerModificationTrx->buildingRevision());
      if (cleanupResult.fail()) {
        return cleanupResult;
      }
    }
    // .........................................................................
    // after here the analyzer must be removed from the persisted store first
    // .........................................................................

    // this should never happen since non-static analyzers should always have a
    // valid '_key' after
    if (pool->_key.null()) {
      return {TRI_ERROR_INTERNAL,
              "failure to find '" + arangodb::StaticStrings::KeyString +
                  "' while removing arangosearch analyzer '" +
                  std::string(name) + "'"};
    }

    auto& engine = server().getFeature<EngineSelectorFeature>().engine();

    // do not allow persistence while in recovery
    if (engine.inRecovery()) {
      return {TRI_ERROR_INTERNAL,
              "failure to remove arangosearch analyzer '" + std::string(name) +
                  "' configuration while storage engine in recovery"};
    }

    if (!analyzerModificationTrx) {
      TRI_ASSERT(ServerState::instance()->isSingleServer());
      auto commitResult = removeFromCollection(pool->_key, split.first);
      if (!commitResult.ok()) {
        return commitResult;
      }
      _analyzers.erase(itr);
    } else {
      auto& dbFeature = server().getFeature<DatabaseFeature>();

      auto* vocbase = dbFeature.useDatabase(static_cast<std::string>(
          split.first));  // FIXME: after C++20 remove cast and use
                          // heterogeneous lookup

      if (!vocbase) {
        return {
            TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
            "failure to find vocbase while removing arangosearch analyzer '" +
                std::string(name) + "'"};
      }

      SingleCollectionTransaction trx(
          transaction::StandaloneContext::Create(*vocbase),
          arangodb::StaticStrings::AnalyzersCollection,
          AccessMode::Type::WRITE);
      auto res = trx.begin();

      if (!res.ok()) {
        return res;
      }

      velocypack::Builder builder;
      OperationOptions options;

      builder.openObject();
      addStringRef(builder, arangodb::StaticStrings::KeyString, pool->_key);
      builder.add(arangodb::StaticStrings::AnalyzersDeletedRevision,
                  VPackValue(analyzerModificationTrx->buildingRevision()));
      builder.close();

      auto result = trx.update(arangodb::StaticStrings::AnalyzersCollection,
                               builder.slice(), options);

      if (!result.ok()) {
        trx.abort();

        return result.result;
      }

      TRI_IF_FAILURE("UpdateAnalyzerForRemove") {
        return Result(TRI_ERROR_DEBUG);
      }

      res = trx.commit();
      if (!res.ok()) {
        return res;
      }
      res = analyzerModificationTrx->commit();
      if (res.fail()) {
        return res;
      }

      // this is ok. As if we got  there removal is committed into agency
      _analyzers.erase(itr);
      auto cached = _lastLoad.find(static_cast<std::string>(
          split.first));  // FIXME: remove cast after C++20
      TRI_ASSERT(cached != _lastLoad.end());
      if (ADB_LIKELY(cached != _lastLoad.end())) {
        // we are hodling writelock so nobody should be able reload analyzers
        // and update cache.
        TRI_ASSERT(cached->second <
                   analyzerModificationTrx->buildingRevision());
        cached->second = analyzerModificationTrx
                             ->buildingRevision();  // as we already "updated
                                                    // cache" to this revision
      }
      res = removeFromCollection(split.second, split.first);
      if (res.fail()) {
        // just log this error. Analyzer is already "deleted" for the whole
        // cluster so we must report success. Leftovers will be cleaned up on
        // the next operation
        LOG_TOPIC("70a8b", WARN, iresearch::TOPIC)
            << " Failed to finalize analyzer '" << split.second << "'"
            << " Error Code:" << res.errorNumber()
            << " Error:" << res.errorMessage();
      }
    }
  } catch (basics::Exception const& e) {
    return {e.code(), StringUtils::concatT(
                          "caught exception while removing configuration for "
                          "arangosearch analyzer name '",
                          name, "': ", e.code(), " ", e.what())};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while removing configuration for arangosearch "
            "analyzer name '" +
                std::string(name) + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while removing configuration for arangosearch "
            "analyzer name '" +
                std::string(name) + "'"};
  }

  return {};
}

void IResearchAnalyzerFeature::start() {
  if (!isEnabled()) {
    return;
  }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // we rely on having a system database
  if (server().hasFeature<SystemDatabaseFeature>()) {
    auto vocbase = server().getFeature<SystemDatabaseFeature>().use();
    if (vocbase) {  // feature/db may be absent in some unit-test enviroment
      TRI_ASSERT(vocbase->name() == arangodb::StaticStrings::SystemDatabase);
    }
  }
#endif
  // register analyzer functions
  if (server().hasFeature<aql::AqlFunctionFeature>()) {
    addFunctions(server().getFeature<aql::AqlFunctionFeature>());
  }

  if (server().hasFeature<ClusterFeature>() &&
      server().hasFeature<SchedulerFeature>() &&  // Mostly for tests without
                                                  // scheduler
      ServerState::instance()->isCoordinator()) {
    queueGarbageCollection(_workItemMutex, _workItem, _gcfunc);
  }
}

void IResearchAnalyzerFeature::beginShutdown() {
  std::lock_guard<std::mutex> guard(_workItemMutex);
  _workItem.reset();
}

void IResearchAnalyzerFeature::stop() {
  if (!isEnabled()) {
    return;
  }

  {
    // '_analyzers' can be asynchronously read
    WRITE_LOCKER(lock, _mutex);

    _analyzers =
        getStaticAnalyzers();  // clear cache and reload static analyzers
  }
  {
    // reset again, as there may be a race between beginShutdown and
    // the execution of the deferred _workItem
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem.reset();
  }
}

Result IResearchAnalyzerFeature::storeAnalyzer(AnalyzerPool& pool) {
  TRI_IF_FAILURE("FailStoreAnalyzer") { return Result(TRI_ERROR_DEBUG); }

  try {
    auto& dbFeature = server().getFeature<DatabaseFeature>();

    if (pool.type().null()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "failure to persist arangosearch analyzer '" + pool.name() +
                  "' configuration with 'null' type"};
    }

    auto& engine = server().getFeature<EngineSelectorFeature>().engine();

    // do not allow persistence while in recovery
    if (engine.inRecovery()) {
      return {TRI_ERROR_INTERNAL,
              "failure to persist arangosearch analyzer '" + pool.name() +
                  "' configuration while storage engine in recovery"};
    }

    auto split = splitAnalyzerName(pool.name());
    auto* vocbase = dbFeature.useDatabase(static_cast<std::string>(
        split.first));  // FIXME: after C++20 remove cast and use heterogeneous
                        // lookup

    if (!vocbase) {
      return {
          TRI_ERROR_INTERNAL,
          "failure to find vocbase while persising arangosearch analyzer '" +
              pool.name() + "'"};
    }

    SingleCollectionTransaction trx(
        transaction::StandaloneContext::Create(*vocbase),
        arangodb::StaticStrings::AnalyzersCollection, AccessMode::Type::WRITE);
    auto res = trx.begin();

    if (!res.ok()) {
      return res;
    }

    velocypack::Builder builder;
    // for storing in db analyzers collection - store only analyzer name
    pool.toVelocyPack(builder, true);

    OperationOptions options;
    options.waitForSync = true;

    auto result = trx.insert(arangodb::StaticStrings::AnalyzersCollection,
                             builder.slice(), options);

    if (!result.ok()) {
      trx.abort();

      return result.result;
    }

    auto slice = result.slice();

    if (!slice.isObject()) {
      return {TRI_ERROR_INTERNAL,
              "failure to parse result as a JSON object while persisting "
              "configuration for arangosearch analyzer name '" +
                  pool.name() + "'"};
    }

    auto key = slice.get(arangodb::StaticStrings::KeyString);

    if (!key.isString()) {
      return {TRI_ERROR_INTERNAL,
              "failure to find the resulting key field while persisting "
              "configuration for arangosearch analyzer name '" +
                  pool.name() + "'"};
    }

    res = trx.commit();

    if (!res.ok()) {
      trx.abort();

      return res;
    }

    pool.setKey(getStringRef(key));
  } catch (basics::Exception const& e) {
    return {e.code(), StringUtils::concatT(
                          "caught exception while persisting configuration for "
                          "arangosearch analyzer name '",
                          pool.name(), "': ", e.code(), " ", e.what())};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while persisting configuration for arangosearch "
            "analyzer name '" +
                pool.name() + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while persisting configuration for arangosearch "
            "analyzer name '" +
                pool.name() + "'"};
  }

  return {};
}

bool IResearchAnalyzerFeature::visit(
    std::function<bool(AnalyzerPool::ptr const&)> const& visitor) const {
  Analyzers analyzers;

  {
    READ_LOCKER(lock, _mutex);
    analyzers = _analyzers;
  }

  for (auto& entry : analyzers) {
    if (entry.second && !visitor(entry.second)) {
      return false;  // termination request
    }
  }

  return true;
}

bool IResearchAnalyzerFeature::visit(
    std::function<bool(AnalyzerPool::ptr const&)> const& visitor,
    TRI_vocbase_t const* vocbase) const {
  // static analyzer visitation
  if (!vocbase) {
    for (auto& entry : getStaticAnalyzers()) {
      if (entry.second && !visitor(entry.second)) {
        return false;  // termination request
      }
    }

    return true;
  }

  auto res = const_cast<IResearchAnalyzerFeature*>(this)->loadAnalyzers(
      vocbase->name());

  if (!res.ok()) {
    LOG_TOPIC("73695", WARN, iresearch::TOPIC)
        << "failure to load analyzers while visiting database '"
        << vocbase->name() << "': " << res.errorNumber() << " "
        << res.errorMessage();
    TRI_set_errno(res.errorNumber());

    return false;
  }

  Analyzers analyzers;

  {
    READ_LOCKER(lock, _mutex);
    analyzers = _analyzers;
  }

  for (auto& entry : analyzers) {
    if (entry.second  // have entry
        && (splitAnalyzerName(entry.first).first ==
            vocbase->name())       // requested vocbase
        && !visitor(entry.second)  // termination request
    ) {
      return false;
    }
  }

  return true;
}

void IResearchAnalyzerFeature::cleanupAnalyzers(
    irs::string_ref const& database) {
  if (ADB_UNLIKELY(database.empty())) {
    TRI_ASSERT(FALSE);
    return;
  }
  for (auto itr = _analyzers.begin(), end = _analyzers.end(); itr != end;) {
    auto split = splitAnalyzerName(itr->first);
    if (split.first == database) {
      itr = _analyzers.erase(itr);
    } else {
      ++itr;
    }
  }
}

void IResearchAnalyzerFeature::invalidate(const TRI_vocbase_t& vocbase) {
  WRITE_LOCKER(lock, _mutex);
  auto database = irs::string_ref(vocbase.name());
  auto itr = _lastLoad.find(
      static_cast<std::string>(  // FIXME: after C++20 remove cast and use
                                 // heterogeneous lookup
          irs::make_hashed_ref(database, std::hash<irs::string_ref>())));
  if (itr != _lastLoad.end()) {
    cleanupAnalyzers(database);
    _lastLoad.erase(itr);
  }
}

void Features::visit(std::function<void(std::string_view)> visitor) const {
  if (irs::IndexFeatures::FREQ == (_indexFeatures & irs::IndexFeatures::FREQ)) {
    visitor(irs::type<irs::frequency>::name());
  }
  if (irs::IndexFeatures::POS == (_indexFeatures & irs::IndexFeatures::POS)) {
    visitor(irs::type<irs::position>::name());
  }
  if (FieldFeatures::NORM == (_fieldFeatures & FieldFeatures::NORM)) {
    visitor(irs::type<irs::norm>::name());
  }
}

std::vector<irs::type_info::type_id> Features::fieldFeatures(
    LinkVersion version) const {
  if (_fieldFeatures == FieldFeatures::NONE) {
    return {};
  }

  return {version > LinkVersion::MIN ? irs::type<irs::norm2>::id()
                                     : irs::type<irs::norm>::id()};
}

bool Features::add(irs::string_ref featureName) {
  if (featureName == irs::type<irs::position>::name()) {
    _indexFeatures |= irs::IndexFeatures::POS;
    return true;
  }

  if (featureName == irs::type<irs::frequency>::name()) {
    _indexFeatures |= irs::IndexFeatures::FREQ;
    return true;
  }

  if (featureName == irs::type<irs::norm>::name()) {
    _fieldFeatures |= FieldFeatures::NORM;
    return true;
  }

  return false;
}

Result Features::validate() const {
  if (irs::IndexFeatures::POS == (_indexFeatures & irs::IndexFeatures::POS)) {
    if (irs::IndexFeatures::FREQ !=
        (_indexFeatures & irs::IndexFeatures::FREQ)) {
      return {TRI_ERROR_BAD_PARAMETER,
              "missing feature 'frequency' required when 'position' feature is "
              "specified"};
    }
  }

  if ((irs::IndexFeatures::POS | irs::IndexFeatures::FREQ) !=
      (_indexFeatures | irs::IndexFeatures::POS | irs::IndexFeatures::FREQ)) {
    return {TRI_ERROR_BAD_PARAMETER,
            "Unsupported index features are specified: "s +
                std::to_string(
                    static_cast<std::underlying_type_t<irs::IndexFeatures>>(
                        _indexFeatures))};
  }

  return {};
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
