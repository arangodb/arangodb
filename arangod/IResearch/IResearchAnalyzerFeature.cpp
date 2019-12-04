////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
#undef NOEXCEPT
#endif

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/text_token_stream.hpp"
#include "analysis/delimited_token_stream.hpp"
#include "analysis/ngram_token_stream.hpp"
#include "analysis/text_token_stemming_stream.hpp"
#include "analysis/text_token_normalizing_stream.hpp"
#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

#include "ApplicationServerHelper.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "IResearchAnalyzerFeature.h"
#include "IResearchLink.h"
#include "IResearchCommon.h"
#include "Logger/LogMacros.h"
#include "RestHandler/RestVocbaseBaseHandler.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VelocyPackHelper.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"
#include "VocBase/Methods/Collections.h"

namespace iresearch {
namespace text_format {

static const irs::text_format::type_id VPACK("vpack");

const type_id& vpack_t() { return VPACK; }

} // iresearch
} // text_format

namespace {

using namespace std::literals::string_literals;

static char const ANALYZER_PREFIX_DELIM = ':'; // name prefix delimiter (2 chars)
static size_t const ANALYZER_PROPERTIES_SIZE_MAX = 1024 * 1024; // arbitrary value
static size_t const DEFAULT_POOL_SIZE = 8;  // arbitrary value
static std::string const FEATURE_NAME("ArangoSearchAnalyzer");
static irs::string_ref const IDENTITY_ANALYZER_NAME("identity");
static auto const RELOAD_INTERVAL = std::chrono::seconds(60); // arbitrary value

bool normalize(std::string& out,
               irs::string_ref const& type,
               VPackSlice const properties) {
  if (type.empty()) {
    // in ArangoSearch we don't allow to have analyzers with empty type string
    return false;
  }

  // for API consistency we only support analyzers configurable via jSON
  return irs::analysis::analyzers::normalize(
    out, type,
    irs::text_format::vpack,
    arangodb::iresearch::ref<char>(properties),
    false);
}

class IdentityAnalyzer final : public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();

  static bool normalize(const irs::string_ref& /*args*/, std::string& out) {
    out.resize(VPackSlice::emptyObjectSlice().byteSize());
    std::memcpy(&out[0], VPackSlice::emptyObjectSlice().begin(), out.size());
    return true;
  }

  static irs::analysis::analyzer::ptr make(irs::string_ref const& /*args*/) {
    return std::make_shared<IdentityAnalyzer>();
  }

  IdentityAnalyzer()
    : irs::analysis::analyzer(IdentityAnalyzer::type()),
      _empty(true) {
    _attrs.emplace(_term);
    _attrs.emplace(_inc);
  }

  virtual irs::attribute_view const& attributes() const noexcept override {
    return _attrs;
  }

  virtual bool next() noexcept override {
    auto empty = _empty;

    _empty = true;

    return !empty;
  }

  virtual bool reset(irs::string_ref const& data) noexcept override {
    _empty = false;
    _term.value(irs::ref_cast<irs::byte_type>(data));

    return true;
  }

 private:
  struct IdentityValue : irs::term_attribute {
    void value(irs::bytes_ref const& data) noexcept {
      value_ = data;
    }
  };

  irs::attribute_view _attrs;
  IdentityValue _term;
  irs::increment _inc;
  bool _empty;
}; // IdentityAnalyzer

DEFINE_ANALYZER_TYPE_NAMED(IdentityAnalyzer, IDENTITY_ANALYZER_NAME);
REGISTER_ANALYZER_VPACK(IdentityAnalyzer, IdentityAnalyzer::make, IdentityAnalyzer::normalize);


// Delimiter analyzer vpack routines ////////////////////////////
namespace delimiter_vpack {
const irs::string_ref DELIMITER_PARAM_NAME = "delimiter";

bool parse_delimiter_vpack_config(const irs::string_ref& args, std::string& delimiter) {
  auto slice = arangodb::iresearch::slice<char>(args);
  if (slice.isString()) {
    delimiter = arangodb::iresearch::getStringRef(slice);
    return true;
  } else if (slice.isObject()) {
    bool seen = false;
    return arangodb::iresearch::getString(delimiter, slice,
                                          DELIMITER_PARAM_NAME, seen, "") &&
           seen;
  }
  LOG_TOPIC("4342c", WARN, arangodb::iresearch::TOPIC) <<
    "Missing '" << DELIMITER_PARAM_NAME << "' while constructing delimited_token_stream from jSON "
    "arguments: " << slice.toString();
  return false;
}

irs::analysis::analyzer::ptr delimiter_vpack_builder(irs::string_ref const& args) {
  std::string delimiter;
  if (parse_delimiter_vpack_config(args, delimiter)) {
    return irs::analysis::delimited_token_stream::make(delimiter);
  }
  return nullptr;
}

bool delimiter_vpack_normalizer(const irs::string_ref& args, std::string& out) {
  std::string tmp;
  if (parse_delimiter_vpack_config(args, tmp)) {
    VPackBuilder vpack;
    {
      VPackObjectBuilder scope(&vpack);
      arangodb::iresearch::addStringRef(vpack, DELIMITER_PARAM_NAME, tmp);
    }
    out.resize(vpack.slice().byteSize());
    std::memcpy(&out[0], vpack.slice().begin(), out.size());
    return true;
  }
  return false;
}

REGISTER_ANALYZER_VPACK(irs::analysis::delimited_token_stream,
                        delimiter_vpack_builder, delimiter_vpack_normalizer);
}  // namespace delimiter_vpack
namespace ngram_vpack {
const irs::string_ref MIN_PARAM_NAME = "min";
const irs::string_ref MAX_PARAM_NAME = "max";
const irs::string_ref PRESERVE_ORIGINAL_PARAM_NAME = "preserveOriginal";

bool parse_ngram_vpack_config(const irs::string_ref& args, irs::analysis::ngram_token_stream::options_t& options) {
  auto slice = arangodb::iresearch::slice<char>(args);

  if (!slice.isObject()) {
    LOG_TOPIC("c0168", WARN, arangodb::iresearch::TOPIC)
      << "Not a jSON object passed while constructing ngram_token_stream, "
      "arguments: " << slice.toString();
    return false;
  }

  uint64_t min = 0, max = 0;
  bool seen = false;
  if (!arangodb::iresearch::getNumber(min, slice, MIN_PARAM_NAME, seen, min) || !seen) {
    LOG_TOPIC("7b706", WARN, arangodb::iresearch::TOPIC)
      << "Failed to read '" << MIN_PARAM_NAME
      << "' attribute as number while constructing "
         "ngram_token_stream from jSON arguments: "
      << slice.toString();
    return false;
  }

  if (!arangodb::iresearch::getNumber(max, slice, MAX_PARAM_NAME, seen, max) || !seen) {
    LOG_TOPIC("eae46", WARN, arangodb::iresearch::TOPIC)
      << "Failed to read '" << MAX_PARAM_NAME
      << "' attribute as number while constructing "
      "ngram_token_stream from jSON arguments: "
      << slice.toString();
    return false;
  }

  if (!slice.hasKey(PRESERVE_ORIGINAL_PARAM_NAME) ||
      !slice.get(PRESERVE_ORIGINAL_PARAM_NAME).isBool()) {
    LOG_TOPIC("e95d3", WARN, arangodb::iresearch::TOPIC)
      << "Failed to read '" << PRESERVE_ORIGINAL_PARAM_NAME
      << "' attribute as boolean while constructing "
      "ngram_token_stream from jSON arguments: "
      << slice.toString();
    return false;
  }
  options.min_gram = min;
  options.max_gram = max;
  options.preserve_original = slice.get(PRESERVE_ORIGINAL_PARAM_NAME).getBool();
  return true;
}


irs::analysis::analyzer::ptr ngram_vpack_builder(irs::string_ref const& args) {
  irs::analysis::ngram_token_stream::options_t tmp;
  if (parse_ngram_vpack_config(args, tmp)) {
    return irs::analysis::ngram_token_stream::make(tmp);
  }
  return nullptr;
}


bool ngram_vpack_normalizer(const irs::string_ref& args, std::string& out) {
  irs::analysis::ngram_token_stream::options_t tmp;
  if (parse_ngram_vpack_config(args, tmp)) {
    VPackBuilder vpack;
    {
      VPackObjectBuilder scope(&vpack);
      vpack.add(MIN_PARAM_NAME, VPackValue(tmp.min_gram));
      vpack.add(MAX_PARAM_NAME, VPackValue(tmp.max_gram));
      vpack.add(PRESERVE_ORIGINAL_PARAM_NAME, VPackValue(tmp.preserve_original));
    }
    out.assign(vpack.slice().startAs<char>(), vpack.slice().byteSize());
    return true;
  }
  return false;
}

REGISTER_ANALYZER_VPACK(irs::analysis::ngram_token_stream,
    ngram_vpack_builder, ngram_vpack_normalizer);
}  // namespace ngram_vpack

