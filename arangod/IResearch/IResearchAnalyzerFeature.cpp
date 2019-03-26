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
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "IResearchAnalyzerFeature.h"
#include "IResearchCommon.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
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
static size_t const DEFAULT_POOL_SIZE = 8;  // arbitrary value
static std::string const FEATURE_NAME("IResearchAnalyzer");
static irs::string_ref const IDENTITY_ANALYZER_NAME("identity");

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

    builder.add(arangodb::iresearch::toValuePair(value));
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
/// @brief ensure configuration collection is present in the specified vocbase
////////////////////////////////////////////////////////////////////////////////
void ensureConfigCollection(TRI_vocbase_t& vocbase) {
  static const std::string json =
      std::string("{\"isSystem\": true, \"name\": \"") +
      ANALYZER_COLLECTION_NAME + "\", \"type\": 2}";

  if (!arangodb::ServerState::instance()->isCoordinator()) {
    try {
      vocbase.createCollection(arangodb::velocypack::Parser::fromJson(json)->slice());
    } catch (arangodb::basics::Exception& e) {
      if (TRI_ERROR_ARANGO_DUPLICATE_NAME != e.code()) {
        throw;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the system database or nullptr on error
////////////////////////////////////////////////////////////////////////////////
arangodb::SystemDatabaseFeature::ptr getSystemDatabase() {
  auto* database =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();

  if (!database) {
    LOG_TOPIC("f8c9f", WARN, arangodb::iresearch::TOPIC)
        << "failure to find feature '" << arangodb::SystemDatabaseFeature::name()
        << "' while getting the system database";

    return nullptr;
  }

  return database->use();
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

    // reguster each legacy static analyzer with the current vocbase
    for (auto& entry: legacyAnalzyers) {
      auto name = // prefix name with vocbase
        std::string(vocbase.name()).append(2, ANALYZER_PREFIX_DELIM).append(entry.first);
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
/// @brief validate that features are supported by arangod an ensure that their
///        dependencies are met
////////////////////////////////////////////////////////////////////////////////
arangodb::Result validateFeatures(irs::flags const& features) {
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

  return arangodb::Result();
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
  return irs::analysis::analyzers::get(type, irs::text_format::json, properties);
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
    : ApplicationFeature(server, IResearchAnalyzerFeature::name()),
      _analyzers(getStaticAnalyzers()),  // load static analyzers
      _started(false) {
  setOptional(true);
  startsAfter("V8Phase");

  startsAfter("AQLFunctions");  // used for registering IResearch analyzer functions
  startsAfter("SystemDatabase");  // used for getting the system database
                                  // containing the persisted configuration
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
  irs::string_ref const& analyzer, // analyzer name
  TRI_vocbase_t const& defaultVocbase, // fallback vocbase if not part of name
  arangodb::auth::Level const& level // access level
) {
  auto* ctx = arangodb::ExecContext::CURRENT;

  if (!ctx) {
    return true; // authentication not enabled
  }

  auto& staticAnalyzers = getStaticAnalyzers();

  if (staticAnalyzers.find(irs::make_hashed_ref(analyzer, std::hash<irs::string_ref>())) != staticAnalyzers.end()) {
    return true; // special case for singleton static analyzers (always allowed)
  }

  auto* sysDatabase = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::SystemDatabaseFeature // featue type
  >();
  auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;
  std::pair<irs::string_ref, irs::string_ref> split;

  if (sysVocbase) {
    split = splitAnalyzerName( // split analyzer name
      arangodb::iresearch::IResearchAnalyzerFeature::normalize( // normalize
        analyzer, defaultVocbase, *sysVocbase // args
      )
    );
  } else {
    split = splitAnalyzerName(analyzer);
  }

  return !split.first.null() // have a vocbase
    && ctx->canUseDatabase(split.first, level) // can use vocbase
    && ctx->canUseCollection(split.first, ANALYZER_COLLECTION_NAME, level); // can use analyzers
}

std::pair<IResearchAnalyzerFeature::AnalyzerPool::ptr, bool> IResearchAnalyzerFeature::emplace(
    irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties,
    irs::flags const& features /*= irs::flags::empty_instance()*/
    ) noexcept {
  return emplace(name, type, properties, true, features);
}

std::pair<IResearchAnalyzerFeature::AnalyzerPool::ptr, bool> IResearchAnalyzerFeature::emplace(
    irs::string_ref const& name, irs::string_ref const& type,
    irs::string_ref const& properties, bool initAndPersist,
    irs::flags const& features /*= irs::flags::empty_instance()*/
    ) noexcept {
  try {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    auto generator = [](irs::hashed_string_ref const& key,
                        AnalyzerPool::ptr const& value) -> irs::hashed_string_ref {
      PTR_NAMED(AnalyzerPool, pool, key);
      const_cast<AnalyzerPool::ptr&>(value) = pool;  // lazy-instantiate pool
      return pool ? irs::hashed_string_ref(key.hash(), pool->name())
                  : key;  // reuse hash but point ref at value in pool
    };
    auto itr = irs::map_utils::try_emplace_update_key(
        _analyzers, generator, irs::make_hashed_ref(name, std::hash<irs::string_ref>()));
    bool erase = itr.second;
    auto cleanup = irs::make_finally([&erase, this, &itr]() -> void {
      if (erase) {
        _analyzers.erase(itr.first);  // ensure no broken analyzers are left behind
      }
    });

    auto pool = itr.first->second;

    if (!pool) {
      LOG_TOPIC("2bdc6", WARN, arangodb::iresearch::TOPIC)
          << "failure creating an arangosearch analyzer instance for name '"
          << name << "' type '" << type << "' properties '" << properties << "'";
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

      return std::make_pair(AnalyzerPool::ptr(), false);
    }

    // skip initialization and persistance
    if (!initAndPersist) {
      erase = false;

      return std::make_pair(pool, itr.second);
    }

    if (itr.second) {  // new pool
      if (!_started) {
        LOG_TOPIC("b8db7", WARN, arangodb::iresearch::TOPIC)
            << "cannot garantee collision-free persistance while creating an "
               "arangosearch analyzer instance for name '"
            << name << "' type '" << type << "' properties '" << properties << "'";
        TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);

        return std::make_pair(AnalyzerPool::ptr(), false);
      }

      if (!pool->init(type, properties, features)) {
        LOG_TOPIC("dfe77", WARN, arangodb::iresearch::TOPIC)
            << "failure initializing an arangosearch analyzer instance for "
               "name '"
            << name << "' type '" << type << "' properties '" << properties << "'";
        TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

        return std::make_pair(AnalyzerPool::ptr(), false);
      }

      auto res = storeAnalyzer(*pool);

      if (!res.ok()) {
        LOG_TOPIC("ab445", WARN, arangodb::iresearch::TOPIC)
          << "failure persisting an arangosearch analyzer instance for name '"
          << name << "' type '" << type << "' properties '" << properties
          << "': " << res.errorNumber() << " " << res.errorMessage();
        TRI_set_errno(res.errorNumber());

        return std::make_pair(AnalyzerPool::ptr(), false);
      }

      _customAnalyzers[itr.first->first] = itr.first->second;  // mark as custom
      erase = false;
    } else if (type != pool->_type || properties != pool->_properties) {
      LOG_TOPIC("f3d4e", WARN, arangodb::iresearch::TOPIC)
          << "name collision detected while registering an arangosearch "
             "analizer name '"
          << name << "' type '" << type << "' properties '" << properties
          << "', previous registration type '" << pool->_type
          << "' properties '" << pool->_properties << "'";
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

      return std::make_pair(AnalyzerPool::ptr(), false);
    } else if (pool->_key.null()) { // not yet persisted
      auto res = storeAnalyzer(*pool);

      if (!res.ok()) {
        LOG_TOPIC("322a6", WARN, arangodb::iresearch::TOPIC)
          << "failure persisting an arangosearch analyzer instance for name '"
          << name << "' type '" << type << "' properties '" << properties
          << "': " << res.errorNumber() << " " << res.errorMessage();
        TRI_set_errno(res.errorNumber());

        return std::make_pair(AnalyzerPool::ptr(), false);
      }
    }

    return std::make_pair(pool, itr.second);
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC("ac568", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while registering an arangosearch analizer name '"
        << name << "' type '" << type << "' properties '" << properties
        << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC("8f3b4", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while registering an arangosearch analizer name '"
        << name << "' type '" << type << "' properties '" << properties
        << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC("9030d", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while registering an arangosearch analizer name '"
        << name << "' type '" << type << "' properties '" << properties << "'";
    IR_LOG_EXCEPTION();
  }

  return std::make_pair(AnalyzerPool::ptr(), false);
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

IResearchAnalyzerFeature::AnalyzerPool::ptr IResearchAnalyzerFeature::ensure( // get analyzer or placeholder
    irs::string_ref const& name // analyzer name
) {
  // insert dummy (uninitialized) placeholders if this feature has not been
  // started to break the dependency loop on DatabaseFeature
  // placeholders will be loaded/validation during start()/loadConfiguration()
  return _started
    ? get(name)
    : emplace(name, irs::string_ref::NIL, irs::string_ref::NIL, false).first;
}

arangodb::Result IResearchAnalyzerFeature::ensure( // ensure analyzer existence if possible
  EmplaceResult& result, // emplacement result on success (out-param)
  irs::string_ref const& name, // analyzer name
  irs::string_ref const& type, // analyzer type
  irs::string_ref const& properties, // analyzer properties
  irs::flags const& features, // analyzer features
  bool allowCreation
) {
  try {
    static const auto generator = []( // key + value generator
      irs::hashed_string_ref const& key, // source key
      AnalyzerPool::ptr const& value // source value
    )->irs::hashed_string_ref {
      auto pool = std::make_shared<AnalyzerPool>(key); // allocate pool
      const_cast<AnalyzerPool::ptr&>(value) = pool; // lazy-instantiate pool to avoid allocation if pool is already present
      return pool ? irs::hashed_string_ref(key.hash(), pool->name()) : key; // reuse hash but point ref at value in pool
    };
    auto res = validateFeatures(features);

    if (!res.ok()) {
      return res;
    }

    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    auto itr = irs::map_utils::try_emplace_update_key( // emplace and update key
      _analyzers, // destination
      generator, // key generator
      irs::make_hashed_ref(name, std::hash<irs::string_ref>()) // key
    );
    bool erase = itr.second; // an insertion took place
    auto cleanup = irs::make_finally([&erase, this, &itr]()->void {
      if (erase) {
        _analyzers.erase(itr.first); // ensure no broken analyzers are left behind
      }
    });
    auto pool = itr.first->second;

    if (!pool) {
      return arangodb::Result( // result
        TRI_ERROR_BAD_PARAMETER, // code
        std::string("failure creating an arangosearch analyzer instance for name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "'"
      );
    }

    // new pool creation
    if (itr.second) {
      if (!pool->init(type, properties, features)) {
        return arangodb::Result( // result
          TRI_ERROR_BAD_PARAMETER, // code
          std::string("failure initializing an arangosearch analyzer instance for name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "'"
        );
      }

      if (!allowCreation) {
        return arangodb::Result( // result
          TRI_ERROR_BAD_PARAMETER, // code
          std::string("forbidden implicit creation of an arangosearch analyzer instance for name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "'"
        );
      }

      // persist only on coordinator and single-server
      res = arangodb::ServerState::instance()->isCoordinator() // coordinator
            || arangodb::ServerState::instance()->isSingleServer() // single-server
          ? storeAnalyzer(*pool) : arangodb::Result();

      if (res.ok()) {
        result = std::make_pair(pool, itr.second);
        erase = false; // successful pool creation, cleanup not required
      }

      return res;
    }

    // pool exists but with different configuration
    if (type != pool->type() // different type
        || properties != pool->properties() // different properties
        || features != pool->features() // different features
       ) {
      return arangodb::Result( // result
        TRI_ERROR_BAD_PARAMETER, // code
        std::string("name collision detected while registering an arangosearch analizer name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "', previous registration type '" + std::string(pool->type()) + "' properties '" + std::string(pool->properties()) + "'"
      );
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
    irs::string_ref const& name // analyzer name
) const noexcept {
  try {
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
    arangodb::ServerState::instance()->isDBServer() // create analyzer only if on db-server
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
        static const irs::flags extraFeatures = {irs::frequency::type(), irs::norm::type()};
        static const irs::string_ref name("identity");
        PTR_NAMED(AnalyzerPool, pool, name);

        if (!pool || !pool->init(IdentityAnalyzer::type().name(),
                                 irs::string_ref::NIL, extraFeatures)) {
          LOG_TOPIC("26de1", WARN, arangodb::iresearch::TOPIC)
              << "failure creating an arangosearch static analyzer instance "
                 "for name '"
              << name << "'";
          throw irs::illegal_state();  // this should never happen, treat as an
                                       // assertion failure
        }

        analyzers.emplace(irs::make_hashed_ref(name, std::hash<irs::string_ref>()), pool);
      }

      // register the text analyzers
      {
        // Note: ArangoDB strings coming from JavaScript user input are UTF-8
        // encoded
        static const std::vector<std::pair<irs::string_ref, irs::string_ref>> textAnalzyers = {
            {"text_de",
             "{ \"locale\": \"de.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
            {"text_en",
             "{ \"locale\": \"en.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
            {"text_es",
             "{ \"locale\": \"es.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
            {"text_fi",
             "{ \"locale\": \"fi.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
            {"text_fr",
             "{ \"locale\": \"fr.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
            {"text_it",
             "{ \"locale\": \"it.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
            {"text_nl",
             "{ \"locale\": \"nl.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
            {"text_no",
             "{ \"locale\": \"no.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
            {"text_pt",
             "{ \"locale\": \"pt.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
            {"text_ru",
             "{ \"locale\": \"ru.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
            {"text_sv",
             "{ \"locale\": \"sv.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
            {"text_zh",
             "{ \"locale\": \"zh.UTF-8\", \"ignored_words\": [ ] "
             "}"},  // empty stop word list
        };
        static const irs::flags extraFeatures = {irs::frequency::type(),
                                                 irs::norm::type(),
                                                 irs::position::type()};  // add norms + frequency/position for
                                                                          // by_phrase
        static const irs::string_ref type("text");

        for (auto& entry : textAnalzyers) {
          auto& name = entry.first;
          auto& args = entry.second;
          PTR_NAMED(AnalyzerPool, pool, name);

          if (!pool || !pool->init(type, args, extraFeatures)) {
            LOG_TOPIC("e25f5", WARN, arangodb::iresearch::TOPIC)
                << "failure creating an arangosearch static analyzer instance "
                   "for name '"
                << name << "'";
            throw irs::illegal_state();  // this should never happen, treat as
                                         // an assertion failure
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

bool IResearchAnalyzerFeature::loadConfiguration() {
  if (arangodb::ServerState::instance()->isRunningInCluster()) {
    // the following code will not be working in the cluster
    // safe to access since loadConfiguration(...) is called from start() which
    // is single-thread
    return _customAnalyzers.empty();
  }

  auto vocbase = getSystemDatabase();

  if (!vocbase) {
    LOG_TOPIC("3cc73", WARN, arangodb::iresearch::TOPIC)
        << "failure to get system database while loading arangosearch analyzer "
           "persisted configuration";

    return false;
  }

  arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      ANALYZER_COLLECTION_NAME, arangodb::AccessMode::Type::READ);
  auto res = trx.begin();

  if (!res.ok()) {
    LOG_TOPIC("16400", WARN, arangodb::iresearch::TOPIC)
        << "failure to start transaction while loading arangosearch analyzer "
           "persisted configuration";

    return false;
  }

  auto* collection = trx.documentCollection();

  if (!collection) {
    LOG_TOPIC("ead4b", WARN, arangodb::iresearch::TOPIC)
        << "failure to get collection while loading arangosearch analyzer "
           "persisted configuration";
    trx.abort();

    return false;
  }

  std::unordered_map<irs::string_ref, AnalyzerPool::ptr> initialized;
  auto visitor = [this, &trx, collection, &initialized](LocalDocumentId const& token) -> bool {
    ManagedDocumentResult result;

    if (!collection->readDocument(&trx, token, result)) {
      LOG_TOPIC("377b5", WARN, arangodb::iresearch::TOPIC)
          << "skipping failed read of an arangosearch analyzer persisted "
             "configuration token: "
          << token.id();

      return true;  // failed to read document, skip
    }

    arangodb::velocypack::Slice slice(result.vpack());

    if (!slice.isObject() || !slice.hasKey(arangodb::StaticStrings::KeyString) ||
        !slice.get(arangodb::StaticStrings::KeyString).isString() ||
        !slice.hasKey("name") || !slice.get("name").isString() || !slice.hasKey("type") ||
        !slice.get("type").isString() || !slice.hasKey("properties") ||
        !(slice.get("properties").isNull() || slice.get("properties").isString() ||
          slice.get("properties").isArray() || slice.get("properties").isObject())) {
      LOG_TOPIC("e8578", WARN, arangodb::iresearch::TOPIC)
          << "skipping invalid arangosearch analyzer persisted configuration "
             "entry: "
          << slice.toJson();

      return true;  // not a valid configuration, skip
    }

    auto key = getStringRef(slice.get(arangodb::StaticStrings::KeyString));
    auto name = getStringRef(slice.get("name"));
    auto type = getStringRef(slice.get("type"));
    irs::string_ref properties;
    auto propertiesSlice = slice.get("properties");
    std::string tmpString;

    // encode jSON array/object as a string for analyzers that support jSON
    if (propertiesSlice.isArray() || propertiesSlice.isObject()) {
      tmpString = propertiesSlice.toJson();  // format as a jSON encoded string
      properties = tmpString;
    } else {
      properties = getStringRef(propertiesSlice);
    }

    auto normalizedName = // normalized name
      collection->vocbase().name() + "::" + std::string(name);
    auto& staticAnalyzersMap = getStaticAnalyzers();

    if (staticAnalyzersMap.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>())) == staticAnalyzersMap.end()) {
      name = normalizedName; // use normalized name if it's not a static analyzer
    }

    auto entry = emplace(name, type, properties,
                         false);  // do not persist since this config is already
                                  // coming from the persisted store
    auto& pool = entry.first;

    if (!pool) {
      LOG_TOPIC("91db1", WARN, arangodb::iresearch::TOPIC)
          << "failure creating an arangosearch analyzer instance for name '"
          << name << "' type '" << type << "' properties '" << properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          std::string(
              "failure creating an arangosearch analyzer instance for name '") +
              std::string(name) + "' type '" + std::string(type) +
              "' properties '" + std::string(properties) + "'");
    }

    if (!entry.second && initialized.find(name) != initialized.end()) {
      LOG_TOPIC("1eb5e", WARN, arangodb::iresearch::TOPIC)
          << "name collision detected while registering an arangosearch "
             "analizer name '"
          << name << "' type '" << type << "' properties '" << properties
          << "', previous registration type '" << pool->_type
          << "' properties '" << pool->_properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          std::string("name collision detected while registering an "
                      "arangosearch analizer name '") +
              std::string(name) + "' type '" + std::string(type) +
              "' properties '" + std::string(properties) +
              "', previous registration type '" + std::string(pool->_type) +
              "' properties '" + std::string(pool->_properties) + "'");
    }

    static auto& staticAnalyzers = getStaticAnalyzers();

    if (!entry.second &&
        staticAnalyzers.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>())) !=
            staticAnalyzers.end()) {
      if (type != pool->_type || properties != pool->_properties) {
        LOG_TOPIC("d3a5e", WARN, arangodb::iresearch::TOPIC)
            << "name collision with a static analyzer detected while "
               "registering an arangosearch analizer name '"
            << name << "' type '" << type << "' properties '" << properties
            << "', previous registration type '" << pool->_type
            << "' properties '" << pool->_properties << "'";

        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            std::string("name collision with a static analyzer detected while "
                        "registering an arangosearch analizer name '") +
                std::string(name) + "' type '" + std::string(type) +
                "' properties '" + std::string(properties) +
                "', previous registration type '" + std::string(pool->_type) +
                "' properties '" + std::string(pool->_properties) + "'");
      }

      LOG_TOPIC("a0a10", WARN, arangodb::iresearch::TOPIC)
          << "name collision with a static analyzer detected while registering "
             "an arangosearch analizer name '"
          << name << "' type '" << type << "' properties '" << properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          std::string("name collision with a static analyzer detected while "
                      "registering an arangosearch analizer name '") +
              std::string(name) + "' type '" + std::string(type) +
              "' properties '" + std::string(properties) + "'");
    } else if (!pool->init(type, properties)) {
      LOG_TOPIC("6fac5", WARN, arangodb::iresearch::TOPIC)
          << "failure initializing an arangosearch analyzer instance for name '"
          << name << "' type '" << type << "' properties '" << properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          std::string("failure initializing an arangosearch analyzer instance "
                      "for name '") +
              std::string(name) + "' type '" + std::string(type) +
              "' properties '" + std::string(properties) + "'");
    }

    initialized.emplace(std::piecewise_construct,
                        std::forward_as_tuple(pool->name()),  // emplace with ref at string in pool
                        std::forward_as_tuple(pool));
    pool->setKey(key);

    return true;  // process next
  };

  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex);  // the cache could return the same pool asynchronously
                       // before '_key' updated/rolled back below
  bool revert = true;
  auto cleanup = irs::make_finally([&revert, &initialized]() -> void {
    if (revert) {
      for (auto& entry : initialized) {
        auto& pool = entry.second;

        // reset pool configuration back to uninitialized
        // safe to reset since loadConfiguration(...) is called from start()
        // which is single-thread
        if (pool) {
          pool->_config.clear();                     // noexcept
          pool->_key = irs::string_ref::NIL;         // noexcept
          pool->_type = irs::string_ref::NIL;        // noexcept
          pool->_properties = irs::string_ref::NIL;  // noexcept
        }
      }
    }
  });

  try {
    collection->invokeOnAllElements(&trx, visitor);

    // ensure all records were initialized
    for (auto& entry : _customAnalyzers) {
      if (initialized.find(entry.first) == initialized.end()) {
        LOG_TOPIC("38291", WARN, arangodb::iresearch::TOPIC)
            << "uninitialized AnalyzerPool deletected while validating "
               "analyzers, arangosearch analyzer name '"
            << entry.first << "'";
        return false;  // found an uninitialized analyzer
      }
    }

    if (!trx.commit().ok()) {
      LOG_TOPIC("7a050", WARN, arangodb::iresearch::TOPIC)
          << "failure to commit transaction while loading AnalyzerFeature "
             "configuration";
      return false;
    }

    revert = false;

    return true;
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC("40979", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while loading configuration: " << e.code() << " "
        << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC("36c78", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while loading configuration: " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC("47651", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while loading configuration";
    IR_LOG_EXCEPTION();
  }

  return false;
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
      return std::string(activeVocbase.name()).append(2, ANALYZER_PREFIX_DELIM).append(split.second);
    }

    if (split.first.empty()) {
      return std::string(systemVocbase.name()).append(2, ANALYZER_PREFIX_DELIM).append(split.second);
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
      return std::string(2, ANALYZER_PREFIX_DELIM).append(split.second);
    }
  }

  return name; // name prefixed with vocbase (or NIL)
}

void IResearchAnalyzerFeature::prepare() {
  ApplicationFeature::prepare();

  // load all known analyzers
  ::iresearch::analysis::analyzers::init();
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
        "static analyzers cannot be removed" // message
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
        TRI_ERROR_FORBIDDEN, // code
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
        TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, // code
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
    builder.add(arangodb::StaticStrings::KeyString, toValuePair(pool->_key));
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
  ApplicationFeature::start();

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
/*FIXME TODO disable until V8 handler is implemented for JavaScript tests
  registerUpgradeTasks(); // register tasks after UpgradeFeature::prepare() has finished
*/
  // ensure that the configuration collection is present before loading
  // configuration for the case of inRecovery() if there is no collection then
  // obviously no custom analyzer configurations were persisted (so missing
  // analyzer is failure) if there is a configuration collection then just load
  // analizer configurations
  {
    auto vocbase = getSystemDatabase();

    if (!vocbase) {
      LOG_TOPIC("a5692", WARN, arangodb::iresearch::TOPIC)
          << "failure to get system database while starting feature "
             "'IResearchAnalyzer'";
      // assume configuration collection exists
    } else {
      auto collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);

      if (!collection) {
        auto* engine = arangodb::EngineSelectorFeature::ENGINE;

        if (!engine) {
          LOG_TOPIC("f113f", WARN, arangodb::iresearch::TOPIC)
              << "failure to get storage engine while starting feature "
                 "'IResearchAnalyzer'";
          // assume not inRecovery(), create collection immediately
        } else if (engine->inRecovery()) {
          auto* feature =
              arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
                  "Database");

          if (!feature) {
            LOG_TOPIC("13812", WARN, arangodb::iresearch::TOPIC)
                << "failure to find feature 'Database' while starting feature "
                   "'IResearchAnalyzer'";
            // can't register post-recovery callback, create collection
            // immediately
          } else {
            std::shared_ptr<TRI_vocbase_t> sharedVocbase(std::move(vocbase));

            feature->registerPostRecoveryCallback([this, sharedVocbase]() -> arangodb::Result {
              ensureConfigCollection(*sharedVocbase);  // ensure configuration collection exists

              WriteMutex mutex(_mutex);
              SCOPED_LOCK(mutex);  // '_started' can be asynchronously read

              // ensure all records were initialized
              if (!_customAnalyzers.empty()) {
                return arangodb::Result(TRI_ERROR_INTERNAL,
                                        "uninitialized AnalyzerPool detected "
                                        "while validating analyzers");
              }

              _started = true;

              return arangodb::Result();
            });

            return;  // nothing more to do while inRecovery()
          }
        }

        ensureConfigCollection(*vocbase);  // ensure configuration collection exists

        WriteMutex mutex(_mutex);
        SCOPED_LOCK(mutex);  // '_customAnalyzers' can be asynchronously
                             // modified, '_started' can be asynchronously read

        // ensure all records were initialized
        if (!_customAnalyzers.empty()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL,
              "uninitialized AnalyzerPool detected while validating analyzers");
        }

        _started = true;

        return;  // no persisted configurations to load since just created
                 // collection
      }
    }

    // load persisted configuration
    if (!loadConfiguration()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "uninitialized AnalyzerPool detected while validating analyzers");
    }
  }

  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex);  // '_started' can be asynchronously read

  _started = true;
}

void IResearchAnalyzerFeature::stop() {
  {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);  // '_analyzers'/_customAnalyzers/'_started' can be
                         // asynchronously read

    _started = false;
    _analyzers = getStaticAnalyzers();  // clear cache and reload static analyzers
    _customAnalyzers.clear();           // clear cache
  }

  ApplicationFeature::stop();
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

  // FIXME TODO remove temporary workaround once emplace(...) and all tests are updated
  if (split.first.null()) {
    vocbase = getSystemDatabase().get();
  }

  if (!vocbase) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("failure to find vocbase while persising arangosearch analyzer '") + pool.name()+ "'"
    );
  }

  try {
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
    builder.add("name", toValuePair(split.second));
    builder.add("type", toValuePair(pool.type()));

    if (pool.properties().null()) {
      builder.add( // add value
        "properties", // name
        arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null) // value
      );
    } else {
      builder.add("properties", toValuePair(pool.properties()));
    }

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

        if (feature->name().null()) {
          builder.add( // add value
            arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null) // value
          );
        } else {
          builder.add(toValuePair(feature->name()));
        }
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

bool IResearchAnalyzerFeature::visit( // visit analyzers
    VisitorType const& visitor, // visitor
    TRI_vocbase_t const* vocbase /*= nullptr*/ // analyzers for vocbase
) {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex);

  for (auto& entry: _analyzers) {
    if (entry.second // have entry
        && (!vocbase || splitAnalyzerName(entry.first).first == vocbase->name()) // requested vocbase
        && !visitor(entry.first, entry.second->_type, entry.second->_properties) // termination request
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