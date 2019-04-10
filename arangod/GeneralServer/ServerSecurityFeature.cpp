////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "V8/v8-globals.h"

#include <v8.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

ServerSecurityFeature::ServerSecurityFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "ServerSecurity"),
      _enableFoxxApi(true),
      _enableFoxxStore(true),
      _hardenedRestApi(false) {
  setOptional(false);
  startsAfter("ServerPlatform");
}

void ServerSecurityFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");
  options->addOption("--server.harden",
                     "users will not be able to receive version information or change "
                     "the log level via the REST API. Admin users and servers without "
                     "authentication will be unaffected.",
                     new BooleanParameter(&_hardenedRestApi))
                     .setIntroducedIn(30500);

  options->addSection("foxx", "Configure Foxx");
  options->addOption("--foxx.api", "enables / disables Foxx management REST APIs",
                     new BooleanParameter(&_enableFoxxApi))
                     .setIntroducedIn(30500);
  options->addOption("--foxx.store", "enables / disables Foxx store in web interface",
                     new BooleanParameter(&_enableFoxxApi))
                     .setIntroducedIn(30500);

}

void ServerSecurityFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
}

void ServerSecurityFeature::start() {
}

bool ServerSecurityFeature::isFoxxApiDisabled(v8::Isolate* isolate) const {
  return !_enableFoxxApi;
}

bool ServerSecurityFeature::isFoxxStoreDisabled(v8::Isolate* isolate) const {
  return !_enableFoxxStore;
}

bool ServerSecurityFeature::isRestApiHardenend(v8::Isolate* isolate) const {
  return _hardenedRestApi;
}

bool ServerSecurityFeature::isInternalContext(v8::Isolate* isolate) const {
  TRI_GET_GLOBALS();
  return v8g != nullptr && v8g->_securityContext.isInternal();
}