namespace text_vpack {
// FIXME implement proper vpack parsing
irs::analysis::analyzer::ptr text_vpack_builder(irs::string_ref const& args) {
  auto slice = arangodb::iresearch::slice<char>(args);
  if (!slice.isNone()) {
    return irs::analysis::analyzers::get("text", irs::text_format::json,
      slice.toString(),
      false);
  }
  return nullptr;
}

bool text_vpack_normalizer(const irs::string_ref& args, std::string& out) {
  std::string tmp;
  auto slice = arangodb::iresearch::slice<char>(args);

  if (!slice.isNone() &&
      irs::analysis::analyzers::normalize(tmp, "text", irs::text_format::json,
                                          slice.toString(),
                                          false)) {
    auto vpack = VPackParser::fromJson(tmp);
    out.assign(vpack->slice().startAs<char>(), vpack->slice().byteSize());
    return true;
  }
  return false;
}

REGISTER_ANALYZER_VPACK(irs::analysis::text_token_stream, text_vpack_builder,
                        text_vpack_normalizer);
}

namespace stem_vpack {
  // FIXME implement proper vpack parsing
  irs::analysis::analyzer::ptr stem_vpack_builder(irs::string_ref const& args) {
    auto slice = arangodb::iresearch::slice<char>(args);
    if (!slice.isNone()) {
      return irs::analysis::analyzers::get("stem", irs::text_format::json,
        slice.toString(),
        false);
    }
    return nullptr;
  }

  bool stem_vpack_normalizer(const irs::string_ref& args, std::string& out) {
    std::string tmp;
    auto slice = arangodb::iresearch::slice<char>(args);
    if (!slice.isNone() &&
        irs::analysis::analyzers::normalize(tmp, "stem", irs::text_format::json,
      slice.toString(), false)) {
      auto vpack = VPackParser::fromJson(tmp);
      out.assign(vpack->slice().startAs<char>(), vpack->slice().byteSize());
      return true;
    }
    return false;
  }

  REGISTER_ANALYZER_VPACK(irs::analysis::text_token_stemming_stream, stem_vpack_builder,
    stem_vpack_normalizer);
}

namespace norm_vpack {
  // FIXME implement proper vpack parsing
  irs::analysis::analyzer::ptr norm_vpack_builder(irs::string_ref const& args) {
    auto slice = arangodb::iresearch::slice<char>(args);
    if (!slice.isNone()) {//cannot be created without properties
      return irs::analysis::analyzers::get("norm", irs::text_format::json,
        slice.toString(),
        false);
    }
    return nullptr;
  }

  bool norm_vpack_normalizer(const irs::string_ref& args, std::string& out) {
    std::string tmp;
    auto slice = arangodb::iresearch::slice<char>(args);
    if (!slice.isNone() && //cannot be created without properties
        irs::analysis::analyzers::normalize(tmp, "norm", irs::text_format::json,
      slice.toString(), false)) {
      auto vpack = VPackParser::fromJson(tmp);
      out.assign(vpack->slice().startAs<char>(), vpack->slice().byteSize());
      return true;
    }
    return false;
  }

  REGISTER_ANALYZER_VPACK(irs::analysis::text_token_normalizing_stream, norm_vpack_builder,
    norm_vpack_normalizer);
}

