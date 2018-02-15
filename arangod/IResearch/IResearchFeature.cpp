
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

#include "search/scorers.hpp"

#include "IResearchFeature.h"
#include "IResearchRocksDBRecoveryHelper.h"
#include "IResearchView.h"
#include "ApplicationServerHelper.h"

#include "RestServer/ViewTypesFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalView.h"

#include "Aql/AqlValue.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Function.h"

#include "Logger/Logger.h"
#include "Logger/LogMacros.h"

#include "Basics/SmallVector.h"

#include "utils/log.hpp"

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
    true,          // can pass arguments by reference
    &noop          // function implementation (use function name as placeholder)
  });

  arangodb::iresearch::addFunction(functions, {
    "STARTS_WITH", // name
    ".,.|.",       // positional arguments (attribute, prefix, scoring-limit)
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    true,          // can pass arguments by reference
    &noop          // function implementation (use function name as placeholder)
  });

  arangodb::iresearch::addFunction(functions, {
    "PHRASE",      // name
    ".,.,.|.+",    // positional arguments (attribute, input [, offset, input... ], analyzer)
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    true,          // can pass arguments by reference
    &noop          // function implementation (use function name as placeholder)
  });
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
      true,   // can pass arguments by reference
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

arangodb::Result transactionStateRegistrationCallback(
    TRI_voc_cid_t cid,
    arangodb::TransactionState& state
) {
  auto* vocbase = state.vocbase();

  if (!vocbase) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "failure to find vocbase while processing a TransactionState by IResearchFeature for tid '" << state.id() << "' cid '" << cid << "'";

    return arangodb::Result(TRI_ERROR_INTERNAL);
  }

  auto view = vocbase->lookupView(cid);

  if (!view || arangodb::iresearch::IResearchView::type() != view->type()) {
    return arangodb::Result(); // not an IResearchView (noop)
  }

  // TODO FIXME find a better way to look up an IResearch View
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view->getImplementation());
  #else
    auto* impl = static_cast<arangodb::iresearch::IResearchView*>(view->getImplementation());
  #endif

  if (!impl) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "failure to get IResearchView while processing a TransactionState by IResearchFeature for tid '" << state.id() << "' cid '" << cid << "'";

    return arangodb::Result(TRI_ERROR_INTERNAL);
  }

  impl->apply(state);

  return arangodb::Result();
}

std::string const FEATURE_NAME("ArangoSearch");
IResearchLogTopic LIBIRESEARCH("libiresearch", arangodb::LogLevel::INFO);

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

/*static*/ arangodb::LogTopic IResearchFeature::IRESEARCH(IResearchFeature::type(), LogLevel::INFO);

IResearchFeature::IResearchFeature(arangodb::application_features::ApplicationServer* server)
  : ApplicationFeature(server, IResearchFeature::name()),
    _running(false) {
  setOptional(true);
  requiresElevatedPrivileges(false);
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

  // register 'arangosearch' view
  ViewTypesFeature::registerViewImplementation(
     IResearchView::type(),
     IResearchView::make
  );

  // register 'arangosearch' TransactionState state-change callback factory
  arangodb::transaction::Methods::addStateRegistrationCallback(
    transactionStateRegistrationCallback
  );

  registerRecoveryHelper();
}

void IResearchFeature::start() {
  ApplicationFeature::start();

  // register IResearchView filters
 {
    auto* functions = getFeature<arangodb::aql::AqlFunctionFeature>("AQLFunctions");

    if (functions) {
      registerFilters(*functions);
      registerScorers(*functions);
    } else {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "failure to find feature 'AQLFunctions' while registering iresearch filters";
    }

  }

  _running = true;
}

void IResearchFeature::stop() {
  _running = false;
  ApplicationFeature::stop();
}

/*static*/ std::string const& IResearchFeature::type() {
  static const std::string value("arangosearch");

  return value;
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