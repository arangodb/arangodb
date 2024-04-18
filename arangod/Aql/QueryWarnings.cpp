////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "QueryWarnings.h"

#include "Aql/ExecutionNodeId.h"
#include "Aql/QueryOptions.h"
#include "Basics/debugging.h"
#include "Basics/Exceptions.h"

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::aql;

QueryWarnings::QueryWarnings()
    : _maxWarningCount(std::numeric_limits<size_t>::max()),
      _failOnWarning(false) {}

// register an error
// this also makes the query abort
void QueryWarnings::registerError(ErrorCode code, std::string_view details) {
  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  if (details.empty()) {
    THROW_ARANGO_EXCEPTION(code);
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(code, details);
}

// register a warning
void QueryWarnings::registerWarning(ErrorCode code, std::string_view details) {
  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  std::lock_guard<std::mutex> guard(_mutex);

  if (_failOnWarning) {
    // make an error from each warning if requested
    THROW_ARANGO_EXCEPTION_MESSAGE(code, details);
  }

  if (_list.size() >= _maxWarningCount) {
    return;
  }

  if (details.empty()) {
    _list.emplace_back(code, TRI_errno_string(code));
  } else {
    _list.emplace_back(code, details);
  }
}

void QueryWarnings::toVelocyPack(arangodb::velocypack::Builder& b) const {
  TRI_ASSERT(b.isOpenObject());

  b.add(VPackValue("warnings"));
  VPackArrayBuilder guard(&b);

  std::lock_guard<std::mutex> lock(_mutex);

  for (auto const& pair : _list) {
    VPackObjectBuilder objGuard(&b);
    b.add("code", VPackValue(pair.first));
    b.add("message", VPackValue(pair.second));
  }
}

bool QueryWarnings::empty() const {
  std::lock_guard<std::mutex> guard(_mutex);
  return _list.empty();
}

size_t QueryWarnings::count() const {
  std::lock_guard<std::mutex> guard(_mutex);
  return _list.size();
}

void QueryWarnings::updateFromOptions(QueryOptions const& opts) {
  std::lock_guard<std::mutex> guard(_mutex);
  _maxWarningCount = opts.maxWarningCount;
  _failOnWarning = opts.failOnWarning;
}

std::vector<std::pair<ErrorCode, std::string>> QueryWarnings::all() const {
  std::lock_guard<std::mutex> guard(_mutex);
  return _list;
}