arangodb::aql::AqlValue aqlFnTokens(arangodb::aql::ExpressionContext* expressionContext,
                                    arangodb::transaction::Methods* trx,
                                    arangodb::aql::VPackFunctionParameters const& args) {
  if (2 != args.size() || !args[0].isString() || !args[1].isString()) {
    irs::string_ref const message = "invalid arguments while computing result for function 'TOKENS'";

    LOG_TOPIC("740fd", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
  }

  auto data = arangodb::iresearch::getStringRef(args[0].slice());
  auto name = arangodb::iresearch::getStringRef(args[1].slice());
  auto* analyzers =
      arangodb::application_features::ApplicationServer::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

  arangodb::iresearch::AnalyzerPool::ptr pool;

  if (trx) {
    auto* sysDatabase = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
      arangodb::SystemDatabaseFeature // featue type
    >();

    auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;

    if (sysVocbase) {
      pool = analyzers->get(name, trx->vocbase(), *sysVocbase);
    }
  } else {
    pool = analyzers->get(name); // verbatim
  }

  if (!pool) {
    auto const message = "failure to find arangosearch analyzer with name '"s
      + static_cast<std::string>(name)
      + "' while computing result for function 'TOKENS'";

    LOG_TOPIC("0d256", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
  }

  auto analyzer = pool->get();

  if (!analyzer) {
    auto const message = "failure to find arangosearch analyzer with name '"s
      + static_cast<std::string>(name)
      + "' while computing result for function 'TOKENS'";
    LOG_TOPIC("d7477", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
  }

  if (!analyzer->reset(data)) {
    auto const message = "failure to reset arangosearch analyzer: ' "s
      + static_cast<std::string>(name)
      + "' while computing result for function 'TOKENS'";

    LOG_TOPIC("45a2d", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }

  auto& values = analyzer->attributes().get<irs::term_attribute>();

  if (!values) {
    auto const message =
        "failure to retrieve values from arangosearch analyzer name '"s
        + static_cast<std::string>(name)
        + "' while computing result for function 'TOKENS'";

    LOG_TOPIC("f46f2", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }

  // to avoid copying Builder's default buffer when initializing AqlValue
  // create the buffer externally and pass ownership directly into AqlValue
  auto buffer = irs::memory::make_unique<arangodb::velocypack::Buffer<uint8_t>>();

  if (!buffer) {
    irs::string_ref const message = "failure to allocate result buffer while "
                                    "computing result for function 'TOKENS'";

    LOG_TOPIC("97cd0", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_OUT_OF_MEMORY, message);
  }

  arangodb::velocypack::Builder builder(*buffer);

  builder.openArray();

  while (analyzer->next()) {
    auto value = irs::ref_cast<char>(values->value());

    arangodb::iresearch::addStringRef(builder, value);
  }

  builder.close();

  bool bufOwner = true;  // out parameter from AqlValue denoting ownership
                         // aquisition (must be true initially)
  auto release = irs::make_finally([&buffer, &bufOwner]() -> void {
    if (!bufOwner) {
      buffer.release();
    }
  });

  return arangodb::aql::AqlValue(buffer.get(), bufOwner);
}

void addFunctions(arangodb::aql::AqlFunctionFeature& functions) {
  arangodb::iresearch::addFunction(
      functions,
      arangodb::aql::Function{
          "TOKENS",  // name
          ".,.",     // positional arguments (data,analyzer)
          // deterministic (true == called during AST optimization and will be
          // used to calculate values for constant expressions)
          arangodb::aql::Function::makeFlags(arangodb::aql::Function::Flags::Deterministic,
                                             arangodb::aql::Function::Flags::Cacheable,
                                             arangodb::aql::Function::Flags::CanRunOnDBServer),
          &aqlFnTokens  // function implementation
      });
}

////////////////////////////////////////////////////////////////////////////////
/// @return pool will generate analyzers as per supplied parameters
////////////////////////////////////////////////////////////////////////////////
bool equalAnalyzer(
    arangodb::iresearch::AnalyzerPool const& pool,
    irs::string_ref const& type,
    VPackSlice const properties,
    irs::flags const& features) {
  std::string normalizedProperties;

  if (!::normalize(normalizedProperties, type, properties)) {
    // failed to normalize definition
    LOG_TOPIC("dfac1", WARN, arangodb::iresearch::TOPIC)
      << "failed to normalize properties for analyzer type '" << type << "' properties '"
      << properties.toString() << "'";
    return false;
  }

  // first check non-normalizeable portion of analyzer definition
  // to rule out need to check normalization of properties
  if (type != pool.type() || features != pool.features()) {
    return false;
  }

  // this check is not final as old-normalized definition may be present in database!
  if (arangodb::basics::VelocyPackHelper::equal(arangodb::iresearch::slice(normalizedProperties),
                                                pool.properties(), false)) {
    return true;
  }

  // Here could be analyzer definition with old-normalized properties (see Issue #9652)
  // To make sure properties really differ, let`s re-normalize and re-check
  std::string reNormalizedProperties;
  if (ADB_UNLIKELY(!::normalize(reNormalizedProperties, pool.type(), pool.properties()))) {
    // failed to re-normalize definition - strange. It was already normalized once.
    // Some bug in load/store?
    TRI_ASSERT(FALSE);
    LOG_TOPIC("a4073", WARN, arangodb::iresearch::TOPIC)
        << "failed to re-normalize properties for analyzer type '"
        << pool.type()
        << "' properties '" << pool.properties().toString() << "'";
    return false;
  }
  return arangodb::basics::VelocyPackHelper::equal(
      arangodb::iresearch::slice(normalizedProperties),
      arangodb::iresearch::slice(reNormalizedProperties), false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the collection containing analyzer definitions (if found)
///        taken from vocabse for single-server and from ClusterInfo on cluster
/// @note cannot use arangodb::methods::Collections::lookup(...) since it will
///       try to resolve via vocbase for the case of db-server
/// @note cannot use arangodb::CollectionNameResolver::getCollection(...) since
///       it will try to resolve via vocbase for the case of db-server
////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<arangodb::LogicalCollection> getAnalyzerCollection(
    TRI_vocbase_t const& vocbase) {
  if (arangodb::ServerState::instance()->isSingleServer()) {
    return vocbase.lookupCollection(arangodb::StaticStrings::AnalyzersCollection);
  }

  auto* ci = arangodb::ClusterInfo::instance();

  if (ci) {
    return ci->getCollectionNT(vocbase.name(), arangodb::StaticStrings::AnalyzersCollection);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read analyzers from vocbase
/// @return visitation completed fully
////////////////////////////////////////////////////////////////////////////////
arangodb::Result visitAnalyzers(
    TRI_vocbase_t& vocbase,
    std::function<arangodb::Result(VPackSlice const&)> const& visitor) {
  static const auto resultVisitor = [](
      std::function<arangodb::Result(VPackSlice const&)> const& visitor,
      TRI_vocbase_t const& vocbase,
      VPackSlice const& slice) -> arangodb::Result {
    if (!slice.isArray()) {
      return {
        TRI_ERROR_INTERNAL,
        "failed to parse contents of collection '" + arangodb::StaticStrings::AnalyzersCollection +
        "' in database '" + vocbase.name() + " while visiting analyzers" };
    }

    for (VPackArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const res = visitor(itr.value().resolveExternal());

      if (!res.ok()) {
        return res;
      }
    }

    return {};
  };

  static const auto queryString = arangodb::aql::QueryString(
    "FOR d IN " + arangodb::StaticStrings::AnalyzersCollection + " RETURN d");

  if (arangodb::ServerState::instance()->isDBServer()) {
    auto cc = arangodb::ClusterComm::instance();

    if (!cc) {
      return {
        TRI_ERROR_INTERNAL,
        "failure to find 'ClusterComm' instance while visiting Analyzer collection '" +
        arangodb::StaticStrings::AnalyzersCollection +
        "' in vocbase '" + vocbase.name() + "'"
      };
    }

    auto ci = arangodb::ClusterInfo::instance();

    if (!ci) {
      return {
        TRI_ERROR_INTERNAL,
        "failure to find 'ClusterInfo' instance while visiting Analyzer collection '" +
        arangodb::StaticStrings::AnalyzersCollection +
        "' in vocbase '" + vocbase.name() + "'"
      };
    }

    static const std::string BODY("{ \"query\" : \"" + queryString.string() + "\" }");
    auto const coords = ci->getCurrentCoordinators();

    std::vector<arangodb::ClusterCommRequest> requests(1);
    auto& request = requests[0];
    request.path = "/_api/cursor";
    request.body = std::shared_ptr<std::string const>(std::shared_ptr<std::string const>(), &BODY);
    request.requestType = arangodb::rest::RequestType::POST;

    arangodb::Result res;
    for (auto const& coord : coords) {
      request.destination = "server:" + coord;

      // same timeout as in ClusterMethods::getDocumentOnCoordinator()
      static double const CL_DEFAULT_TIMEOUT = 120.0;
      cc->performRequests(requests, CL_DEFAULT_TIMEOUT, arangodb::iresearch::TOPIC, false, false);

      if (TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == request.result.errorCode) {
        // no "_analyzers" collection => not an error
        return {};
      }

      if (TRI_ERROR_CLUSTER_TIMEOUT == request.result.errorCode) {
        res = { request.result.errorCode, request.result.errorMessage };
        continue; // try another coordinator
      }

      if (TRI_ERROR_NO_ERROR != request.result.errorCode) {
        return { request.result.errorCode, request.result.errorMessage };
      }

      if (!request.result.answer) {
        return { TRI_ERROR_INTERNAL,
                 "failed to get answer from 'ClusterComm' instance while visiting Analyzer collection '" +
                 arangodb::StaticStrings::AnalyzersCollection + "' in vocbase '" + vocbase.name() + "'" };
      }

      auto slice = request.result.answer->payload();

      if (!slice.hasKey("result")) {
        return { TRI_ERROR_INTERNAL,
                 "failed to parse result from 'ClusterComm' instance while visiting Analyzer collection '" +
                 arangodb::StaticStrings::AnalyzersCollection + "' in vocbase '" + vocbase.name() + "'" };
      }

      res = resultVisitor(visitor, vocbase, slice.get("result"));

      break;
    }

    return res;
  }

  arangodb::aql::Query query(false, vocbase, queryString,
                             nullptr, nullptr, arangodb::aql::PART_MAIN);

  auto* queryRegistry = arangodb::QueryRegistryFeature::registry();

  if (!queryRegistry) {
    return { TRI_ERROR_INTERNAL, "QueryRegistry is not set" };
  }

  auto result = query.executeSync(queryRegistry);

  if (TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == result.result.errorNumber()) {
    return {}; // treat missing collection as if there are no analyzers
  }

  if (result.result.fail()) {
    return result.result;
  }

  auto slice = result.data->slice();

  return resultVisitor(visitor, vocbase, slice);
}

inline std::string normalizedAnalyzerName(
    std::string database,
    irs::string_ref const& analyzer) {
  return database.append(2, ANALYZER_PREFIX_DELIM).append(analyzer);
}

bool analyzerInUse(irs::string_ref const& dbName,
                   arangodb::iresearch::AnalyzerPool::ptr const& analyzerPtr) {
  using arangodb::application_features::ApplicationServer;

  TRI_ASSERT(analyzerPtr);

  if (analyzerPtr.use_count() > 1) { // +1 for ref in '_analyzers'
    return true;
  }

  auto* server = arangodb::ServerState::instance();

  if (!server) {
    // no server state
    return false;
  }

  auto checkDatabase = [server, analyzer = analyzerPtr.get()](TRI_vocbase_t* vocbase) {
    if (!vocbase) {
      // no database
      return false;
    }

    std::vector<std::shared_ptr<arangodb::LogicalCollection>> collections;

    if (server->isCoordinator()) {
      auto* ci = arangodb::ClusterInfo::instance();

      if (ci) {
        collections = ci->getCollections(vocbase->name());
      }
    } else {
      collections = vocbase->collections(false);
    }

    for (auto const& collection : collections) {
      for (auto const& index : collection->getIndexes()) {
        if (!index || arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
          continue;  // not an IResearchLink
        }

        // TODO FIXME find a better way to retrieve an iResearch Link
        // cannot use static_cast/reinterpret_cast since Index is not related to
        // IResearchLink
        auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);

        if (!link) {
          continue;
        }

        if (nullptr != link->findAnalyzer(*analyzer)) {
          // found referenced analyzer
          return true;
        }
      }
    }

    return false;
  };

  TRI_vocbase_t* vocbase{};

  // check analyzer database
  auto* dbFeature = ApplicationServer::lookupFeature<arangodb::DatabaseFeature>("Database");

  if (dbFeature) {
    vocbase = dbFeature->lookupDatabase(dbName);

    if (checkDatabase(vocbase)) {
      return true;
    }
  }

  // check system database if necessary
  auto* sysDbFeature = ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>("SystemDatabase");

  if (sysDbFeature) {
    auto sysVocbase = sysDbFeature->use();

    if (sysVocbase.get() != vocbase && checkDatabase(sysVocbase.get())) {
      return true;
    }
  }

  return false;
}

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

} // namespace

namespace arangodb {
namespace iresearch {

void AnalyzerPool::toVelocyPack(
    VPackBuilder& builder,
    irs::string_ref const& name) {
  TRI_ASSERT(builder.isOpenObject());
  addStringRef(builder, StaticStrings::AnalyzerNameField, name);
  addStringRef(builder, StaticStrings::AnalyzerTypeField, type());
  builder.add(StaticStrings::AnalyzerPropertiesField, properties());

  // add features
  VPackArrayBuilder featuresScope(&builder, StaticStrings::AnalyzerFeaturesField);
  for (auto& feature: features()) {
    TRI_ASSERT(feature); // has to be non-nullptr
    addStringRef(builder, feature->name());
  }
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
        name = irs::string_ref(split.second.c_str()-2, split.second.size()+2);
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
  }

  toVelocyPack(builder, name);
}

/*static*/ AnalyzerPool::Builder::ptr
AnalyzerPool::Builder::make(
    irs::string_ref const& type,
    VPackSlice properties) {
  if (type.empty()) {
    // in ArangoSearch we don't allow to have analyzers with empty type string
    return nullptr;
  }

  // for API consistency we only support analyzers configurable via jSON
  return irs::analysis::analyzers::get(
    type, irs::text_format::vpack, iresearch::ref<char>(properties), false);
}

AnalyzerPool::AnalyzerPool(irs::string_ref const& name)
  : _cache(DEFAULT_POOL_SIZE),
    _name(name) {
}

bool AnalyzerPool::operator==(AnalyzerPool const& rhs) const {
  return _name == rhs._name &&
      _type == rhs._type &&
      _features == rhs._features &&
      basics::VelocyPackHelper::equal(_properties, rhs._properties, true);
}

bool AnalyzerPool::init(
    irs::string_ref const& type,
    VPackSlice const properties,
    irs::flags const& features /*= irs::flags::empty_instance()*/) {
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
      _properties = VPackSlice::noneSlice();
      _type = irs::string_ref::NIL;
      _key = irs::string_ref::NIL;

      _properties = iresearch::slice(_config);

      if (!type.null()) {
        _config.append(type);
        _type = irs::string_ref(_config.c_str() + _properties.byteSize() , type.size());
      }

      _features = features;  // store only requested features

      return true;
    }
  } catch (basics::Exception& e) {
    LOG_TOPIC("62062", WARN, iresearch::TOPIC)
        << "caught exception while initializing an arangosearch analizer type '" << _type
        << "' properties '" << _properties << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC("a9196", WARN, iresearch::TOPIC)
        << "caught exception while initializing an arangosearch analizer type '"
        << _type << "' properties '" << _properties << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC("7524a", WARN, iresearch::TOPIC)
        << "caught exception while initializing an arangosearch analizer type '"
        << _type << "' properties '" << _properties << "'";
    IR_LOG_EXCEPTION();
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
    _type = irs::string_ref(_config.c_str() + _properties.byteSize(), _type.size());
  }

  _key = irs::string_ref(_config.c_str() + keyOffset, key.size());
}

irs::analysis::analyzer::ptr AnalyzerPool::get() const noexcept {
  try {
    // FIXME do not use shared_ptr
    return _cache.emplace(_type, _properties).release();
  } catch (basics::Exception const& e) {
    LOG_TOPIC("c9256", WARN, iresearch::TOPIC)
        << "caught exception while instantiating an arangosearch analizer type "
           "'"
        << _type << "' properties '" << _properties << "': " << e.code() << " "
        << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC("93baf", WARN, iresearch::TOPIC)
        << "caught exception while instantiating an arangosearch analizer type "
           "'"
        << _type << "' properties '" << _properties << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC("08db9", WARN, iresearch::TOPIC)
        << "caught exception while instantiating an arangosearch analizer type "
           "'"
        << _type << "' properties '" << _properties << "'";
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

IResearchAnalyzerFeature::IResearchAnalyzerFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, IResearchAnalyzerFeature::name()) {
  setOptional(true);
  startsAfter("V8Phase");
  // used for registering IResearch analyzer functions
  startsAfter("AQLFunctions");
  // used for getting the system database
  // containing the persisted configuration
  startsAfter("SystemDatabase");
  startsAfter("CommunicationPhase");
  startsAfter("Aql");
  startsAfter("OptimizerRules");
  startsAfter("QueryRegistry");
}

/*static*/ bool IResearchAnalyzerFeature::canUse( // check permissions
    TRI_vocbase_t const& vocbase, // analyzer vocbase
    auth::Level const& level // access level
) {
  auto* ctx = ExecContext::CURRENT;

  return !ctx // authentication not enabled
    || (ctx->canUseDatabase(vocbase.name(), level) // can use vocbase
        && (ctx->canUseCollection(vocbase.name(), arangodb::StaticStrings::AnalyzersCollection, level)) // can use analyzers
       );
}

/*static*/ bool IResearchAnalyzerFeature::canUse( // check permissions
  irs::string_ref const& name, // analyzer name (already normalized)
  auth::Level const& level // access level
) {
  auto* ctx = ExecContext::CURRENT;

  if (!ctx) {
    return true; // authentication not enabled
  }

  auto& staticAnalyzers = getStaticAnalyzers();

  if (staticAnalyzers.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>())) != staticAnalyzers.end()) {
    return true; // special case for singleton static analyzers (always allowed)
  }

  auto split = splitAnalyzerName(name);

  return split.first.null() // static analyzer (always allowed)
    || (ctx->canUseDatabase(split.first, level) // can use vocbase
        && ctx->canUseCollection(split.first, arangodb::StaticStrings::AnalyzersCollection, level) // can use analyzers
       );
}


/*static*/ Result IResearchAnalyzerFeature::createAnalyzerPool(
    AnalyzerPool::ptr& pool,
    irs::string_ref const& name,
    irs::string_ref const& type,
    VPackSlice const properties,
    irs::flags const& features) {
  // check type available
  if (!irs::analysis::analyzers::exists(type, irs::text_format::vpack, false)) {
    return {
      TRI_ERROR_NOT_IMPLEMENTED,
      "Not implemented analyzer type '" + std::string(type) + "'."
    };
  }

  // validate analyzer name
  auto split = splitAnalyzerName(name);

  if (!TRI_vocbase_t::IsAllowedName(false, velocypack::StringRef(split.second.c_str(), split.second.size()))) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid characters in analyzer name '") + std::string(split.second) + "'"
    };
  }

  // validate that features are supported by arangod an ensure that their
  // dependencies are met
  for(auto& feature: features) {
    if (&irs::frequency::type() == feature) {
      // no extra validation required
    } else if (&irs::norm::type() == feature) {
      // no extra validation required
    } else if (&irs::position::type() == feature) {
      if (!features.check(irs::frequency::type())) {
        return {
          TRI_ERROR_BAD_PARAMETER,
          "missing feature '" + std::string(irs::frequency::type().name()) +
          "' required when '" + std::string(feature->name()) + "' feature is specified"
        };
      }
    } else if (feature) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "unsupported analyzer feature '" + std::string(feature->name()) + "'"
      };
    }
  }

  // limit the maximum size of analyzer properties
  if (ANALYZER_PROPERTIES_SIZE_MAX < properties.byteSize()) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "analyzer properties size of '" + std::to_string(properties.byteSize()) +
      "' exceeds the maximum allowed limit of '" + std::to_string(ANALYZER_PROPERTIES_SIZE_MAX) + "'"
    };
  }

  auto analyzerPool = std::make_shared<AnalyzerPool>(name);

  if (!analyzerPool->init(type, properties, features)) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Failure initializing an arangosearch analyzer instance for name '" + std::string(name) +
      "' type '" + std::string(type) + "'." +
      (properties.isNone() ?
        std::string(" Init without properties")
        : std::string(" Properties '") + properties.toString() + "'") +
      " was rejected by analyzer. Please check documentation for corresponding analyzer type."
    };
  }

  // noexcept
  pool = analyzerPool;
  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate analyzer parameters and emplace into map
