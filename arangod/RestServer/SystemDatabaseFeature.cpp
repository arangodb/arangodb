////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "SystemDatabaseFeature.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/vocbase.h"

namespace {
static std::string const FEATURE_NAME("SystemDatabase");
}

namespace arangodb {

void SystemDatabaseFeature::VocbaseReleaser::operator()(TRI_vocbase_t* ptr) {
  if (ptr) {
    ptr->release();
  }
}

SystemDatabaseFeature::SystemDatabaseFeature(application_features::ApplicationServer& server,
    std::shared_ptr<TRI_vocbase_t> const& vocbase /*= nullptr*/
                                             )
    : ApplicationFeature(server, SystemDatabaseFeature::name()), _vocbase(vocbase) {
  startsAfter("Database");
}

/*static*/ std::string const& SystemDatabaseFeature::name() noexcept {
  return FEATURE_NAME;
}

void SystemDatabaseFeature::start() {
  auto* feature =
      application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
          "Database");

  if (feature) {
    auto vocbase = std::shared_ptr<TRI_vocbase_t>( // system vocbase
      feature->lookupDatabase(TRI_VOC_SYSTEM_DATABASE) // args
    );

    std::atomic_store(&_vocbase, vocbase);

    return;
  }

  LOG_TOPIC(WARN, arangodb::Logger::FIXME)
      << "failure to find feature 'Database' while starting feature '"
      << FEATURE_NAME << "'";
  FATAL_ERROR_EXIT();
}

void SystemDatabaseFeature::unprepare() { _vocbase.reset(); }

SystemDatabaseFeature::ptr SystemDatabaseFeature::use() const {
  auto vocbase = std::atomic_load(&_vocbase);

  return ptr(vocbase && vocbase->use() ? vocbase.get() : nullptr);
}

}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------