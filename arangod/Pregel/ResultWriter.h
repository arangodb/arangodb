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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGO_SSSP_H
#define ARANGODB_PREGEL_ALGO_SSSP_H 1
#include "Algorithm.h"


struct TRI_vocbase_t;
namespace arangodb {
namespace pregel {

template<typename V, typename E> class GraphStore;
    
template<typename V, typename E>
class ResultWriter {
 std::string _resultVertexCollection;
 std::string _resultEdgeCollection;
 bool _writeInSameCollections = true;
 bool resultField;
    
 public:
  ResultWriter(VPackSlice params) {}
  void writeResults(TRI_vocbase_t *vocbase, GraphStore<V,E> *store);
};
}
}
#endif