////////////////////////////////////////////////////////////////////////////////
Result IResearchAnalyzerFeature::emplaceAnalyzer( // emplace
    EmplaceAnalyzerResult& result, // emplacement result on success (out-param)
    Analyzers& analyzers,
    irs::string_ref const& name,
    irs::string_ref const& type,
    VPackSlice const properties,
    irs::flags const& features) {
  // check type available
  if (!irs::analysis::analyzers::exists(type, irs::text_format::vpack, false)) {
    return {
      TRI_ERROR_NOT_IMPLEMENTED,
      "Not implemented analyzer type '" + std::string(type) + "'." };
  }

  // validate analyzer name
  auto split = splitAnalyzerName(name);

  if (!TRI_vocbase_t::IsAllowedName(false, velocypack::StringRef(split.second.c_str(), split.second.size()))) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid characters in analyzer name '") + std::string(split.second) + "'" };
  }

  // validate that features are supported by arangod an ensure that their
  // dependencies are met
  for(auto& feature: features) {
    if (&irs::frequency::type() == feature) {
      // no extra validation required
    } else if (&irs::norm::type() == feature) {
      // no extra validation required
    } else if (&irs::position::type() == feature) {
      if (!features.check(irs::frequency::type())) {
        return {
          TRI_ERROR_BAD_PARAMETER,
          "missing feature '" + std::string(irs::frequency::type().name()) +
          "' required when '" + std::string(feature->name()) + "' feature is specified" };
      }
    } else if (feature) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "unsupported analyzer feature '" + std::string(feature->name()) + "'" };
    }
  }

  // limit the maximum size of analyzer properties
  if (ANALYZER_PROPERTIES_SIZE_MAX < properties.byteSize()) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "analyzer properties size of '" + std::to_string(properties.byteSize()) +
      "' exceeds the maximum allowed limit of '" + std::to_string(ANALYZER_PROPERTIES_SIZE_MAX) + "'" };
  }

  static const auto generator = []( // key + value generator
    irs::hashed_string_ref const& key, // source key
    AnalyzerPool::ptr const& value // source value
  )->irs::hashed_string_ref {
    auto pool = std::make_shared<AnalyzerPool>(key); // allocate pool
    const_cast<AnalyzerPool::ptr&>(value) = pool; // lazy-instantiate pool to avoid allocation if pool is already present
    return pool ? irs::hashed_string_ref(key.hash(), pool->name()) : key; // reuse hash but point ref at value in pool
  };
  auto itr = irs::map_utils::try_emplace_update_key( // emplace and update key
    analyzers, // destination
    generator, // key generator
    irs::make_hashed_ref(name, std::hash<irs::string_ref>()) // key
  );
  auto analyzer = itr.first->second;

  if (!analyzer) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "failure creating an arangosearch analyzer instance for name '" + std::string(name) +
      "' type '" + std::string(type) + "' properties '" + properties.toString() + "'" };
  }

  // new analyzer creation, validate
  if (itr.second) {
    bool erase = true; // potentially invalid insertion took place
    auto cleanup = irs::make_finally([&erase, &analyzers, &itr]()->void {
      if (erase) {
        analyzers.erase(itr.first); // ensure no broken analyzers are left behind
      }
    });

    if (!analyzer->init(type, properties, features)) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "Failure initializing an arangosearch analyzer instance for name '" + std::string(name) +
        "' type '" + std::string(type) + "'." +
        (properties.isNone() ?
          std::string(" Init without properties")
          : std::string(" Properties '") + properties.toString() + "'") +
        " was rejected by analyzer. Please check documentation for corresponding analyzer type." };
    }

    erase = false;
  } else if (!equalAnalyzer(*analyzer, type, properties, features)) { // duplicate analyzer with different configuration
    std::ostringstream errorText; // make it look more like velocypack toString result
    errorText << "Name collision detected while registering an arangosearch analyzer.\n"
      << "Current definition is:\n{\n  name:'" << name << "'\n"
      << "  type: '" << type << "'\n";
    if (!properties.isNone()) {
      errorText << "  properties:'" << properties.toString() << "'\n";
    }
    errorText  << "  features: [\n";
    for (auto feature = std::begin(features); feature != std::end(features);) {
      errorText << "    '" << (*feature)->name() << "'";
      ++feature;
      if (feature != std::end(features)) {
        errorText << ",";
      }
      errorText << "\n";
    }
    VPackBuilder existingDefinition;
    analyzer->toVelocyPack(existingDefinition, false);
    errorText << "  ]\n}\nPrevious definition was:\n"
      << existingDefinition.toString();
    return  {TRI_ERROR_BAD_PARAMETER, errorText.str() };

  }

  result = itr;

  return {};
}

