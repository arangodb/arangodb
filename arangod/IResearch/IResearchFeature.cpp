
//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasily Nabatchikov
////////////////////////////////////////////////////////////////////////////////

// otherwise define conflict between 3rdParty\date\include\date\date.h and 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
  #include "date/date.h"
  #undef NOEXCEPT
#endif

#include "search/scorers.hpp"
#include "utils/log.hpp"

#include "ApplicationServerHelper.h"
#include "IResearchCommon.h"
#include "IResearchFeature.h"
#include "IResearchMMFilesLink.h"
#include "IResearchRocksDBLink.h"
#include "IResearchLinkCoordinator.h"
#include "IResearchLinkHelper.h"
#include "IResearchRocksDBRecoveryHelper.h"
#include "IResearchView.h"
#include "IResearchViewCoordinator.h"
#include "IResearchViewDBServer.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Function.h"
#include "Basics/SmallVector.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "RestServer/ViewTypesFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "MMFiles/MMFilesEngine.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalView.h"

NS_BEGIN(arangodb)

NS_BEGIN(basics)
class VPackStringBufferAdapter;
NS_END // basics

NS_BEGIN(aql)
class Query;
NS_END // aql

NS_END // arangodb

NS_LOCAL

class IResearchLogTopic final : public arangodb::LogTopic {
 public:
  IResearchLogTopic(std::string const& name, arangodb::LogLevel level)
    : arangodb::LogTopic(name, level) {
    setIResearchLogLevel(level);
  }

  virtual void setLogLevel(arangodb::LogLevel level) override {
    arangodb::LogTopic::setLogLevel(level);
    setIResearchLogLevel(level);
  }

 private:
  static void setIResearchLogLevel(arangodb::LogLevel level) {
    auto irsLevel = static_cast<irs::logger::level_t>(level);
    irsLevel = std::max(irsLevel, irs::logger::IRL_FATAL);
    irsLevel = std::min(irsLevel, irs::logger::IRL_TRACE);

    irs::logger::enabled(irsLevel);
  }
}; // IResearchLogTopic

arangodb::aql::AqlValue noop(
    arangodb::aql::Query*,
    arangodb::transaction::Methods* ,
    arangodb::SmallVector<arangodb::aql::AqlValue> const&) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
    TRI_ERROR_NOT_IMPLEMENTED,
    "Function is designed to be used with IResearchView only"
  );
}

void registerFilters(arangodb::aql::AqlFunctionFeature& functions) {
  arangodb::iresearch::addFunction(functions, {
    "EXISTS",      // name
    ".|.,.",       // positional arguments (attribute, [ "analyzer"|"type", analyzer-name|"string"|"numeric"|"bool"|"null" ])
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    &noop          // function implementation (use function name as placeholder)
  });

  arangodb::iresearch::addFunction(functions, {
    "STARTS_WITH", // name
    ".,.|.",       // positional arguments (attribute, prefix, scoring-limit)
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    &noop          // function implementation (use function name as placeholder)
  });

  arangodb::iresearch::addFunction(functions, {
    "PHRASE",      // name
    ".,.,.|.+",    // positional arguments (attribute, input [, offset, input... ], analyzer)
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    &noop          // function implementation (use function name as placeholder)
  });
}

void registerIndexFactory() {
  static const std::map<std::string, arangodb::IndexFactory::IndexTypeFactory> factories = {
    { "ClusterEngine", arangodb::iresearch::IResearchLinkCoordinator::make },
    { "MMFilesEngine", arangodb::iresearch::IResearchMMFilesLink::make },
    { "RocksDBEngine", arangodb::iresearch::IResearchRocksDBLink::make }
  };
  auto const& indexType = arangodb::iresearch::DATA_SOURCE_TYPE.name();

  // register 'arangosearch' link
  for (auto& entry: factories) {
    auto* engine = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::StorageEngine
    >(entry.first);

    // valid situation if not running with the specified storage engine
    if (!engine) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to find feature '" << entry.first << "' while registering index type '" << indexType << "', skipping";
      continue;
    }

    // ok to const-cast since this should only be called on startup
    auto& indexFactory =
      const_cast<arangodb::IndexFactory&>(engine->indexFactory());
    auto res = indexFactory.emplaceFactory(indexType, entry.second);

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(),
        std::string("failure registering IResearch link factory with index factory from feature '") + entry.first + "': " + res.errorMessage()
      );
    }

    res = indexFactory.emplaceNormalizer(
      indexType, arangodb::iresearch::IResearchLinkHelper::normalize
    );

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(),
        std::string("failure registering IResearch link normalizer with index factory from feature '") + entry.first + "': " + res.errorMessage()
      );
    }
  }
}

void registerScorers(arangodb::aql::AqlFunctionFeature& functions) {
  irs::scorers::visit([&functions](
     irs::string_ref const& name, irs::text_format::type_id const& args_format
  )->bool {
    // ArangoDB, for API consistency, only supports scorers configurable via jSON
    if (irs::text_format::json != args_format) {
      return true;
    }

    std::string upperName = name;

    // AQL function external names are always in upper case
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);

    arangodb::iresearch::addFunction(functions, {
      std::move(upperName),
      ".|+", // positional arguments (attribute [, <scorer-specific properties>...])
      true,   // deterministic
      false,  // can't throw
      true,   // can be run on server
      &noop   // function implementation (use function name as placeholder)
    });

    return true;
  });
}

