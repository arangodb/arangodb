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

#ifndef ARANGODB_VOCBASE_PATHENUMERATOR_H
#define ARANGODB_VOCBASE_PATHENUMERATOR_H 1

#include <velocypack/Slice.h>
#include <stack>
#include "Basics/Common.h"
#include "Graph/EdgeDocumentToken.h"

namespace arangodb {
namespace aql {
struct AqlValue;
}

namespace velocypack {
class Builder;
}

namespace graph {
class EdgeCursor;
}

namespace traverser {
class Traverser;
struct TraverserOptions;

struct EnumeratedPath {
  std::vector<graph::EdgeDocumentToken> edges;
  std::vector<arangodb::velocypack::StringRef> vertices;
  EnumeratedPath() {}
};

class PathEnumerator {
 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief This is the class that knows the details on how to
  ///        load the data and how to return data in the expected format
  ///        NOTE: This class does not known the traverser.
  //////////////////////////////////////////////////////////////////////////////

  traverser::Traverser* _traverser;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Indicates if we issue next() the first time.
  ///        It shall return an empty path in this case.
  //////////////////////////////////////////////////////////////////////////////

  bool _isFirst;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Options used in the traversal
  //////////////////////////////////////////////////////////////////////////////

  TraverserOptions* _opts;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief List of the last path is used to
  //////////////////////////////////////////////////////////////////////////////

  EnumeratedPath _enumeratedPath;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Number of HTTP requests made
  //////////////////////////////////////////////////////////////////////////////

  size_t _httpRequests;
  
 public:
  PathEnumerator(Traverser* traverser, std::string const& startVertex,
                 TraverserOptions* opts);

  virtual ~PathEnumerator() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Compute the next Path element from the traversal.
  ///        Returns false if there is no next path element.
  ///        Only if this is true one can compute the AQL values.
  //////////////////////////////////////////////////////////////////////////////

  virtual bool next() = 0;

  virtual aql::AqlValue lastVertexToAqlValue() = 0;
  virtual aql::AqlValue lastEdgeToAqlValue() = 0;
  virtual aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder&) = 0;
  
  /// @brief return number of HTTP requests made, and reset it to 0
  size_t getAndResetHttpRequests() { 
    size_t value = _httpRequests;
    _httpRequests = 0;
    return value;
  }
  
  void incHttpRequests(size_t requests) { _httpRequests += requests; }
};

class DepthFirstEnumerator final : public PathEnumerator {
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief The stack of EdgeCursors to walk through.
  //////////////////////////////////////////////////////////////////////////////

  std::stack<std::unique_ptr<graph::EdgeCursor>> _edgeCursors;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Flag if we need to prune the next path
  //////////////////////////////////////////////////////////////////////////////
  bool _pruneNext;

 public:
  DepthFirstEnumerator(Traverser* traverser, std::string const& startVertex,
                       TraverserOptions* opts);

  ~DepthFirstEnumerator();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next Path element from the traversal.
  //////////////////////////////////////////////////////////////////////////////

  bool next() override;

  aql::AqlValue lastVertexToAqlValue() override;

  aql::AqlValue lastEdgeToAqlValue() override;

  aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder& result) override;

 private:
  bool shouldPrune();

  velocypack::Slice pathToSlice(arangodb::velocypack::Builder& result);
};
}  // namespace traverser
}  // namespace arangodb

#endif
