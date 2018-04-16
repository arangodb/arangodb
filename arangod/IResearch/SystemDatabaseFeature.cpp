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

#include "ApplicationServerHelper.h"
#include "IResearchCommon.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/vocbase.h"

#include "SystemDatabaseFeature.h"

namespace {
  static std::string const FEATURE_NAME("SystemDatabase");
}

namespace arangodb {
namespace iresearch {

void SystemDatabaseFeature::VocbaseReleaser::operator()(TRI_vocbase_t* ptr) {
  if (ptr) {
    ptr->release();
  }
}

SystemDatabaseFeature::SystemDatabaseFeature(
    application_features::ApplicationServer* server,
    TRI_vocbase_t* vocbase /*= nullptr*/
): ApplicationFeature(server, SystemDatabaseFeature::name()),
   _vocbase(vocbase) {
  startsAfter("Database"); // used for getting the system database
}

/*static*/ std::string const& SystemDatabaseFeature::name() noexcept {
  return FEATURE_NAME;
}

void SystemDatabaseFeature::start() {
  auto* databases = getFeature<arangodb::DatabaseFeature>("Database");

  if (databases) {
    _vocbase.store(databases->systemDatabase());

    return;
  }

  LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
    << "failure to find feature 'Database' while starting SystemDatabaseFeature";
  FATAL_ERROR_EXIT();
}

void SystemDatabaseFeature::stop() {
  _vocbase.store(nullptr);
}

SystemDatabaseFeature::ptr SystemDatabaseFeature::use() const {
  auto* vocbase = _vocbase.load();

  return ptr(vocbase && vocbase->use() ? vocbase : nullptr);
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------