void registerRecoveryHelper() {
  auto helper = std::make_shared<arangodb::iresearch::IResearchRocksDBRecoveryHelper>();
  auto res = arangodb::RocksDBEngine::registerRecoveryHelper(helper);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), "failed to register RocksDB recovery helper");
  }
}

void registerViewFactory() {
  auto& viewType = arangodb::iresearch::DATA_SOURCE_TYPE;
  auto* viewTypes = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::ViewTypesFeature
  >();

  if (!viewTypes) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      std::string("failed to find feature '") + arangodb::ViewTypesFeature::name() + "' while registering view type '" + viewType.name() + "'"
    );
  }

  arangodb::Result res;

  // DB server in custer or single-server
  if (arangodb::ServerState::instance()->isCoordinator()) {
    res = viewTypes->emplace(viewType, arangodb::iresearch::IResearchViewCoordinator::make);
  } else if (arangodb::ServerState::instance()->isDBServer()) {
    res = viewTypes->emplace(viewType, arangodb::iresearch::IResearchViewDBServer::make);
  } else {
    res = viewTypes->emplace(viewType, arangodb::iresearch::IResearchView::make);
  }

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      res.errorNumber(),
      std::string("failure registering IResearch view factory: ") + res.errorMessage()
    );
  }
}

template<typename Impl>
arangodb::Result transactionStateRegistrationCallback(
    arangodb::LogicalDataSource& dataSource,
    arangodb::TransactionState& state
) {
  if (arangodb::iresearch::DATA_SOURCE_TYPE != dataSource.type()) {
    return arangodb::Result(); // not an IResearchView (noop)
  }

  // TODO FIXME find a better way to look up a LogicalView
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* view = dynamic_cast<arangodb::LogicalView*>(&dataSource);
  #else
    auto* view = static_cast<arangodb::LogicalView*>(&dataSource);
  #endif

  if (!view) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to get LogicalView while processing a TransactionState by IResearchFeature for tid '" << state.id() << "' name '" << dataSource.name() << "'";

    return arangodb::Result(TRI_ERROR_INTERNAL);
  }

  // TODO FIXME find a better way to look up an IResearch View
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* impl = dynamic_cast<Impl*>(view);
  #else
    auto* impl = static_cast<Impl*>(view);
  #endif

  if (!impl) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to get IResearchView while processing a TransactionState by IResearchFeature for tid '" << state.id() << "' cid '" << dataSource.name() << "'";

    return arangodb::Result(TRI_ERROR_INTERNAL);
  }

  impl->apply(state);

  return arangodb::Result();
}

void registerTransactionStateCallback() {
  if (arangodb::ServerState::instance()->isCoordinator()) {
    // NOOP
  } else if(arangodb::ServerState::instance()->isDBServer()) {
    arangodb::transaction::Methods::addStateRegistrationCallback(
      transactionStateRegistrationCallback<arangodb::iresearch::IResearchViewDBServer>
    );
  } else {
    arangodb::transaction::Methods::addStateRegistrationCallback(
      transactionStateRegistrationCallback<arangodb::iresearch::IResearchView>
    );
  }
}

std::string const FEATURE_NAME("ArangoSearch");
IResearchLogTopic LIBIRESEARCH("libiresearch", arangodb::LogLevel::INFO);

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchFeature::IResearchFeature(arangodb::application_features::ApplicationServer* server)
  : ApplicationFeature(server, IResearchFeature::name()),
    _running(false) {
  setOptional(true);
  startsAfter("ViewTypes");
  startsAfter("Logger");
  startsAfter("Database");
  startsAfter("IResearchAnalyzer"); // used for retrieving IResearch analyzers for functions
  startsAfter("AQLFunctions");
  // TODO FIXME: we need the MMFilesLogfileManager to be available here if we
  // use the MMFiles engine. But it does not feel right to have such storage engine-
  // specific dependency here. Better create a "StorageEngineFeature" and make
  // ourselves start after it!
  startsAfter("MMFilesLogfileManager");
  startsAfter("TransactionManager");

  startsBefore("GeneralServer");
}

void IResearchFeature::beginShutdown() {
  _running = false;
  ApplicationFeature::beginShutdown();
}

void IResearchFeature::collectOptions(
    std::shared_ptr<arangodb::options::ProgramOptions> options
) {
  _running = false;
  ApplicationFeature::collectOptions(options);
}

/*static*/ std::string const& IResearchFeature::name() {
  return FEATURE_NAME;
}

void IResearchFeature::prepare() {
  _running = false;
  ApplicationFeature::prepare();

  // load all known codecs
  ::iresearch::formats::init();

  // load all known scorers
  ::iresearch::scorers::init();

  // register 'arangosearch' index
  registerIndexFactory();

  // register 'arangosearch' view
  registerViewFactory();

  // register 'arangosearch' TransactionState state-change callback factory
  registerTransactionStateCallback();

  registerRecoveryHelper();
}

void IResearchFeature::start() {
  ApplicationFeature::start();

  // register IResearchView filters
  {
    auto* functions = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::aql::AqlFunctionFeature
    >("AQLFunctions");

    if (functions) {
      registerFilters(*functions);
      registerScorers(*functions);
    } else {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to find feature 'AQLFunctions' while registering iresearch filters";
    }

  }

  _running = true;
}

void IResearchFeature::stop() {
  _running = false;
  ApplicationFeature::stop();
}

void IResearchFeature::unprepare() {
  _running = false;
  ApplicationFeature::unprepare();
}

void IResearchFeature::validateOptions(
    std::shared_ptr<arangodb::options::ProgramOptions> options
) {
  _running = false;
  ApplicationFeature::validateOptions(options);
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------