Result IResearchAnalyzerFeature::emplace(
    EmplaceResult& result,
    irs::string_ref const& name,
    irs::string_ref const& type,
    VPackSlice const properties,
    irs::flags const& features) {
  auto const split = splitAnalyzerName(name);

  try {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    if (!split.first.null()) { // do not trigger load for static-analyzer requests
      auto res = loadAnalyzers(split.first);

      if (!res.ok()) {
        return res;
      }
    }

    // validate and emplace an analyzer
    EmplaceAnalyzerResult itr;
    auto res = emplaceAnalyzer(itr, _analyzers, name, type, properties, features);

    if (!res.ok()) {
      return res;
    }

    auto* engine = EngineSelectorFeature::ENGINE;
    bool erase = itr.second; // an insertion took place
    auto cleanup = irs::make_finally([&erase, this, &itr]()->void {
      if (erase) {
        _analyzers.erase(itr.first); // ensure no broken analyzers are left behind
      }
    });
    auto pool = itr.first->second;

    // new pool creation
    if (itr.second) {
      if (!pool) {
        return {
          TRI_ERROR_INTERNAL,
          "failure creating an arangosearch analyzer instance for name '" + std::string(name) +
          "' type '" + std::string(type) +
          "' properties '" + properties.toString() + "'" };
      }

      // persist only on coordinator and single-server while not in recovery
      if ((!engine || !engine->inRecovery()) // do not persist during recovery
          && (ServerState::instance()->isCoordinator() // coordinator
              || ServerState::instance()->isSingleServer())) {// single-server
        res = storeAnalyzer(*pool);
      }

      if (res.ok()) {
        result = std::make_pair(pool, itr.second);
        erase = false; // successful pool creation, cleanup not required
      }

      return res;
    }

    result = std::make_pair(pool, itr.second);
  } catch (basics::Exception const& e) {
    return {
      e.code(),
      "caught exception while registering an arangosearch analizer name '" + std::string(name) +
      "' type '" + std::string(type) +
      "' properties '" + properties.toString() +
      "': " + std::to_string(e.code()) + " " + e.what() };
  } catch (std::exception const& e) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while registering an arangosearch analizer name '" + std::string(name) +
      "' type '" + std::string(type) +
      "' properties '" + properties.toString() + "': " + e.what() };
  } catch (...) {
    return {
      TRI_ERROR_INTERNAL, // code
      "caught exception while registering an arangosearch analizer name '" + std::string(name) +
      "' type '" + std::string(type) +
      "' properties '" + properties.toString() + "'" };
  }

  return {};
}

AnalyzerPool::ptr IResearchAnalyzerFeature::get(
    irs::string_ref const& normalizedName,
    AnalyzerName const& name,
    bool onlyCached) const noexcept {
  try {
    if (!name.first.null()) { // check if analyzer is static
      if (!onlyCached) {
        // load analyzers for database
        auto const res = const_cast<IResearchAnalyzerFeature*>(this)->loadAnalyzers(name.first);

        if (!res.ok()) {
          LOG_TOPIC("36062", WARN, iresearch::TOPIC)
            << "failure to load analyzers for database '" << name.first
            << "' while getting analyzer '" << name
            << "': " << res.errorNumber() << " " << res.errorMessage();
          TRI_set_errno(res.errorNumber());

          return nullptr;
        }
      }
    }

    ReadMutex mutex(_mutex);
    SCOPED_LOCK(mutex);
    auto itr = _analyzers.find(irs::make_hashed_ref(normalizedName, std::hash<irs::string_ref>()));

    if (itr == _analyzers.end()) {
      LOG_TOPIC("4049c", WARN, iresearch::TOPIC)
          << "failure to find arangosearch analyzer name '" << normalizedName << "'";

      return nullptr;
    }

    auto pool = itr->second;

    if (pool) {
      return pool;
    }

    LOG_TOPIC("1a29c", WARN, iresearch::TOPIC)
        << "failure to get arangosearch analyzer name '" << normalizedName << "'";
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
    irs::string_ref const& name,
    TRI_vocbase_t const& activeVocbase,
    TRI_vocbase_t const& systemVocbase,
    bool onlyCached /*= false*/) const {
  auto const normalizedName = normalize(name, activeVocbase, systemVocbase, true);

  auto const split = splitAnalyzerName(normalizedName);

  if (!split.first.null() &&
      split.first != activeVocbase.name() &&
      split.first != systemVocbase.name()) {
    // accessing local analyzer from within another database
    return nullptr;
  }

  return get(normalizedName, split, onlyCached);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a container of statically defined/initialized analyzers
////////////////////////////////////////////////////////////////////////////////
/*static*/ IResearchAnalyzerFeature::Analyzers const& IResearchAnalyzerFeature::getStaticAnalyzers() {
  struct Instance {
    Analyzers analyzers;
    Instance() {
      // register the identity analyzer
      {
        irs::flags const extraFeatures = {irs::frequency::type(), irs::norm::type()};
        irs::string_ref const name("identity");

        auto pool = std::make_shared<AnalyzerPool>(name);


        if (!pool || !pool->init(IdentityAnalyzer::type().name(),
                                 VPackSlice::emptyObjectSlice(),
                                 extraFeatures)) {
          LOG_TOPIC("26de1", WARN, iresearch::TOPIC)
              << "failure creating an arangosearch static analyzer instance "
                 "for name '"
              << name << "'";

          // this should never happen, treat as an assertion failure
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "failed to create arangosearch static analyzer");
        }

        analyzers.emplace(
          irs::make_hashed_ref(irs::string_ref(pool->name()), std::hash<irs::string_ref>()),
          pool);
      }

      // register the text analyzers
      {
        // Note: ArangoDB strings coming from JavaScript user input are UTF-8 encoded
        irs::string_ref const locales[] = {
          "de", "en", "es", "fi", "fr", "it",
          "nl", "no", "pt", "ru", "sv", "zh"
        };

        irs::flags const extraFeatures = {
          irs::frequency::type(),
          irs::norm::type(),
          irs::position::type()
        };  // add norms + frequency/position for by_phrase

        irs::string_ref const type("text");

        std::string name;
        VPackBuilder properties;
        for (auto const& locale: locales) {
          // "text_<locale>"
          {
            name = "text_";
            name.append(locale.c_str(), locale.size());
          }

          // { locale: "<locale>.UTF-8", stopwords: [] }
          {
            properties.clear();
            VPackObjectBuilder rootScope(&properties);
            properties.add("locale", VPackValue(std::string(locale) + ".UTF-8"));
            VPackArrayBuilder stopwordsArrayScope(&properties, "stopwords");
          }

          auto pool = std::make_shared<AnalyzerPool>(name);

          if (!pool->init(type, properties.slice(), extraFeatures)) {
            LOG_TOPIC("e25f5", WARN, iresearch::TOPIC)
                << "failure creating an arangosearch static analyzer instance "
                   "for name '"
                << name << "'";

            // this should never happen, treat as an assertion failure
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "failed to create arangosearch static analyzer instance");
          }

          analyzers.emplace(
            irs::make_hashed_ref(irs::string_ref(pool->name()), std::hash<irs::string_ref>()),
            pool);
        }
      }
    }
  };
  static const Instance instance;

  return instance.analyzers;
}

