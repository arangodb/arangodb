////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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

#include "KPathFinder.h"
#include "Graph/ShortestPathOptions.h"

using namespace arangodb;
using namespace arangodb::graph;


bool KPathFinder::VertexIdentifier::operator<(VertexIdentifier const& other) const {
  // We only compare on the id vaue.
  // predecessor does not matter
  return id < other.id;
}

KPathFinder::Ball::Ball() {}
KPathFinder::Ball::~Ball() = default;

void KPathFinder::Ball::reset(VertexRef center) {
  _center = center;
  _shell.clear();
  _interior.clear();
  _shell.emplace(VertexIdentifier{center, 0});
  _depth = 0;
  _searchIndex = std::numeric_limits<size_t>::max();
}

void KPathFinder::Ball::startNextDepth() {
  // Move everything from Shell to interior
  // Now Shell will contain the new vertices
  _searchIndex = _interior.size();
  _interior.insert(_interior.end(), std::make_move_iterator(_shell.begin()),
                   std::make_move_iterator(_shell.end()));
  _shell.clear();
  _depth++;
}

auto KPathFinder::Ball::noPathLeft() const -> bool {
  // TODO: Not yet complete
  return _searchIndex >= _interior.size() && _shell.empty();
}

auto KPathFinder::Ball::getDepth() const -> size_t {
  return _depth;
}

auto KPathFinder::Ball::shellSize() const -> size_t {
  return _shell.size();
}

KPathFinder::KPathFinder(ShortestPathOptions& options) : _opts(options) {}
KPathFinder::~KPathFinder() = default;

void KPathFinder::reset(VertexRef source, VertexRef target) {
  _results.clear();
  _left.reset(source);
  _right.reset(target);

  // Special Case depth == 0
  if (_opts.minDepth == 0 && source == target) {
    _results.emplace_back(std::make_pair(VertexIdentifier{source, 0}, VertexIdentifier{target, 0}));
  }
}

auto KPathFinder::hasMore() const -> bool {
  return !_results.empty() || !searchDone();
}

// get the next available path serialized in the builder
auto KPathFinder::getNextPath(arangodb::velocypack::Builder& result) -> bool{
  return false;
}

auto KPathFinder::searchDone() const -> bool {
  return _left.noPathLeft() || _right.noPathLeft() || _left.getDepth() + _right.getDepth() < _opts.maxDepth;
}
