////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#include "Pregel3/Utils.h"

using namespace arangodb;
namespace arangodb::pregel3 {

std::string const Utils::vertexCollNames = "vertexCollNames";
std::string const Utils::edgeCollNames = "edgeCollNames";
std::string const Utils::wrongGraphSpecErrMsg =
    "The graph specification is "
    "either the graph name or an object containing a list of vertex collection "
    "names and a list of edge collection names.";
std::string const wrongRequestBody = "Could not parse the body of the request.";

std::string const queryId = "queryId";
std::string const graphName = "graphName";
std::string const graphSpec = "graphSpec";
std::string const vertexCollectionNames = "vertexCollectionNames";
std::string const edgeCollectionNames = "edgeCollectionNames";

}  // namespace arangodb::pregel3