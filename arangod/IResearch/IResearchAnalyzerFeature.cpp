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
#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

#include "ApplicationServerHelper.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "IResearchAnalyzerFeature.h"
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
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"
#include "VocBase/Methods/Collections.h"

namespace {

static std::string const ANALYZER_COLLECTION_NAME("_analyzers");
static char const ANALYZER_PREFIX_DELIM = ':'; // name prefix delimiter (2 chars)
static size_t const ANALYZER_PROPERTIES_SIZE_MAX = 1024 * 1024; // arbitrary value
static size_t const DEFAULT_POOL_SIZE = 8;  // arbitrary value
static std::string const FEATURE_NAME("IResearchAnalyzer");
static irs::string_ref const IDENTITY_ANALYZER_NAME("identity");
static auto const RELOAD_INTERVAL = std::chrono::seconds(60); // arbitrary value

struct IdentityValue : irs::term_attribute {
  void value(irs::bytes_ref const& data) noexcept { value_ = data; }
};

class IdentityAnalyzer : public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();
  DECLARE_FACTORY(irs::string_ref const& args);  // args ignored

  IdentityAnalyzer();
  virtual irs::attribute_view const& attributes() const NOEXCEPT override;
  virtual bool next() override;
  virtual bool reset(irs::string_ref const& data) override;

 private:
  irs::attribute_view _attrs;
  IdentityValue _term;
  irs::increment _inc;
  irs::string_ref _value;
  bool _empty;
};

DEFINE_ANALYZER_TYPE_NAMED(IdentityAnalyzer, IDENTITY_ANALYZER_NAME);
REGISTER_ANALYZER_JSON(IdentityAnalyzer, IdentityAnalyzer::make);

/*static*/ irs::analysis::analyzer::ptr IdentityAnalyzer::make(irs::string_ref const& args) {
  UNUSED(args);
  PTR_NAMED(IdentityAnalyzer, ptr);
  return ptr;
}

IdentityAnalyzer::IdentityAnalyzer()
    : irs::analysis::analyzer(IdentityAnalyzer::type()), _empty(true) {
  _attrs.emplace(_term);
  _attrs.emplace(_inc);
}

irs::attribute_view const& IdentityAnalyzer::attributes() const NOEXCEPT {
  return _attrs;
}

bool IdentityAnalyzer::next() {
  auto empty = _empty;

  _term.value(irs::ref_cast<irs::byte_type>(_value));
  _empty = true;
  _value = irs::string_ref::NIL;

  return !empty;
}

bool IdentityAnalyzer::reset(irs::string_ref const& data) {
  _empty = false;
  _value = data;

  return !_empty;
}