/*static*/ AnalyzerPool::ptr IResearchAnalyzerFeature::identity() noexcept {
  struct Identity {
    AnalyzerPool::ptr instance;
    Identity() {
      // find the 'identity' analyzer pool in the static analyzers
      auto& staticAnalyzers = getStaticAnalyzers();
      irs::string_ref name = "identity";  // hardcoded name of the identity analyzer pool
      auto key = irs::make_hashed_ref(name, std::hash<irs::string_ref>());
      auto itr = staticAnalyzers.find(key);

      if (itr != staticAnalyzers.end()) {
        instance = itr->second;
      }
    }
  };
  static const Identity identity;

  return identity.instance;
}

Result IResearchAnalyzerFeature::loadAnalyzers(
    irs::string_ref const& database /*= irs::string_ref::NIL*/) {
  auto* dbFeature = application_features::ApplicationServer::lookupFeature<DatabaseFeature>("Database");

  if (!dbFeature) {
    return {
      TRI_ERROR_INTERNAL,
      "failure to find feature 'Database' while loading analyzers for database '" + std::string(database)+ "'"
    };
  }

  try {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // '_analyzers'/'_lastLoad' can be asynchronously read

    // load all databases
    if (database.null()) {
      Result res;
      std::unordered_set<irs::hashed_string_ref> seen;
      auto visitor = [this, &res, &seen](TRI_vocbase_t& vocbase)->void {
        auto name = irs::make_hashed_ref(irs::string_ref(vocbase.name()),
                                         std::hash<irs::string_ref>());
        auto result = loadAnalyzers(name);
        auto itr = _lastLoad.find(name);

        if (itr != _lastLoad.end()) {
          seen.insert(name);
        } else if (res.ok()) { // load errors take precedence
          res = {
            TRI_ERROR_INTERNAL,
            "failure to find database last load timestamp after loading analyzers"
          };
        }

        if (!result.ok()) {
          res = result;
        }
      };

      dbFeature->enumerateDatabases(visitor);

      std::unordered_set<std::string> unseen; // make copy since original removed

      // remove unseen databases from timestamp list
      for (auto itr = _lastLoad.begin(), end = _lastLoad.end(); itr != end;) {
        auto name = irs::make_hashed_ref(irs::string_ref(itr->first),
                                         std::hash<irs::string_ref>());
        auto seenItr = seen.find(name);

        if (seenItr == seen.end()) {
          unseen.insert(std::move(itr->first)); // reuse key
          itr = _lastLoad.erase(itr);
        } else {
          ++itr;
        }
      }

      // remove no longer valid analyzers (force remove)
      for (auto itr = _analyzers.begin(), end = _analyzers.end(); itr != end;) {
        auto split = splitAnalyzerName(itr->first);
        // ignore static analyzers
        auto unseenItr = split.first.null() ? unseen.end() : unseen.find(split.first);

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

    auto currentTimestamp = std::chrono::system_clock::now();
    auto databaseKey = // database key used in '_lastLoad'
      irs::make_hashed_ref(database, std::hash<irs::string_ref>());
    auto* engine = EngineSelectorFeature::ENGINE;
    auto itr = _lastLoad.find(databaseKey); // find last update timestamp

    if (!engine || engine->inRecovery()) {
      // always load if inRecovery since collection contents might have changed
      // unless on db-server which does not store analyzer definitions in collections
      if (ServerState::instance()->isDBServer()) {
        return {}; // db-server should not access cluster during inRecovery
      }
    } else if (ServerState::instance()->isSingleServer()) { // single server
      if(itr != _lastLoad.end()) {
        return {}; // do not reload on single-server
      }
    } else if (itr != _lastLoad.end() // had a previous load
               && itr->second + RELOAD_INTERVAL > currentTimestamp) { // timeout not reached
      return {}; // reload interval not reached
    }

    auto* vocbase = dbFeature->lookupDatabase(database);

    if (!vocbase) {
      if (engine && engine->inRecovery()) {
        return {}; // database might not have come up yet
      }
      if (itr != _lastLoad.end()) {
        cleanupAnalyzers(database); // cleanup any analyzers for 'database'
        _lastLoad.erase(itr);
      }

      return {
        TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
        "failed to find database '" + std::string(database) + "' while loading analyzers"
      };
    }

    if (!getAnalyzerCollection(*vocbase)) {
      if (itr != _lastLoad.end()) {
        cleanupAnalyzers(database); // cleanup any analyzers for 'database'
      }
      _lastLoad[databaseKey] = currentTimestamp; // update timestamp

      return {}; // no collection means nothing to load
    }

    Analyzers analyzers;
    auto visitor = [this, &analyzers, &vocbase](velocypack::Slice const& slice)-> Result {
      if (!slice.isObject()) {
        LOG_TOPIC("5c7a5", ERR, iresearch::TOPIC)
          << "failed to find an object value for analyzer definition while loading analyzer form collection '"
          << arangodb::StaticStrings::AnalyzersCollection
          << "' in database '" << vocbase->name()
          << "', skipping it: " << slice.toString();

        return {}; // skip analyzer
      }

      irs::flags features;
      irs::string_ref key;
      irs::string_ref name;
      irs::string_ref type;
      VPackSlice properties;
      std::string propertiesBuf;

      if (!slice.hasKey(arangodb::StaticStrings::KeyString) // no such field (required)
          || !slice.get(arangodb::StaticStrings::KeyString).isString()) {
        LOG_TOPIC("1dc56", ERR, iresearch::TOPIC)
          << "failed to find a string value for analyzer '" << arangodb::StaticStrings::KeyString
          << "' while loading analyzer form collection '" << arangodb::StaticStrings::AnalyzersCollection
          << "' in database '" << vocbase->name() << "', skipping it: " << slice.toString();

        return {}; // skip analyzer
      }

      key = getStringRef(slice.get(arangodb::StaticStrings::KeyString));

      if (!slice.hasKey("name") // no such field (required)
          || !(slice.get("name").isString() || slice.get("name").isNull())) {
        LOG_TOPIC("f5920", ERR, iresearch::TOPIC)
          << "failed to find a string value for analyzer 'name' while loading analyzer form collection '"
          << arangodb::StaticStrings::AnalyzersCollection
          << "' in database '" << vocbase->name()
          << "', skipping it: " << slice.toString();

        return {}; // skip analyzer
      }

      name = getStringRef(slice.get("name"));

      if (!slice.hasKey("type") // no such field (required)
          || !(slice.get("type").isString() || slice.get("name").isNull())) {
        LOG_TOPIC("9f5c8", ERR, iresearch::TOPIC)
          << "failed to find a string value for analyzer 'type' while loading analyzer form collection '"
          << arangodb::StaticStrings::AnalyzersCollection
          << "' in database '" << vocbase->name()
          << "', skipping it: " << slice.toString();

        return {}; // skip analyzer
      }

      type = getStringRef(slice.get("type"));

      if (slice.hasKey("properties")) {
        auto subSlice = slice.get("properties");

        if (subSlice.isArray() || subSlice.isObject()) {
          properties = subSlice; // format as a jSON encoded string
        } else {
          LOG_TOPIC("a297e", ERR, arangodb::iresearch::TOPIC)
            << "failed to find a string value for analyzer 'properties' while loading analyzer form collection '"
            << arangodb::StaticStrings::AnalyzersCollection
            << "' in database '" << vocbase->name()
            << "', skipping it: " << slice.toString();

          return {}; // skip analyzer
        }
      }

      if (slice.hasKey("features")) {
        auto subSlice = slice.get("features");

        if (!subSlice.isArray()) {
          LOG_TOPIC("7ec8a", ERR, arangodb::iresearch::TOPIC)
            << "failed to find an array value for analyzer 'features' while loading analyzer form collection '"
            << arangodb::StaticStrings::AnalyzersCollection
            << "' in database '" << vocbase->name()
            << "', skipping it: " << slice.toString();

          return {}; // skip analyzer
        }

        for (velocypack::ArrayIterator subItr(subSlice);
             subItr.valid();
             ++subItr
            ) {
          auto subEntry = *subItr;

          if (!subEntry.isString() && !subSlice.isNull()) {
            LOG_TOPIC("7620d", ERR, iresearch::TOPIC)
              << "failed to find a string value for an entry in analyzer 'features' while loading analyzer form collection '"
              << arangodb::StaticStrings::AnalyzersCollection
              << "' in database '" << vocbase->name()
              << "', skipping it: " << slice.toString();

            return {}; // skip analyzer
          }

          auto featureName = getStringRef(subEntry);
          auto* feature = irs::attribute::type_id::get(featureName);

          if (!feature) {
            LOG_TOPIC("4fedc", ERR, iresearch::TOPIC)
              << "failed to find feature '" << featureName << "' while loading analyzer form collection '"
              << arangodb::StaticStrings::AnalyzersCollection
              << "' in database '" << vocbase->name()
              << "', skipping it: " << slice.toString();

            return {}; // skip analyzer
          }

          features.add(*feature);
        }
      }

      auto normalizedName = normalizedAnalyzerName(vocbase->name(), name);
      EmplaceAnalyzerResult result;
      auto res = emplaceAnalyzer(result, analyzers, normalizedName, type, properties, features);

      if (!res.ok()) {
        return res; // caught error emplacing analyzer (abort further processing)
      }

      if (result.second && result.first->second) {
        result.first->second->setKey(key); // update key
      }

      return {};
    };
    auto res = visitAnalyzers(*vocbase, visitor);

    if (!res.ok()) {
      return res;
    }

    // copy over relevant analyzers from '_analyzers' and validate no duplicates
    for (auto& entry: _analyzers) {
      if (!entry.second) {
        continue; // invalid analyzer (should never happen if insertions done via here)
      }

      auto split = splitAnalyzerName(entry.first);

      // different database
      if (split.first != vocbase->name()) {
        auto result = analyzers.emplace(entry.first, entry.second);

        if (!result.second) { // existing entry
          if (result.first->second // valid new entry
              && !equalAnalyzer(*(entry.second),
                                result.first->second->type(),
                                result.first->second->properties(),
                                result.first->second->features())) {
            return {
              TRI_ERROR_BAD_PARAMETER,
              "name collision detected while re-registering a duplicate arangosearch analizer name '" +
              std::string(result.first->second->name()) +
              "' type '" + std::string(result.first->second->type()) +
              "' properties '" + result.first->second->properties().toString() +
              "', previous registration type '" + std::string(entry.second->type()) +
              "' properties '" + entry.second->properties().toString() + "'"
            };
          }

          result.first->second = entry.second; // reuse old analyzer pool to avoid duplicates in memmory
          const_cast<Analyzers::key_type&>(result.first->first) = entry.first; // point key at old pool
        }

        continue; // done with this analyzer
      }

      auto itr = analyzers.find(entry.first);

      if (itr == analyzers.end()) {
        continue; // removed analyzer
      }

      if (itr->second // valid new entry
          && !equalAnalyzer(*(entry.second),
                            itr->second->type(),
                            itr->second->properties(),
                            itr->second->features())) {
        return {
          TRI_ERROR_BAD_PARAMETER,
          "name collision detected while registering a duplicate arangosearch analizer name '" +
          std::string(itr->second->name()) +
          "' type '" + std::string(itr->second->type()) +
          "' properties '" + itr->second->properties().toString() +
          "', previous registration type '" + std::string(entry.second->type()) +
          "' properties '" + entry.second->properties().toString() + "'"
        };
      }

      itr->second = entry.second; // reuse old analyzer pool to avoid duplicates in memmory
      const_cast<Analyzers::key_type&>(itr->first) = entry.first; // point key at old pool
    }

    _lastLoad[databaseKey] = currentTimestamp; // update timestamp
    _analyzers = std::move(analyzers); // update mappings
  } catch (basics::Exception const& e) {
    return { e.code(),
             "caught exception while loading configuration for arangosearch analyzers from database '" +
              std::string(database) + "': " + std::to_string(e.code()) + " "+ e.what() };
  } catch (std::exception const& e) {
    return { TRI_ERROR_INTERNAL,
             "caught exception while loading configuration for arangosearch analyzers from database '" +
             std::string(database) + "': " + e.what() };
  } catch (...) {
    return { TRI_ERROR_INTERNAL,
             "caught exception while loading configuration for arangosearch analyzers from database '" +
             std::string(database) + "'" };
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
  if (dbNameFromAnalyzer.null()) { // NULL means local db name = always reachable
    return true;
  }
  if (dbNameFromAnalyzer.empty()) { // empty name with :: means always system
    if (forGetters) {
      return true; // system database is always accessible for reading analyzer from any other db
    }
    return currentDbName == arangodb::StaticStrings::SystemDatabase;
  }
  return currentDbName == dbNameFromAnalyzer ||
         (forGetters && dbNameFromAnalyzer == arangodb::StaticStrings::SystemDatabase);
}

/*static*/ std::pair<irs::string_ref, irs::string_ref> IResearchAnalyzerFeature::splitAnalyzerName(
    irs::string_ref const& analyzer) noexcept {
  // search for vocbase prefix ending with '::'
  for (size_t i = 1, count = analyzer.size(); i < count; ++i) {
    if (analyzer[i] == ANALYZER_PREFIX_DELIM // current is delim
        && analyzer[i - 1] == ANALYZER_PREFIX_DELIM) { // previous is also delim
      auto vocbase = i > 1 // non-empty prefix, +1 for first delimiter char
        ? irs::string_ref(analyzer.c_str(), i - 1) // -1 for the first ':' delimiter
        : irs::string_ref::EMPTY;
      auto name = i < count - 1 // have suffix
        ? irs::string_ref(analyzer.c_str() + i + 1, count - i - 1) // +-1 for the suffix after '::'
        : irs::string_ref::EMPTY; // do not point after end of buffer

      return std::make_pair(vocbase, name); // prefixed analyzer name
    }
  }

  return std::make_pair(irs::string_ref::NIL, analyzer); // unprefixed analyzer name
}

/*static*/ std::string IResearchAnalyzerFeature::normalize( // normalize name
    irs::string_ref const& name, // analyzer name
    TRI_vocbase_t const& activeVocbase, // fallback vocbase if not part of name
    TRI_vocbase_t const& systemVocbase, // the system vocbase for use with empty prefix
    bool expandVocbasePrefix /*= true*/) { // use full vocbase name as prefix for active/system v.s. EMPTY/'::'
  auto& staticAnalyzers = getStaticAnalyzers();

  if (staticAnalyzers.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>())) != staticAnalyzers.end()) {
    return name; // special case for singleton static analyzers
  }

  auto split = splitAnalyzerName(name);

  if (expandVocbasePrefix) {
    if (split.first.null()) {
      return normalizedAnalyzerName(activeVocbase.name(), split.second);
    }

    if (split.first.empty()) {
      return normalizedAnalyzerName(systemVocbase.name(), split.second);
    }
  } else {
    // .........................................................................
    // normalize vocbase such that active vocbase takes precedence over system
    // vocbase i.e. prefer NIL over EMPTY
    // .........................................................................
    if (&systemVocbase == &activeVocbase ||
        split.first.null() ||
        (split.first == activeVocbase.name())) { // active vocbase
      return split.second;
    }

    if (split.first.empty() || split.first == systemVocbase.name()) { // system vocbase
      return normalizedAnalyzerName("", split.second);
    }
  }

  return name; // name prefixed with vocbase (or NIL)
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

Result IResearchAnalyzerFeature::remove(
    irs::string_ref const& name,
    bool force /*= false*/) {
  try {
    auto split = splitAnalyzerName(name);

    if (split.first.null()) {
      return { TRI_ERROR_FORBIDDEN, "built-in analyzers cannot be removed" };
    }

    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    auto itr = _analyzers.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>()));

    if (itr == _analyzers.end()) {
      return {
        TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
        "failure to find analyzer while removing arangosearch analyzer '" + std::string(name)+ "'"
      };
    }

    auto& pool = itr->second;

    // this should never happen since analyzers should always be valid, but this
    // will make the state consistent again
    if (!pool) {
      _analyzers.erase(itr);
      return {
        TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
        "failure to find a valid analyzer while removing arangosearch analyzer '" + std::string(name)+ "'"
      };
    }

    if (!force && analyzerInUse(split.first, pool)) { // +1 for ref in '_analyzers'
      return {
        TRI_ERROR_ARANGO_CONFLICT,
        "analyzer in-use while removing arangosearch analyzer '" + std::string(name)+ "'"
      };
    }

    // on db-server analyzers are not persisted
    // allow removal even inRecovery()
    if (ServerState::instance()->isDBServer()) {
      _analyzers.erase(itr);

      return {};
    }

    // .........................................................................
    // after here the analyzer must be removed from the persisted store first
    // .........................................................................

    // this should never happen since non-static analyzers should always have a
    // valid '_key' after
    if (pool->_key.null()) {
      return {
        TRI_ERROR_INTERNAL,
        "failure to find '" + arangodb::StaticStrings::KeyString +
        "' while removing arangosearch analyzer '" + std::string(name)+ "'" };
    }

    auto* engine = EngineSelectorFeature::ENGINE;

    // do not allow persistence while in recovery
    if (engine && engine->inRecovery()) {
      return { TRI_ERROR_INTERNAL,
               "failure to remove arangosearch analyzer '" + std::string(name) +
               "' configuration while storage engine in recovery" };
    }

    auto* dbFeature = application_features::ApplicationServer::lookupFeature<
      DatabaseFeature
    >("Database");

    if (!dbFeature) {
      return {
        TRI_ERROR_INTERNAL,
        "failure to find feature 'Database' while removing arangosearch analyzer '" + std::string(name)+ "'"
      };
    }

    auto* vocbase = dbFeature->useDatabase(split.first);

    if (!vocbase) {
      return {
        TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
        "failure to find vocbase while removing arangosearch analyzer '" + std::string(name)+ "'"
      };
    }

    std::shared_ptr<LogicalCollection> collection;
    auto collectionCallback = [&collection](
        std::shared_ptr<LogicalCollection> const& col)->void {
      collection = col;
    };

    methods::Collections::lookup( // find collection
      *vocbase, arangodb::StaticStrings::AnalyzersCollection, collectionCallback // args
    );

    if (!collection) {
      return {
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        "failure to find collection while removing arangosearch analyzer '" + std::string(name)+ "'"
      };
    }

    SingleCollectionTransaction trx(transaction::StandaloneContext::Create(*vocbase),
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
    builder.close();

    auto result = // remove
      trx.remove(arangodb::StaticStrings::AnalyzersCollection, builder.slice(), options);

    if (!result.ok()) {
      trx.abort();

      return result.result;
    }

    auto commitResult = trx.commit();
    if (!commitResult.ok()) {
      return commitResult;
    }

    _analyzers.erase(itr);
  } catch (basics::Exception const& e) {
    return { e.code(),
            "caught exception while removing configuration for arangosearch analyzer name '" +
            std::string(name) + "': " + std::to_string(e.code()) + " "+ e.what() };
  } catch (std::exception const& e) {
    return { TRI_ERROR_INTERNAL,
             "caught exception while removing configuration for arangosearch analyzer name '" +
             std::string(name) + "': " + e.what() };
  } catch (...) {
    return { TRI_ERROR_INTERNAL,
             "caught exception while removing configuration for arangosearch analyzer name '" +
             std::string(name) + "'" };
  }

  return {};
}

void IResearchAnalyzerFeature::start() {
  if (!isEnabled()) {
    return;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // sanity check: we rely on this condition is true internally
  {
    auto sysDbFeature =
        application_features::ApplicationServer::lookupFeature<SystemDatabaseFeature>(
            "SystemDatabase");
    if (sysDbFeature && sysDbFeature->use()) {  // feature/db may be absent in some unit-test enviroment
      TRI_ASSERT(sysDbFeature->use()->name() == arangodb::StaticStrings::SystemDatabase);
    }
  }
#endif
  // register analyzer functions
  {
    auto* functions =
        application_features::ApplicationServer::lookupFeature<aql::AqlFunctionFeature>(
            "AQLFunctions");

    if (functions) {
      addFunctions(*functions);
    } else {
      LOG_TOPIC("7dcd9", WARN, iresearch::TOPIC)
          << "failure to find feature 'AQLFunctions' while registering "
             "IResearch functions";
    }
  }
}

void IResearchAnalyzerFeature::stop() {
  if (!isEnabled()) {
    return;
  }

  {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // '_analyzers' can be asynchronously read

    _analyzers = getStaticAnalyzers();  // clear cache and reload static analyzers
  }
}

Result IResearchAnalyzerFeature::storeAnalyzer(AnalyzerPool& pool) {
  auto* dbFeature = application_features::ApplicationServer::lookupFeature< // find feature
    DatabaseFeature // feature type
  >("Database");

  if (!dbFeature) {
    return { TRI_ERROR_INTERNAL,
             "failure to find feature 'Database' while persising arangosearch analyzer '" +
             pool.name()+ "'" };
  }

  if (pool.type().null()) {
    return { TRI_ERROR_BAD_PARAMETER,
             "failure to persist arangosearch analyzer '" +
             pool.name() + "' configuration with 'null' type" };
  }

  auto* engine = EngineSelectorFeature::ENGINE;

  // do not allow persistence while in recovery
  if (engine && engine->inRecovery()) {
    return { TRI_ERROR_INTERNAL,
             "failure to persist arangosearch analyzer '" + pool.name() +
             "' configuration while storage engine in recovery" };
  }

  auto split = splitAnalyzerName(pool.name());
  auto* vocbase = dbFeature->useDatabase(split.first);

  if (!vocbase) {
    return { TRI_ERROR_INTERNAL,
             "failure to find vocbase while persising arangosearch analyzer '" +
             pool.name() + "'" };
  }

  try {
    auto collection = getAnalyzerCollection(*vocbase);

    if (!collection) {
       return {
         TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
         "failure to find collection '" +
         arangodb::StaticStrings::AnalyzersCollection +
         "' in vocbase '" + vocbase->name() +
         "' vocbase while persising arangosearch analyzer '" + pool.name()+ "'" };
    }

    SingleCollectionTransaction trx(transaction::StandaloneContext::Create(*vocbase),
                                    arangodb::StaticStrings::AnalyzersCollection,
                                    AccessMode::Type::WRITE);
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
      return { TRI_ERROR_INTERNAL,
               "failure to parse result as a JSON object while persisting configuration for arangosearch analyzer name '" +
               pool.name() + "'" };
    }

    auto key = slice.get(arangodb::StaticStrings::KeyString);

    if (!key.isString()) {
      return { TRI_ERROR_INTERNAL,
               "failure to find the resulting key field while persisting configuration for arangosearch analyzer name '" +
               pool.name() + "'" };
    }

    res = trx.commit();

    if (!res.ok()) {
      trx.abort();

      return res;
    }

    pool.setKey(getStringRef(key));
  } catch (basics::Exception const& e) {
    return { e.code(),
             "caught exception while persisting configuration for arangosearch analyzer name '" +
             pool.name() + "': " + std::to_string(e.code()) + " "+ e.what() };
  } catch (std::exception const& e) {
    return { TRI_ERROR_INTERNAL,
             "caught exception while persisting configuration for arangosearch analyzer name '" +
             pool.name() + "': " + e.what() };
  } catch (...) {
    return { TRI_ERROR_INTERNAL,
             "caught exception while persisting configuration for arangosearch analyzer name '" +
             pool.name() + "'" };
  }

  return {};
}

bool IResearchAnalyzerFeature::visit( // visit all analyzers
    std::function<bool(AnalyzerPool::ptr const& analyzer)> const& visitor // visitor
) const {
  Analyzers analyzers;

  {
    ReadMutex mutex(_mutex);
    SCOPED_LOCK(mutex);
    analyzers = _analyzers;
  }

  for (auto& entry: analyzers) {
    if (entry.second && !visitor(entry.second)) {
      return false; // termination request
    }
  }

  return true;
}

bool IResearchAnalyzerFeature::visit( // visit analyzers
    std::function<bool(AnalyzerPool::ptr const& analyzer)> const& visitor, // visitor
    TRI_vocbase_t const* vocbase // analyzers for vocbase
) const {
  // static analyzer visitation
  if (!vocbase) {
    for (auto& entry: getStaticAnalyzers()) {
      if (entry.second && !visitor(entry.second)) {
        return false; // termination request
      }
    }

    return true;
  }

  auto res = const_cast<IResearchAnalyzerFeature*>(this)->loadAnalyzers( // load analyzers for database
    vocbase->name() // args
  );

  if (!res.ok()) {
    LOG_TOPIC("73695", WARN, iresearch::TOPIC)
      << "failure to load analyzers while visiting database '" << vocbase->name() << "': " << res.errorNumber() << " " << res.errorMessage();
    TRI_set_errno(res.errorNumber());

    return false;
  }

  Analyzers analyzers;

  {
    ReadMutex mutex(_mutex);
    SCOPED_LOCK(mutex);
    analyzers = _analyzers;
  }

  for (auto& entry: analyzers) {
    if (entry.second // have entry
        && (splitAnalyzerName(entry.first).first == vocbase->name()) // requested vocbase
        && !visitor(entry.second) // termination request
       ) {
      return false;
    }
  }

  return true;
}

void IResearchAnalyzerFeature::cleanupAnalyzers(irs::string_ref const& database) {
  if (ADB_UNLIKELY(database.empty())) {
    TRI_ASSERT(FALSE);
    return;
  }
  for (auto itr = _analyzers.begin(), end = _analyzers.end();  itr != end;) {
    auto split = splitAnalyzerName(itr->first);
    if (split.first == database) {
      itr = _analyzers.erase(itr);
    } else {
      ++itr;
    }
  }
}

void IResearchAnalyzerFeature::invalidate(const TRI_vocbase_t& vocbase) {
  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex);
  auto database = irs::string_ref(vocbase.name());
  auto itr = _lastLoad.find(
      irs::make_hashed_ref(database, std::hash<irs::string_ref>()));
  if (itr != _lastLoad.end()) {
    cleanupAnalyzers(database);
    _lastLoad.erase(itr);
  }
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
