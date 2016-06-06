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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ActionFeature.h"

#include "Actions/actions.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-actions.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

ActionFeature* ActionFeature::ACTION = nullptr;

ActionFeature::ActionFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Action"),
      _allowUseDatabase(false) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void ActionFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addHiddenOption(
      "--server.allow-use-database",
      "allow change of database in REST actions, only needed for "
      "unittests",
      new BooleanParameter(&_allowUseDatabase));
}

void ActionFeature::start() {
  ACTION = this;

  V8DealerFeature* dealer = 
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");

  dealer->defineContextUpdate(
      [](v8::Isolate* isolate, v8::Handle<v8::Context> context, size_t) {
        TRI_InitV8Actions(isolate, context);
      },
      nullptr);
}

void ActionFeature::unprepare() {
  TRI_CleanupActions();

  ACTION = nullptr;
}