arangodb::aql::AqlValue aqlFnTokens(arangodb::aql::ExpressionContext* expressionContext,
                                    arangodb::transaction::Methods* trx,
                                    arangodb::aql::VPackFunctionParameters const& args) {
  if (2 != args.size() || !args[0].isString() || !args[1].isString()) {
    LOG_TOPIC("740fd", WARN, arangodb::iresearch::TOPIC)
        << "invalid arguments passed while computing result for function "
           "'TOKENS'";
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return arangodb::aql::AqlValue();
  }

  auto data = arangodb::iresearch::getStringRef(args[0].slice());
  auto name = arangodb::iresearch::getStringRef(args[1].slice());
  auto* analyzers =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

  if (!analyzers) {
    LOG_TOPIC("fbd91", WARN, arangodb::iresearch::TOPIC)
        << "failure to find feature 'arangosearch' while computing result for "
           "function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return arangodb::aql::AqlValue();
  }

  arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr pool;

  if (trx) {
    auto* sysDatabase = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
      arangodb::SystemDatabaseFeature // featue type
    >();
    auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;

    if (sysVocbase) {
      pool = analyzers->get( // get analyzer
        arangodb::iresearch::IResearchAnalyzerFeature::normalize( // normalize
          name, trx->vocbase(), *sysVocbase // args
        )
      );
    }
  } else {
    pool = analyzers->get(name); // verbatim
  }

  if (!pool) {
    LOG_TOPIC("0d256", WARN, arangodb::iresearch::TOPIC)
        << "failure to find arangosearch analyzer pool name '" << name
        << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return arangodb::aql::AqlValue();
  }

  auto analyzer = pool->get();

  if (!analyzer) {
    LOG_TOPIC("d7477", WARN, arangodb::iresearch::TOPIC)
        << "failure to find arangosearch analyzer name '" << name
        << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return arangodb::aql::AqlValue();
  }

  if (!analyzer->reset(data)) {
    LOG_TOPIC("45a2d", WARN, arangodb::iresearch::TOPIC)
        << "failure to reset arangosearch analyzer name '" << name
        << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return arangodb::aql::AqlValue();
  }

  auto& values = analyzer->attributes().get<irs::term_attribute>();

  if (!values) {
    LOG_TOPIC("f46f2", WARN, arangodb::iresearch::TOPIC)
        << "failure to retrieve values from arangosearch analyzer name '"
        << name << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return arangodb::aql::AqlValue();
  }

  // to avoid copying Builder's default buffer when initializing AqlValue
  // create the buffer externally and pass ownership directly into AqlValue
  auto buffer = irs::memory::make_unique<arangodb::velocypack::Buffer<uint8_t>>();

  if (!buffer) {
    LOG_TOPIC("97cd0", WARN, arangodb::iresearch::TOPIC)
        << "failure to allocate result buffer while computing result for "
           "function 'TOKENS'";

    return arangodb::aql::AqlValue();
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
   arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool const& pool, // analyzer
   irs::string_ref const& type, // analyzer type
   irs::string_ref const& properties, // analyzer properties
   irs::flags const& features // analyzer features
) noexcept {
  return type == pool.type() // same type
         && properties == pool.properties() // same properties
         && features == pool.features(); // same features
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the collection containing analyzer definitions (if found)
///        taken from vocabse for single-server and from ClusterInfo on cluster
/// @note cannot use arangodb::methods::Collections::lookup(...) since it will
///       try to resolve via vocbase for the case of db-server
/// @note cannot use arangodb::CollectionNameResolver::getCollection(...) since
///       it will try to resolve via vocbase for the case of db-server
////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<arangodb::LogicalCollection> getAnalyzerCollection( // get collection
    TRI_vocbase_t const& vocbase // collection vocbase
) {
  if (arangodb::ServerState::instance()->isSingleServer()) {
    return vocbase.lookupCollection(ANALYZER_COLLECTION_NAME);
  }

  try {
    auto* ci = arangodb::ClusterInfo::instance();

    if (ci) {
      return ci->getCollectionNT(vocbase.name(), ANALYZER_COLLECTION_NAME);
    }

    LOG_TOPIC("00001", WARN, arangodb::iresearch::TOPIC)
      << "failure to find 'ClusterInfo' instance while looking up Analyzer collection '" << ANALYZER_COLLECTION_NAME << "' in vocbase '" << vocbase.name() << "'";
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC("00002", WARN, arangodb::iresearch::TOPIC)
      << "caught exception while looking up Analyzer collection '" << ANALYZER_COLLECTION_NAME << "' in vocbase '" << vocbase.name() << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC("00003", WARN, arangodb::iresearch::TOPIC)
      << "caught exception while looking up Analyzer collection '" << ANALYZER_COLLECTION_NAME << "' in vocbase '" << vocbase.name() << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC("00004", WARN, arangodb::iresearch::TOPIC)
      << "caught exception while looking up Analyzer collection '" << ANALYZER_COLLECTION_NAME << "' in vocbase '" << vocbase.name() << "'";
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

std::string normalizedAnalyzerName(
    std::string database, // database
    irs::string_ref const& analyzer // analyzer
) {
  return database.append(2, ANALYZER_PREFIX_DELIM).append(analyzer);
}

bool iresearchAnalyzerLegacyAnalyzers( // upgrade task
    TRI_vocbase_t& vocbase, // upgraded vocbase
    arangodb::velocypack::Slice const& upgradeParams // upgrade params
) {
  auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::iresearch::IResearchAnalyzerFeature // feature type
  >();

  if (!analyzers) {
    LOG_TOPIC("6b6b5", WARN, arangodb::iresearch::TOPIC)
      << "failure to find '" << arangodb::iresearch::IResearchAnalyzerFeature::name() << "' feature while registering legacy static analyzers with vocbase '" << vocbase.name() << "'";
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return false; // internal error
  }

  // drop legacy collection if upgrading the system vocbase and collection found
  {
    auto* sysDatabase = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
      arangodb::SystemDatabaseFeature // feature type
    >();

    if (!sysDatabase) {
      LOG_TOPIC("8783e", WARN, arangodb::iresearch::TOPIC)
        << "failure to find '" << arangodb::SystemDatabaseFeature::name() << "' feature while registering legacy static analyzers with vocbase '" << vocbase.name() << "'";
      TRI_set_errno(TRI_ERROR_INTERNAL);

      return false; // internal error
    }

    auto sysVocbase = sysDatabase->use();

    if (sysVocbase && sysVocbase->name() == vocbase.name()) { // upgrading system vocbase
      static std::string const LEGACY_ANALYZER_COLLECTION_NAME("_iresearch_analyzers");

      arangodb::methods::Collections::lookup( // find legacy analyzer collection
        *sysVocbase, // vocbase to search
        LEGACY_ANALYZER_COLLECTION_NAME, // collection name to search
        [](std::shared_ptr<arangodb::LogicalCollection> const& col)->void { // callback if found
          if (col) {
            arangodb::methods::Collections::drop(*col, true, -1.0); // -1.0 same as in RestCollectionHandler
          }
        }
      );
    }
  }

  // register the text analyzers with the current vocbase
  {
    // NOTE: ArangoDB strings coming from JavaScript user input are UTF-8 encoded
    static const std::vector<std::pair<irs::string_ref, irs::string_ref>> legacyAnalzyers = {
      { "text_de", "{ \"locale\": \"de.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
      { "text_en", "{ \"locale\": \"en.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
      { "text_es", "{ \"locale\": \"es.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
      { "text_fi", "{ \"locale\": \"fi.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
      { "text_fr", "{ \"locale\": \"fr.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
      { "text_it", "{ \"locale\": \"it.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
      { "text_nl", "{ \"locale\": \"nl.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
      { "text_no", "{ \"locale\": \"no.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
      { "text_pt", "{ \"locale\": \"pt.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
      { "text_ru", "{ \"locale\": \"ru.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
      { "text_sv", "{ \"locale\": \"sv.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
      { "text_zh", "{ \"locale\": \"zh.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
    };
    static const irs::flags legacyAnalyzerFeatures = { // add norms + frequency/position for by_phrase
      irs::frequency::type(), // frequency feature
      irs::norm::type(), // norm feature
      irs::position::type(), // position feature
    };
    static const irs::string_ref legacyAnalyzerType("text");
    bool success = true;

    // register each legacy static analyzer with the current vocbase
    for (auto& entry: legacyAnalzyers) {
      auto name = normalizedAnalyzerName(vocbase.name(), entry.first);
      auto& type = legacyAnalyzerType;
      auto& properties = entry.second;
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      auto res = analyzers->emplace( // add analyzer
        result, name, type, properties, legacyAnalyzerFeatures // args
      );

      if (!res.ok()) {
        LOG_TOPIC("ec566", WARN, arangodb::iresearch::TOPIC)
          << "failure while registering a legacy static analyzer '" << name << "' with vocbase '" << vocbase.name() << "': " << res.errorNumber() << " " << res.errorMessage();

        success = false;
      } else if (!result.first) {
        LOG_TOPIC("1dc1d", WARN, arangodb::iresearch::TOPIC)
          << "failure while registering a legacy static analyzer '" << name << "' with vocbase '" << vocbase.name() << "'";

        success = false;
      }
    }

    return success;
  }
}

void registerUpgradeTasks() {
  auto* upgrade = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::UpgradeFeature // feature type
  >("Upgrade");

  if (!upgrade) {
    return; // nothing to register with (OK if no tasks actually need to be applied)
  }

  // register legacy static analyzers with each vocbase found in DatabaseFeature
  // required for backward compatibility for e.g. TOKENS(...) function
  // NOTE: db-servers do not have a dedicated collection for storing analyzers,
  //       instead they get their cache populated from coordinators
  {
    arangodb::methods::Upgrade::Task task;
    task.name = "IResearhAnalyzer legacy analyzers";
    task.description = // description
      "register legacy static analyzers with each vocbase found in DatabaseFeature";
    task.systemFlag = arangodb::methods::Upgrade::Flags::DATABASE_ALL;
    task.clusterFlags = // flags
      arangodb::methods::Upgrade::Flags::CLUSTER_COORDINATOR_GLOBAL // any 1 single coordinator
      | arangodb::methods::Upgrade::Flags::CLUSTER_NONE // local server
      ;
    task.databaseFlags = arangodb::methods::Upgrade::Flags::DATABASE_UPGRADE;
    task.action = &iresearchAnalyzerLegacyAnalyzers;
    upgrade->addTask(std::move(task));

    // FIXME TODO find out why CLUSTER_COORDINATOR_GLOBAL will only work with DATABASE_INIT (hardcoded in Upgrade::clusterBootstrap(...))
    task.name = "IResearhAnalyzer legacy analyzers";
    task.description =
      "register legacy static analyzers with each vocbase found in DatabaseFeature";
    task.systemFlag = arangodb::methods::Upgrade::Flags::DATABASE_ALL;
    task.clusterFlags = // flags
      arangodb::methods::Upgrade::Flags::CLUSTER_COORDINATOR_GLOBAL; // any 1 single coordinator
    task.databaseFlags = arangodb::methods::Upgrade::Flags::DATABASE_INIT;
    task.action = &iresearchAnalyzerLegacyAnalyzers;
    upgrade->addTask(std::move(task));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief split the analyzer name into the vocbase part and analyzer part
/// @param name analyzer name
/// @return pair of first == vocbase name, second == analyzer name
///         EMPTY == system vocbase
///         NIL == unprefixed analyzer name, i.e. active vocbase
////////////////////////////////////////////////////////////////////////////////
std::pair<irs::string_ref, irs::string_ref> splitAnalyzerName( // split name
    irs::string_ref const& analyzer // analyzer name
) noexcept {
  // search for vocbase prefix ending with '::'
  for (size_t i = 1, count = analyzer.size(); i < count; ++i) {
    if (analyzer[i] == ANALYZER_PREFIX_DELIM // current is delim
        && analyzer[i - 1] == ANALYZER_PREFIX_DELIM // previous is also delim
       ) {
      auto vocbase = i > 1 // non-empty prefix, +1 for first delimiter char
        ? irs::string_ref(analyzer.c_str(), i - 1) // -1 for the first ':' delimiter
        : irs::string_ref::EMPTY
        ;
      auto name = i < count - 1 // have suffix
        ? irs::string_ref(analyzer.c_str() + i + 1, count - i - 1) // +-1 for the suffix after '::'
        : irs::string_ref::EMPTY // do not point after end of buffer
        ;

      return std::make_pair(vocbase, name); // prefixed analyzer name
    }
  }

  return std::make_pair(irs::string_ref::NIL, analyzer); // unprefixed analyzer name
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read analyzers from vocbase
/// @return visitation completed fully
////////////////////////////////////////////////////////////////////////////////
arangodb::Result visitAnalyzers( // visit analyzers
  TRI_vocbase_t& vocbase, // vocbase to visit
  std::function<arangodb::Result(arangodb::velocypack::Slice const& slice)> const& visitor // visitor
) {
  static const auto resultVisitor = [](
    std::function<arangodb::Result(arangodb::velocypack::Slice const& slice)> const& visitor, // visitor
    TRI_vocbase_t const& vocbase, // vocbase
    arangodb::velocypack::Slice const& slice // slice to visit
  )->arangodb::Result {
    if (!slice.isArray()) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failed to parse contents of collection '") + ANALYZER_COLLECTION_NAME + "' in database '" + vocbase.name() + " while visiting analyzers"
      );
    }

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto res = visitor(itr.value().resolveExternal());

      if (!res.ok()) {
        return res;
      }
    }

    return arangodb::Result();
  };

  // FIXME TODO find a better way to query a cluster collection
  // workaround for aql::Query failing to execute on a cluster collection
  if (arangodb::ServerState::instance()->isDBServer()) {
    auto cc = arangodb::ClusterComm::instance();

    if (!cc) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failure to find 'ClusterComm' instance while visiting Analyzer collection '") + ANALYZER_COLLECTION_NAME + "' in vocbase '" + vocbase.name() + "'"
      );
    }

    auto collection = getAnalyzerCollection(vocbase);

    if (!collection) {
      return arangodb::Result(); // nothing to load
    }

    static const std::string body("{}"); // RestSimpleQueryHandler::allDocuments() expects opbject (calls get() on slice)
    std::vector<arangodb::ClusterCommRequest> requests;

    // create a request for every shard
    //for (auto& entry: collection->errorNum()) {
    for (auto& entry: *(collection->shardIds())) {
      auto& shardId = entry.first;
      auto url = // url
        "/_db/" + arangodb::basics::StringUtils::urlEncode(vocbase.name())
        + arangodb::RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH
        + "?collection=" + shardId;

      requests.emplace_back( // add shard request
        "shard:" + shardId, // shard
        arangodb::rest::RequestType::PUT, // request type as per SimpleQueryHandker
        url, // request url
        std::shared_ptr<std::string const>(&body, [](std::string const*)->void {}) // body
      );
    }

    // same timeout as in ClusterMethods::getDocumentOnCoordinator()
    cc->performRequests( // execute requests
      requests, 120.0, arangodb::iresearch::TOPIC, false, false // args
    );

    for (auto& request: requests) {
      if (TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == request.result.errorCode) {
        continue; // treat missing collection as if there are no analyzers
      }

      if (TRI_ERROR_NO_ERROR != request.result.errorCode) {
        return arangodb::Result( // result
          request.result.errorCode, request.result.errorMessage // args
        );
      }

      if (!request.result.answer) {
        return arangodb::Result( // result
          TRI_ERROR_INTERNAL, // code
          std::string("failed to get answer from 'ClusterComm' instance while visiting Analyzer collection '") + ANALYZER_COLLECTION_NAME + "' in vocbase '" + vocbase.name() + "'"
        );
      }

      auto slice = request.result.answer->payload();

      if (!slice.hasKey("result")) {
        return arangodb::Result( // result
          TRI_ERROR_INTERNAL, // code
          std::string("failed to parse result from 'ClusterComm' instance while visiting Analyzer collection '") + ANALYZER_COLLECTION_NAME + "' in vocbase '" + vocbase.name() + "'"
        );
      }

      auto res = resultVisitor(visitor, vocbase, slice.get("result"));

      if (!res.ok()) {
        return res;
      }
    }

    return arangodb::Result();
  }

  if (arangodb::ServerState::instance()->isClusterRole()) {
    if (!getAnalyzerCollection(vocbase)) {
      return arangodb::Result(); // treat missing collection as if there are no analyzers
    }

    static const auto queryString = arangodb::aql::QueryString( // query to execute
      std::string("FOR d IN ") + ANALYZER_COLLECTION_NAME + " RETURN d" // query
    );
    arangodb::aql::Query query( // query
      false, vocbase, queryString, nullptr, nullptr, arangodb::aql::PART_MAIN // args
    );
    auto* queryRegistry = arangodb::QueryRegistryFeature::registry();
    auto result = query.executeSync(queryRegistry);

    if (TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == result.result.errorNumber()) {
      return arangodb::Result(); // treat missing collection as if there are no analyzers
    }

    if (result.result.fail()) {
      return result.result;
    }

    auto slice = result.data->slice();

    return resultVisitor(visitor, vocbase, slice);
  }

  if (!vocbase.lookupCollection(ANALYZER_COLLECTION_NAME)) {
    return arangodb::Result(); // treat missing collection as if there are no analyzers
  }

  arangodb::OperationOptions options;
  arangodb::SingleCollectionTransaction trx( // transaction
    arangodb::transaction::StandaloneContext::Create(vocbase), // context
    ANALYZER_COLLECTION_NAME, // collection
    arangodb::AccessMode::Type::READ // access more
  );
  auto res = trx.begin();

  if (!res.ok()) {
    return res;
  }

  auto commit  = irs::make_finally([&trx]()->void { trx.commit(); }); // end read-only transaction
  auto result = trx.all(ANALYZER_COLLECTION_NAME, 0, 0, options);

  if (!result.result.ok()) {
    return result.result;
  }

  auto slice = arangodb::velocypack::Slice(result.buffer->data());

  return resultVisitor(visitor, vocbase, slice);
}

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

}  // namespace

namespace arangodb {
namespace iresearch {

/*static*/ IResearchAnalyzerFeature::AnalyzerPool::Builder::ptr
IResearchAnalyzerFeature::AnalyzerPool::Builder::make(irs::string_ref const& type,
                                                      irs::string_ref const& properties) {
  if (type.empty()) {
    // in ArangoSearch we don't allow to have analyzers with empty type string
    return nullptr;
  }

  // ArangoDB, for API consistency, only supports analyzers configurable via
  // jSON
  return irs::analysis::analyzers::get( // get analyzer
    type, irs::text_format::json, properties, false // args
  );
}

IResearchAnalyzerFeature::AnalyzerPool::AnalyzerPool(irs::string_ref const& name)
    : _cache(DEFAULT_POOL_SIZE), _name(name) {}

bool IResearchAnalyzerFeature::AnalyzerPool::init(irs::string_ref const& type,
                                                  irs::string_ref const& properties,
                                                  irs::flags const& features /*= irs::flags::empty_instance()*/
) {
  try {
    _cache.clear();  // reset for new type/properties

    auto instance = _cache.emplace(type, properties);

    if (instance) {
      _config.clear();
      _config.append(type).append(properties);
      _key = irs::string_ref::NIL;
      _type = irs::string_ref::NIL;
      _properties = irs::string_ref::NIL;

      if (!type.null()) {
        _type = irs::string_ref(&(_config[0]), type.size());
      }

      if (!properties.null()) {
        _properties = irs::string_ref(&(_config[0]) + _type.size(), properties.size());
      }

      _features = features;  // store only requested features

      return true;
    }
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC("62062", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while initializing an arangosearch analizer type '" << _type
        << "' properties '" << _properties << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC("a9196", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while initializing an arangosearch analizer type '"
        << _type << "' properties '" << _properties << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC("7524a", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while initializing an arangosearch analizer type '"
        << _type << "' properties '" << _properties << "'";
    IR_LOG_EXCEPTION();
  }

  _config.clear();                     // set as uninitialized
  _key = irs::string_ref::NIL;         // set as uninitialized
  _type = irs::string_ref::NIL;        // set as uninitialized
  _properties = irs::string_ref::NIL;  // set as uninitialized
  _features.clear();                   // set as uninitialized

  return false;
}

void IResearchAnalyzerFeature::AnalyzerPool::setKey(irs::string_ref const& key) {
  if (key.null()) {
    _key = irs::string_ref::NIL;

    return;  // nothing more to do
  }

  auto keyOffset = _config.size();  // append at end
  auto typeOffset = _type.null() ? 0 : (_type.c_str() - &(_config[0]));  // start of type
  auto propertiesOffset =
      _properties.null() ? 0 : (_properties.c_str() - &(_config[0]));  // start of properties

  _config.append(key.c_str(), key.size());
  _key = irs::string_ref(&(_config[0]) + keyOffset, key.size());

  // update '_type' since '_config' might have been reallocated during
  // append(...)
  if (!_type.null()) {
    TRI_ASSERT(typeOffset + _type.size() <= _config.size());
    _type = irs::string_ref(&(_config[0]) + typeOffset, _type.size());
  }

  // update '_properties' since '_config' might have been reallocated during
  // append(...)
  if (!_properties.null()) {
    TRI_ASSERT(propertiesOffset + _properties.size() <= _config.size());
    _properties = irs::string_ref(&(_config[0]) + propertiesOffset, _properties.size());
  }
}

irs::analysis::analyzer::ptr IResearchAnalyzerFeature::AnalyzerPool::get() const noexcept {
  try {
    // FIXME do not use shared_ptr
    return _cache.emplace(_type, _properties).release();
  } catch (arangodb::basics::Exception const& e) {
    LOG_TOPIC("c9256", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while instantiating an arangosearch analizer type "
           "'"
        << _type << "' properties '" << _properties << "': " << e.code() << " "
        << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC("93baf", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while instantiating an arangosearch analizer type "
           "'"
        << _type << "' properties '" << _properties << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC("08db9", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while instantiating an arangosearch analizer type "
           "'"
        << _type << "' properties '" << _properties << "'";
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

IResearchAnalyzerFeature::IResearchAnalyzerFeature(arangodb::application_features::ApplicationServer& server)
    : ApplicationFeature(server, IResearchAnalyzerFeature::name()) {
  setOptional(true);
  startsAfter("V8Phase");
  // used for registering IResearch analyzer functions
  startsAfter("AQLFunctions");
  // used for getting the system database
  // containing the persisted configuration
  startsAfter("SystemDatabase");
}

/*static*/ bool IResearchAnalyzerFeature::canUse( // check permissions
    TRI_vocbase_t const& vocbase, // analyzer vocbase
    arangodb::auth::Level const& level // access level
) {
  auto* ctx = arangodb::ExecContext::CURRENT;

  return !ctx // authentication not enabled
    || (ctx->canUseDatabase(vocbase.name(), level) // can use vocbase
        && (ctx->canUseCollection(vocbase.name(), ANALYZER_COLLECTION_NAME, level)) // can use analyzers
       );
}

/*static*/ bool IResearchAnalyzerFeature::canUse( // check permissions
  irs::string_ref const& name, // analyzer name (already normalized)
  arangodb::auth::Level const& level // access level
) {
  auto* ctx = arangodb::ExecContext::CURRENT;

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
        && ctx->canUseCollection(split.first, ANALYZER_COLLECTION_NAME, level) // can use analyzers
       );
}

arangodb::Result IResearchAnalyzerFeature::emplace( // emplace an analyzer
  EmplaceResult& result, // emplacement result on success (out-parameter)
  irs::string_ref const& name, // analyzer name
  irs::string_ref const& type, // analyzer type
  irs::string_ref const& properties, // analyzer properties
  irs::flags const& features /*= irs::flags::empty_instance()*/ // analyzer features
) {
  return ensure(result, name, type, properties, features, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate analyzer parameters and emplace into map
////////////////////////////////////////////////////////////////////////////////
arangodb::Result IResearchAnalyzerFeature::emplaceAnalyzer( // emplace
  EmplaceAnalyzerResult& result, // emplacement result on success (out-param)
  Analyzers& analyzers, // analyzers
  irs::string_ref const& name, // analyzer name
  irs::string_ref const& type, // analyzer type
  irs::string_ref const& properties, // analyzer properties
  irs::flags const& features // analyzer features
) {
  // validate analyzer name
  auto split = splitAnalyzerName(name);

  if (!TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(split.second.c_str(), split.second.size()))) {
    return arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("invalid characters in analyzer name '") + std::string(split.second) + "'"
    );
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
        return arangodb::Result( // result
          TRI_ERROR_BAD_PARAMETER, // code
          std::string("missing feature '") + std::string(irs::frequency::type().name()) +"' required when '" + std::string(feature->name()) + "' feature is specified"
        );
      }
    } else if (feature) {
      return arangodb::Result( // result
        TRI_ERROR_BAD_PARAMETER, // code
        std::string("unsupported analyzer feature '") + std::string(feature->name()) + "'" // value
      );
    }
  }

  // limit the maximum size of analyzer properties
  if (ANALYZER_PROPERTIES_SIZE_MAX < properties.size()) {
    return arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("analyzer properties size of '") + std::to_string(properties.size()) + "' exceeds the maximum allowed limit of '" + std::to_string(ANALYZER_PROPERTIES_SIZE_MAX) + "'"
    );
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
    return arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("failure creating an arangosearch analyzer instance for name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "'"
    );
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
      return arangodb::Result( // result
        TRI_ERROR_BAD_PARAMETER, // code
        std::string("failure initializing an arangosearch analyzer instance for name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "'"
      );
    }

    erase = false;
  } else if (!equalAnalyzer(*analyzer, type, properties, features)) { // duplicate analyzer with different configuration
    return arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("name collision detected while registering an arangosearch analizer name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "', previous registration type '" + std::string(analyzer->type()) + "' properties '" + std::string(analyzer->properties()) + "'"
    );
  }

  result = itr;

  return arangodb::Result();
}

arangodb::Result IResearchAnalyzerFeature::ensure( // ensure analyzer existence if possible
  EmplaceResult& result, // emplacement result on success (out-param)
  irs::string_ref const& name, // analyzer name
  irs::string_ref const& type, // analyzer type
  irs::string_ref const& properties, // analyzer properties
  irs::flags const& features, // analyzer features
  bool isEmplace
) {
  try {
    auto split = splitAnalyzerName(name);

    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    if (!split.first.null()) { // do not trigger load for static-analyzer requests
      // do not trigger load of analyzers on coordinator or db-server to avoid
      // recursive lock aquisition in ClusterInfo::loadPlan() if called due to
      // IResearchLink creation,
      // also avoids extra cluster calls if it can be helped (optimization)
      if (!isEmplace && arangodb::ServerState::instance()->isClusterRole()) {
        auto itr = _analyzers.find( // find analyzer previous definition
         irs::make_hashed_ref(name, std::hash<irs::string_ref>())
        );

        if (itr != _analyzers.end()) {
          _analyzers.erase(itr); // remove old definition instead of reloading all
        }
      } else { // trigger analyzer load
        auto res = loadAnalyzers(split.first);

        if (!res.ok()) {
          return res;
        }
      }
    }

    EmplaceAnalyzerResult itr;
    auto res = // validate and emplace an analyzer
      emplaceAnalyzer(itr, _analyzers, name, type, properties, features);

    if (!res.ok()) {
      return res;
    }

    auto* engine = arangodb::EngineSelectorFeature::ENGINE;
    auto allowCreation = // should analyzer creation be allowed (always for cluster)
      isEmplace // if it's a user creation request
      || arangodb::ServerState::instance()->isClusterRole() // always for cluster
      || (engine && engine->inRecovery()); // always during recovery since analyzer collection might not be available yet
    bool erase = itr.second; // an insertion took place
    auto cleanup = irs::make_finally([&erase, this, &itr]()->void {
      if (erase) {
        _analyzers.erase(itr.first); // ensure no broken analyzers are left behind
      }
    });
    auto pool = itr.first->second;

    // new pool creation
    if (itr.second) {
      if (!allowCreation) {
        return arangodb::Result( // result
          TRI_ERROR_BAD_PARAMETER, // code
          std::string("forbidden implicit creation of an arangosearch analyzer instance for name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "'"
        );
      }

      if (!pool) {
        return arangodb::Result( // result
          TRI_ERROR_INTERNAL, // code
          std::string("failure creating an arangosearch analyzer instance for name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "'"
        );
      }

      // persist only on coordinator and single-server while not in recovery
      if ((!engine || !engine->inRecovery()) // do not persist during recovery
          && (arangodb::ServerState::instance()->isCoordinator() // coordinator
              || arangodb::ServerState::instance()->isSingleServer())) {// single-server
        res = storeAnalyzer(*pool);
      }

      if (res.ok()) {
        result = std::make_pair(pool, itr.second);
        erase = false; // successful pool creation, cleanup not required
      }

      return res;
    }

    result = std::make_pair(pool, itr.second);
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result( // result
      e.code(), // code
      std::string("caught exception while registering an arangosearch analizer name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "': " + std::to_string(e.code()) + " " + e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while registering an arangosearch analizer name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while registering an arangosearch analizer name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "'"
    );
  }

  return arangodb::Result();
}

IResearchAnalyzerFeature::AnalyzerPool::ptr IResearchAnalyzerFeature::get( // find analyzer
    irs::string_ref const& name, // analyzer name
    bool onlyCached /*= false*/ // check only locally cached analyzers
) const noexcept {
  try {
    auto split = splitAnalyzerName(name);

    if (!split.first.null() && !onlyCached) { // do not trigger load for static-analyzer requests
      auto res = // load analyzers for database
        const_cast<IResearchAnalyzerFeature*>(this)->loadAnalyzers(split.first);

      if (!res.ok()) {
        LOG_TOPIC("36062", WARN, arangodb::iresearch::TOPIC)
          << "failure to load analyzers for database '" << split.first << "' while getting analyzer '" << name << "': " << res.errorNumber() << " " << res.errorMessage();
        TRI_set_errno(res.errorNumber());

        return nullptr;
      }
    }

    ReadMutex mutex(_mutex);
    SCOPED_LOCK(mutex);
    auto itr =
        _analyzers.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>()));

    if (itr == _analyzers.end()) {
      LOG_TOPIC("4049d", WARN, arangodb::iresearch::TOPIC)
          << "failure to find arangosearch analyzer name '" << name << "'";

      return nullptr;
    }

    auto pool = itr->second;

    if (pool) {
      return pool;
    }

    LOG_TOPIC("1a29c", WARN, arangodb::iresearch::TOPIC)
        << "failure to get arangosearch analyzer name '" << name << "'";
    TRI_set_errno(TRI_ERROR_INTERNAL);
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC("29eff", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while retrieving an arangosearch analizer name '"
        << name << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC("ce8d5", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while retrieving an arangosearch analizer name '"
        << name << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC("5505f", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while retrieving an arangosearch analizer name '"
        << name << "'";
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

IResearchAnalyzerFeature::AnalyzerPool::ptr IResearchAnalyzerFeature::get( // find analyzer
  irs::string_ref const& name, // analyzer name
  irs::string_ref const& type, // analyzer type
  irs::string_ref const& properties, // analyzer properties
  irs::flags const& features // analyzer features
) {
  EmplaceResult result;
  auto res = ensure( // find and validate analyzer
    result, // result
    name, // analyzer name
    type, // analyzer type
    properties, // analyzer properties
    features, // analyzer features
    false
  );

  if (!res.ok()) {
    LOG_TOPIC("ed6a3", WARN, arangodb::iresearch::TOPIC)
      << "failure to get arangosearch analyzer name '" << name << "': " << res.errorNumber() << " " << res.errorMessage();
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return nullptr;
  }

  return result.first;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a container of statically defined/initialized analyzers
////////////////////////////////////////////////////////////////////////////////
/*static*/ IResearchAnalyzerFeature::Analyzers const& IResearchAnalyzerFeature::getStaticAnalyzers() {
  struct Instance {
    Analyzers analyzers;
    Instance() {
      // register the indentity analyzer
      {
        irs::flags const extraFeatures = {irs::frequency::type(), irs::norm::type()};
        irs::string_ref const name("identity");
        PTR_NAMED(AnalyzerPool, pool, name);

        if (!pool || !pool->init(IdentityAnalyzer::type().name(),
                                 irs::string_ref::NIL, extraFeatures)) {
          LOG_TOPIC("26de1", WARN, arangodb::iresearch::TOPIC)
              << "failure creating an arangosearch static analyzer instance "
                 "for name '"
              << name << "'";

          // this should never happen, treat as an assertion failure
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "failed to create arangosearch static analyzer");
        }

        analyzers.emplace(irs::make_hashed_ref(name, std::hash<irs::string_ref>()), pool);
      }

      auto* databaseFeature =
          application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
              "Database");

      // check if DB is currently being upgraded (load all legacy built-in analyzers)
      bool const inUpgrade = databaseFeature
        ? (databaseFeature->upgrade() || databaseFeature->checkVersion())
        : false;

      if (!inUpgrade) {
        return;
      }

      // register the text analyzers
      {
        // Note: ArangoDB strings coming from JavaScript user input are UTF-8 encoded
        std::vector<std::pair<irs::string_ref, irs::string_ref>> const textAnalzyers = {
          {"text_de", "{ \"locale\": \"de.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
          {"text_en", "{ \"locale\": \"en.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
          {"text_es", "{ \"locale\": \"es.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
          {"text_fi", "{ \"locale\": \"fi.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
          {"text_fr", "{ \"locale\": \"fr.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
          {"text_it", "{ \"locale\": \"it.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
          {"text_nl", "{ \"locale\": \"nl.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
          {"text_no", "{ \"locale\": \"no.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
          {"text_pt", "{ \"locale\": \"pt.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
          {"text_ru", "{ \"locale\": \"ru.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
          {"text_sv", "{ \"locale\": \"sv.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
          {"text_zh", "{ \"locale\": \"zh.UTF-8\", \"ignored_words\": [ ] " "}"},  // empty stop word list
        };
        irs::flags const extraFeatures = {
          irs::frequency::type(), irs::norm::type(), irs::position::type()
        };  // add norms + frequency/position for by_phrase

        irs::string_ref const type("text");

        for (auto& entry : textAnalzyers) {
          auto& name = entry.first;
          auto& args = entry.second;
          PTR_NAMED(AnalyzerPool, pool, name);

          if (!pool || !pool->init(type, args, extraFeatures)) {
            LOG_TOPIC("e25f5", WARN, arangodb::iresearch::TOPIC)
                << "failure creating an arangosearch static analyzer instance "
                   "for name '"
                << name << "'";

            // this should never happen, treat as an assertion failure
            THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
          }

          analyzers.emplace(irs::make_hashed_ref(name, std::hash<irs::string_ref>()), pool);
        }
      }
    }
  };
  static const Instance instance;

  return instance.analyzers;
}

/*static*/ IResearchAnalyzerFeature::AnalyzerPool::ptr IResearchAnalyzerFeature::identity() noexcept {
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

arangodb::Result IResearchAnalyzerFeature::loadAnalyzers( // load
  irs::string_ref const& database /*= irs::string_ref::NIL*/ // database to load
) {
  try {
    auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
      arangodb::DatabaseFeature // feature type
    >("Database");

    if (!dbFeature) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failure to find feature 'Database' while loading analyzers for database '") + std::string(database)+ "'"
      );
    }

    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // '_analyzers'/'_lastLoad' can be asynchronously read

    // load all databases
    if (database.null()) {
      arangodb::Result res;
      std::unordered_set<irs::hashed_string_ref> seen;
      auto visitor = [this, &res, &seen](TRI_vocbase_t& vocbase)->void {
        if (!vocbase.lookupCollection(ANALYZER_COLLECTION_NAME)) {
          return; // skip databases lacking 'ANALYZER_COLLECTION_NAME' (no analyzers there, not an error)
        }

        auto name = irs::make_hashed_ref( // vocbase name
          irs::string_ref(vocbase.name()), std::hash<irs::string_ref>() // args
        );
        auto result = loadAnalyzers(name);
        auto itr = _lastLoad.find(name);

        if (itr != _lastLoad.end()) {
          seen.insert(name);
        } else if (res.ok()) { // load errors take precedence
          res = arangodb::Result( // result
            TRI_ERROR_INTERNAL, // code
            "failure to find database last load timestamp after loading analyzers" // message
          );
        }

        if (!result.ok()) {
          res = result;
        }
      };

      dbFeature->enumerateDatabases(visitor);

      std::unordered_set<std::string> unseen; // make copy since original removed

      // remove unseen databases from timestamp list
      for (auto itr = _lastLoad.begin(), end = _lastLoad.end(); itr != end;) {
        auto name = irs::make_hashed_ref( // vocbase name
          irs::string_ref(itr->first), std::hash<irs::string_ref>() // args
        );
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
        auto unseenItr = // ignore static analyzers
          split.first.null() ? unseen.end() : unseen.find(split.first);

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
    auto* engine = arangodb::EngineSelectorFeature::ENGINE;
    auto itr = _lastLoad.find(databaseKey); // find last update timestamp

    if (!engine || engine->inRecovery()) {
      // always load if inRecovery since collection contents might have changed
      // unless on db-server which does not store analyzer definitions in collections
      if (arangodb::ServerState::instance()->isDBServer()) {
        return arangodb::Result(); // db-server should not access cluster during inRecovery
      }
    } else if (arangodb::ServerState::instance()->isSingleServer()) { // single server
      if(itr != _lastLoad.end()) {
        return arangodb::Result(); // do not reload on single-server
      }
    } else if (itr != _lastLoad.end() // had a previous load
               && itr->second + RELOAD_INTERVAL > currentTimestamp // timeout not reached
              ) {
      return arangodb::Result(); // reload interval not reached
    }

    auto* vocbase = dbFeature->lookupDatabase(database);
    static auto cleanupAnalyzers = []( // remove invalid analyzers
      IResearchAnalyzerFeature& feature, // analyzer feature
      decltype(_lastLoad)::iterator& lastLoadItr, // iterator
      irs::string_ref const& database // database
    )->void {
      if (lastLoadItr == feature._lastLoad.end()) {
        return; // nothing to do (if not in '_lastLoad' then not in '_analyzers')
      }

      // remove invalid database and analyzers
      feature._lastLoad.erase(lastLoadItr);

      // remove no longer valid analyzers (force remove)
      for (auto itr = feature._analyzers.begin(),
           end = feature._analyzers.end();
           itr != end;
          ) {
        auto split = splitAnalyzerName(itr->first);

        if (split.first == database) {
          itr = feature._analyzers.erase(itr);
        } else {
          ++itr;
        }
      }
    };

    if (!vocbase) {
      if (engine && engine->inRecovery()) {
        return arangodb::Result(); // database might not have come up yet
      }

      cleanupAnalyzers(*this, itr, database); // cleanup any analyzers for 'database'

      return arangodb::Result( // result
        TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, // code
        std::string("failed to find database '") + std::string(database) + "' while loading analyzers"
      );
    }

    if (!getAnalyzerCollection(*vocbase)) {
      cleanupAnalyzers(*this, itr, database); // cleanup any analyzers for 'database'
      _lastLoad[databaseKey] = currentTimestamp; // update timestamp

      return arangodb::Result(); // no collection means nothing to load
    }

    Analyzers analyzers;
    auto visitor = [this, &analyzers, &vocbase]( // visitor
      arangodb::velocypack::Slice const& slice // analyzer definition
    )->arangodb::Result {
      if (!slice.isObject()) {
        LOG_TOPIC("5c7a5", ERR, arangodb::iresearch::TOPIC)
          << "failed to find an object value for analyzer definition while loading analyzer form collection '" << ANALYZER_COLLECTION_NAME << "' in database '" << vocbase->name() + "', skipping it: " << slice.toString();

        return arangodb::Result(); // skip analyzer
      }

      irs::flags features;
      irs::string_ref key;
      irs::string_ref name;
      irs::string_ref type;
      irs::string_ref properties;
      std::string propertiesBuf;

      if (!slice.hasKey(arangodb::StaticStrings::KeyString) // no such field (required)
          || !slice.get(arangodb::StaticStrings::KeyString).isString()) {
        LOG_TOPIC("1dc56", ERR, arangodb::iresearch::TOPIC)
          << "failed to find a string value for analyzer '" << arangodb::StaticStrings::KeyString << "' while loading analyzer form collection '" << ANALYZER_COLLECTION_NAME << "' in database '" << vocbase->name() + "', skipping it: " << slice.toString();

        return arangodb::Result(); // skip analyzer
      }

      key = getStringRef(slice.get(arangodb::StaticStrings::KeyString));

      if (!slice.hasKey("name") // no such field (required)
          || !(slice.get("name").isString() || slice.get("name").isNull())) {
        LOG_TOPIC("f5920", ERR, arangodb::iresearch::TOPIC)
          << "failed to find a string value for analyzer 'name' while loading analyzer form collection '" << ANALYZER_COLLECTION_NAME << "' in database '" << vocbase->name() + "', skipping it: " << slice.toString();

        return arangodb::Result(); // skip analyzer
      }

      name = getStringRef(slice.get("name"));

      if (!slice.hasKey("type") // no such field (required)
          || !(slice.get("type").isString() || slice.get("name").isNull())) {
        LOG_TOPIC("9f5c8", ERR, arangodb::iresearch::TOPIC)
          << "failed to find a string value for analyzer 'type' while loading analyzer form collection '" << ANALYZER_COLLECTION_NAME << "' in database '" << vocbase->name() + "', skipping it: " << slice.toString();

        return arangodb::Result(); // skip analyzer
      }

      type = getStringRef(slice.get("type"));

      if (slice.hasKey("properties")) {
        auto subSlice = slice.get("properties");

        // encode jSON array/object as a string for analyzers that support jSON
        if (subSlice.isArray() || subSlice.isObject()) {
          propertiesBuf = subSlice.toJson(); // format as a jSON encoded string
          properties = propertiesBuf;
        } else if (subSlice.isString() || subSlice.isNull()) {
          properties = getStringRef(subSlice);
        } else {
          LOG_TOPIC("a297e", ERR, arangodb::iresearch::TOPIC)
            << "failed to find a string value for analyzer 'properties' while loading analyzer form collection '" << ANALYZER_COLLECTION_NAME << "' in database '" << vocbase->name() + "', skipping it: " << slice.toString();

          return arangodb::Result(); // skip analyzer
        }
      }

      if (slice.hasKey("features")) {
        auto subSlice = slice.get("features");

        if (!subSlice.isArray()) {
          LOG_TOPIC("7ec8a", ERR, arangodb::iresearch::TOPIC)
            << "failed to find an array value for analyzer 'features' while loading analyzer form collection '" << ANALYZER_COLLECTION_NAME << "' in database '" << vocbase->name() + "', skipping it: " << slice.toString();

          return arangodb::Result(); // skip analyzer
        }

        for (arangodb::velocypack::ArrayIterator subItr(subSlice);
             subItr.valid();
             ++subItr
            ) {
          auto subEntry = *subItr;

          if (!subEntry.isString() && !subSlice.isNull()) {
            LOG_TOPIC("7620d", ERR, arangodb::iresearch::TOPIC)
              << "failed to find a string value for an entry in analyzer 'features' while loading analyzer form collection '" << ANALYZER_COLLECTION_NAME << "' in database '" << vocbase->name() + "', skipping it: " << slice.toString();

            return arangodb::Result(); // skip analyzer
          }

          auto featureName = getStringRef(subEntry);
          auto* feature = irs::attribute::type_id::get(featureName);

          if (!feature) {
            LOG_TOPIC("4fedc", ERR, arangodb::iresearch::TOPIC)
              << "failed to find feature '" << featureName << "' while loading analyzer form collection '" << ANALYZER_COLLECTION_NAME << "' in database '" << vocbase->name() + "', skipping it: " << slice.toString();

            return arangodb::Result(); // skip analyzer
          }

          features.add(*feature);
        }
      }

      auto normalizedName = normalizedAnalyzerName(vocbase->name(), name);
      EmplaceAnalyzerResult result;
      auto res = emplaceAnalyzer( // emplace into map
        result, analyzers, normalizedName, type, properties, features // args
      );

      if (!res.ok()) {
        return res; // caught error emplacing analyzer (abort further processing)
      }

      if (result.second && result.first->second) {
        result.first->second->setKey(key); // update key
      }

      return arangodb::Result();
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
              && !equalAnalyzer(*(entry.second), result.first->second->type(), result.first->second->properties(), result.first->second->features())) {
            return arangodb::Result( // result
              TRI_ERROR_BAD_PARAMETER, // code
              std::string("name collision detected while re-registering a duplicate arangosearch analizer name '") + std::string(result.first->second->name()) + "' type '" + std::string(result.first->second->type()) + "' properties '" + std::string(result.first->second->properties()) + "', previous registration type '" + std::string(entry.second->type()) + "' properties '" + std::string(entry.second->properties()) + "'"
            );
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
          && !equalAnalyzer(*(entry.second), itr->second->type(), itr->second->properties(), itr->second->features())) {
        return arangodb::Result( // result
          TRI_ERROR_BAD_PARAMETER, // code
          std::string("name collision detected while registering a duplicate arangosearch analizer name '") + std::string(itr->second->name()) + "' type '" + std::string(itr->second->type()) + "' properties '" + std::string(itr->second->properties()) + "', previous registration type '" + std::string(entry.second->type()) + "' properties '" + std::string(entry.second->properties()) + "'"
        );
      }

      itr->second = entry.second; // reuse old analyzer pool to avoid duplicates in memmory
      const_cast<Analyzers::key_type&>(itr->first) = entry.first; // point key at old pool
    }

    _lastLoad[databaseKey] = currentTimestamp; // update timestamp
    _analyzers = std::move(analyzers); // update mappings
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result( // result
      e.code(), // code
      std::string("caught exception while loading configuration for arangosearch analyzers from database '") + std::string(database) + "': " + std::to_string(e.code()) + " "+ e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while loading configuration for arangosearch analyzers from database '") + std::string(database) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while loading configuration for arangosearch analyzers from database '") + std::string(database) + "'"
    );
  }

  return arangodb::Result();
}

/*static*/ std::string const& IResearchAnalyzerFeature::name() noexcept {
  return FEATURE_NAME;
}

/*static*/ std::string IResearchAnalyzerFeature::normalize( // normalize name
  irs::string_ref const& name, // analyzer name
  TRI_vocbase_t const& activeVocbase, // fallback vocbase if not part of name
  TRI_vocbase_t const& systemVocbase, // the system vocbase for use with empty prefix
  bool expandVocbasePrefix /*= true*/ // use full vocbase name as prefix for active/system v.s. EMPTY/'::'
) {
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
    if (split.first.null() || split.first == activeVocbase.name()) { // active vocbase
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

arangodb::Result IResearchAnalyzerFeature::remove( // remove analyzer
  irs::string_ref const& name, // analyzer name
  bool force /*= false*/ // force removal
) {
  try {
    auto split = splitAnalyzerName(name);

    if (split.first.null()) {
      return arangodb::Result( // result
        TRI_ERROR_FORBIDDEN, // code
        "built-in analyzers cannot be removed" // message
      );
    }

    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    auto itr = // find analyzer
      _analyzers.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>()));

    if (itr == _analyzers.end()) {
      return arangodb::Result( // result
        TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, // code
        std::string("failure to find analyzer while removing arangosearch analyzer '") + std::string(name)+ "'"
      );
    }

    auto& pool = itr->second;

    // this should never happen since analyzers should always be valid, but this
    // will make the state consistent again
    if (!pool) {
      _analyzers.erase(itr);

      return arangodb::Result( // result
        TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, // code
        std::string("failure to find a valid analyzer while removing arangosearch analyzer '") + std::string(name)+ "'"
      );
    }

    if (!force && pool.use_count() > 1) { // +1 for ref in '_analyzers'
      return arangodb::Result( // result
        TRI_ERROR_ARANGO_CONFLICT, // code
        std::string("analyzer in-use while removing arangosearch analyzer '") + std::string(name)+ "'"
      );
    }

    // on db-server analyzers are not persisted
    // allow removal even inRecovery()
    if (arangodb::ServerState::instance()->isDBServer()) {
      _analyzers.erase(itr);

      return arangodb::Result();
    }

    // .........................................................................
    // after here the analyzer must be removed from the persisted store first
    // .........................................................................

    // this should never happen since non-static analyzers should always have a
    // valid '_key' after
    if (pool->_key.null()) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failure to find '") + arangodb::StaticStrings::KeyString + "' while removing arangosearch analyzer '" + std::string(name)+ "'"
      );
    }

    auto* engine = arangodb::EngineSelectorFeature::ENGINE;

    // do not allow persistence while in recovery
    if (engine && engine->inRecovery()) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failure to remove arangosearch analyzer '") + std::string(name)+ "' configuration while storage engine in recovery"
      );
    }

    auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
      arangodb::DatabaseFeature // feature type
    >("Database");

    if (!dbFeature) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failure to find feature 'Database' while removing arangosearch analyzer '") + std::string(name)+ "'"
      );
    }

    auto* vocbase = dbFeature->useDatabase(split.first);

    if (!vocbase) {
      return arangodb::Result( // result
        TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, // code
        std::string("failure to find vocbase while removing arangosearch analyzer '") + std::string(name)+ "'"
      );
    }

    std::shared_ptr<arangodb::LogicalCollection> collection;
    auto collectionCallback = [&collection]( // store collection
      std::shared_ptr<arangodb::LogicalCollection> const& col // args
    )->void {
      collection = col;
    };

    arangodb::methods::Collections::lookup( // find collection
      *vocbase, ANALYZER_COLLECTION_NAME, collectionCallback // args
    );

    if (!collection) {
      return arangodb::Result( // result
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, // code
        std::string("failure to find collection while removing arangosearch analyzer '") + std::string(name)+ "'"
      );
    }

    arangodb::SingleCollectionTransaction trx( // transaction
      arangodb::transaction::StandaloneContext::Create(*vocbase), // transaction context
      ANALYZER_COLLECTION_NAME, // collection name
      arangodb::AccessMode::Type::WRITE // collection access type
    );
    auto res = trx.begin();

    if (!res.ok()) {
      return res;
    }

    arangodb::velocypack::Builder builder;
    arangodb::OperationOptions options;

    builder.openObject();
      addStringRef(builder, arangodb::StaticStrings::KeyString, pool->_key);
    builder.close();

    auto result = // remove
      trx.remove(ANALYZER_COLLECTION_NAME, builder.slice(), options);

    if (!result.ok()) {
      trx.abort();

      return result.result;
    }

    _analyzers.erase(itr);
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result( // result
      e.code(), // code
      std::string("caught exception while removing configuration for arangosearch analyzer name '") + std::string(name) + "': " + std::to_string(e.code()) + " "+ e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while removing configuration for arangosearch analyzer name '") + std::string(name) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while removing configuration for arangosearch analyzer name '") + std::string(name) + "'"
    );
  }

  return arangodb::Result();
}

void IResearchAnalyzerFeature::start() {
  if (!isEnabled()) {
    return;
  }

  // register analyzer functions
  {
    auto* functions =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::aql::AqlFunctionFeature>(
            "AQLFunctions");

    if (functions) {
      addFunctions(*functions);
    } else {
      LOG_TOPIC("7dcd9", WARN, arangodb::iresearch::TOPIC)
          << "failure to find feature 'AQLFunctions' while registering "
             "IResearch functions";
    }
  }

  registerUpgradeTasks(); // register tasks after UpgradeFeature::prepare() has finished

  auto res = loadAnalyzers();

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
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

arangodb::Result IResearchAnalyzerFeature::storeAnalyzer(AnalyzerPool& pool) {
  auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::DatabaseFeature // feature type
  >("Database");

  if (!dbFeature) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("failure to find feature 'Database' while persising arangosearch analyzer '") + pool.name()+ "'"
    );
  }

  if (pool.type().null()) {
    return arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("failure to persist arangosearch analyzer '") + pool.name()+ "' configuration with 'null' type"
    );
  }

  auto* engine = arangodb::EngineSelectorFeature::ENGINE;

  // do not allow persistence while in recovery
  if (engine && engine->inRecovery()) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("failure to persist arangosearch analyzer '") + pool.name()+ "' configuration while storage engine in recovery"
    );
  }

  auto split = splitAnalyzerName(pool.name());
  auto* vocbase = dbFeature->useDatabase(split.first);

  if (!vocbase) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("failure to find vocbase while persising arangosearch analyzer '") + pool.name()+ "'"
    );
  }

  try {
    auto collection = getAnalyzerCollection(*vocbase);

    if (!collection) {
      auto collectionCallback = [&collection]( // store collection
        std::shared_ptr<arangodb::LogicalCollection> const& col // args
      )->void {
        collection = col;
      };
      static auto const properties = // analyzer collection properties
        arangodb::velocypack::Parser::fromJson("{ \"isSystem\": true }");
      auto res = arangodb::methods::Collections::create( // create collection
        *vocbase, // collection vocbase
        ANALYZER_COLLECTION_NAME, // collection name
        TRI_col_type_e::TRI_COL_TYPE_DOCUMENT, // collection type
        properties->slice(), // collection properties
        true, // waitsForSyncReplication same as UpgradeTasks::createSystemCollection(...)
        true, // enforceReplicationFactor same as UpgradeTasks::createSystemCollection(...)
        collectionCallback // callback if created
      );

      if (!res.ok()) {
        return res;
      }

      if (!collection) {
        return arangodb::Result( // result
          TRI_ERROR_INTERNAL, // code
          std::string("failure to create collection '") + ANALYZER_COLLECTION_NAME + "' in vocbase '" + vocbase->name() + "' vocbase while persising arangosearch analyzer '" + pool.name()+ "'"
        );
      }
    }

    arangodb::SingleCollectionTransaction trx( // transaction
      arangodb::transaction::StandaloneContext::Create(*vocbase), // transaction context
      ANALYZER_COLLECTION_NAME, // collection name
      arangodb::AccessMode::Type::WRITE // collection access type
    );
    auto res = trx.begin();

    if (!res.ok()) {
      return res;
    }

    arangodb::velocypack::Builder builder;
    arangodb::OperationOptions options;

    builder.openObject();
    addStringRef(builder, "name", split.second);
    addStringRef(builder, "type", pool.type());
    addStringRef(builder, "properties", pool.properties());

    // only add features if there are present
    if (!pool.features().empty()) {
      builder.add( // add array
        "features", // name
        arangodb::velocypack::Value(arangodb::velocypack::ValueType::Array) // value
      );

      for (auto& feature: pool.features()) {
        // this should never happen since irs::flags currently provides no way
        // to set nullptr
        if (!feature) {
          return arangodb::Result( // result
            TRI_ERROR_INTERNAL, // code
            std::string("failure to to add null feature persising arangosearch analyzer '") + pool.name()+ "'"
          );
        }

        addStringRef(builder, feature->name());
      }

      builder.close();
    }

    builder.close();
    options.waitForSync = true;

    auto result = // insert
      trx.insert(ANALYZER_COLLECTION_NAME, builder.slice(), options);

    if (!result.ok()) {
      trx.abort();

      return result.result;
    }

    auto slice = result.slice();

    if (!slice.isObject()) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failure to parse result as a JSON object while persisting configuration for arangosearch analyzer name '") + pool.name() + "'"
      );
    }

    auto key = slice.get(arangodb::StaticStrings::KeyString);

    if (!key.isString()) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failure to find the resulting key field while persisting configuration for arangosearch analyzer name '") + pool.name() + "'"
      );
    }

    res = trx.commit();

    if (!res.ok()) {
      trx.abort();

      return res;
    }

    pool.setKey(getStringRef(key));
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result( // result
      e.code(), // code
      std::string("caught exception while persisting configuration for arangosearch analyzer name '") + pool.name() + "': " + std::to_string(e.code()) + " "+ e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while persisting configuration for arangosearch analyzer name '") + pool.name() + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while persisting configuration for arangosearch analyzer name '") + pool.name() + "'"
    );
  }

  return arangodb::Result();
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
    LOG_TOPIC("73695", WARN, arangodb::iresearch::TOPIC)
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

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
