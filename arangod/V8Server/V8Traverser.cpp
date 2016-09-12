////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "V8Traverser.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/SingleServerTraverser.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::traverser;

using VPackStringHash = arangodb::basics::VelocyPackHelper::VPackStringHash;
using VPackStringEqual = arangodb::basics::VelocyPackHelper::VPackStringEqual;

ShortestPathOptions::ShortestPathOptions(arangodb::Transaction* trx)
    : BasicOptions(trx),
      direction("outbound"),
      useWeight(false),
      weightAttribute(""),
      defaultWeight(1),
      bidirectional(true),
      multiThreaded(true) {
}

void ShortestPathOptions::setStart(std::string const& id) {
  start = id;
  startBuilder.clear();
  startBuilder.add(VPackValue(id));
}

void ShortestPathOptions::setEnd(std::string const& id) {
  end = id;
  endBuilder.clear();
  endBuilder.add(VPackValue(id));
}

VPackSlice ShortestPathOptions::getStart() const {
  return startBuilder.slice();
}

VPackSlice ShortestPathOptions::getEnd() const {
  return endBuilder.slice();
}
