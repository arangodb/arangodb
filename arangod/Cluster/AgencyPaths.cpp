////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "AgencyPaths.h"

using namespace arangodb;
using namespace arangodb::cluster;
using namespace arangodb::cluster::paths;

std::shared_ptr<Root::Arango const> arangodb::cluster::paths::aliases::arango() {
  return root()->arango();
}

std::shared_ptr<Root::Arango::Plan const> arangodb::cluster::paths::aliases::plan() {
  return root()->arango()->plan();
}

std::shared_ptr<Root::Arango::Current const> arangodb::cluster::paths::aliases::current() {
  return root()->arango()->current();
}

std::shared_ptr<Root::Arango::Target const> arangodb::cluster::paths::aliases::target() {
  return root()->arango()->target();
}

std::shared_ptr<Root::Arango::Supervision const> arangodb::cluster::paths::aliases::supervision() {
  return root()->arango()->supervision();
}
