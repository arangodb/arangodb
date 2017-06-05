
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

#include "ViewFeature.h"
#include "RestViewHandler.h"

#include "Basics/ArangoGlobalContext.h"
#include "Basics/StringRef.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "RestHandler/RestHandlerCreator.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

#include <unordered_map>
#include <functional>

using namespace arangodb;
using namespace arangodb::options;

namespace {

std::unordered_map<arangodb::StringRef, ViewFeature::ViewFactory> VIEW_FACTORIES;

RestViewHandler::ViewFactory localViewFactory = [] (
    StringRef const& type,
    VPackSlice params,
    TRI_vocbase_t* vocbase) -> bool {
  const auto it = VIEW_FACTORIES.find(type);

  return it == VIEW_FACTORIES.end()
    ? false
    : it->second(params, vocbase);
};

RestViewHandler::ViewFactory coordinatorViewFactory = [] (
    StringRef const& type,
    VPackSlice params,
    TRI_vocbase_t* vocbase) -> bool {
  return false;
};

inline RestViewHandler::ViewFactory* viewFactory() {
  return  ServerState::instance()->isCoordinator()
    ? &coordinatorViewFactory
    : &localViewFactory;
}

}

/*static*/ void ViewFeature::registerFactory(
    StringRef const& type,
    ViewFactory const& factory) {
  VIEW_FACTORIES.emplace(type, factory);
}

ViewFeature::ViewFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "View") {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
  // ensure that GeneralServerFeature::HANDLER_FACTORY is already initialized
  startsAfter("GeneralServer");
}

void ViewFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
}

void ViewFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
}

void ViewFeature::prepare() {
}

void ViewFeature::start() {
  GeneralServerFeature::HANDLER_FACTORY->addPrefixHandler(
    RestViewHandler::VIEW_PATH,
    RestHandlerCreator<RestViewHandler>::createData<const RestViewHandler::ViewFactory*>,
    viewFactory()
  );
}
