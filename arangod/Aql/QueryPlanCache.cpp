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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "QueryPlanCache.h"

#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "VocBase/vocbase.h"

#include <boost/container_hash/hash.hpp>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>

namespace arangodb::aql {

size_t QueryPlanCache::KeyHasher::operator()(
    QueryPlanCache::Key const& key) const noexcept {
  size_t hash = 0;
  boost::hash_combine(hash, key.queryString.hash());
  boost::hash_combine(hash, key.bindParameters->slice().normalizedHash());
  return hash;
}

bool QueryPlanCache::KeyEqual::operator()(
    QueryPlanCache::Key const& lhs,
    QueryPlanCache::Key const& rhs) const noexcept {
  if (!lhs.queryString.equal(rhs.queryString)) {
    return false;
  }

  if (lhs.bindParameters->slice().normalizedHash() !=
      rhs.bindParameters->slice().normalizedHash()) {
    return false;
  }
  return basics::VelocyPackHelper::compare(
             lhs.bindParameters->slice(), rhs.bindParameters->slice(),
             /*useUtf8*/ true, &VPackOptions::Defaults, nullptr, nullptr) == 0;
}

QueryPlanCache::QueryPlanCache() : _entries(5, KeyHasher{}, KeyEqual{}) {}

QueryPlanCache::~QueryPlanCache() {}

std::shared_ptr<velocypack::Builder> QueryPlanCache::lookup(
    QueryPlanCache::Key const& key) const {
  std::shared_lock guard(_mutex);
  if (auto it = _entries.find(key); it != _entries.end()) {
    return (*it).second.serializedPlan;
  }
  return nullptr;
}

void QueryPlanCache::store(
    Key&& key, std::vector<std::string>&& dataSources,
    std::shared_ptr<velocypack::Builder> serializedPlan) {
  Value value{std::move(dataSources), std::move(serializedPlan)};

  std::unique_lock guard(_mutex);

  _entries.insert_or_assign(std::move(key), std::move(value));
}

QueryPlanCache::Key QueryPlanCache::createCacheKey(
    QueryString const& queryString,
    std::shared_ptr<velocypack::Builder> const& bindVars) const {
  // TODO: avoid full copy of queryString here
  return {queryString, filterBindParameters(bindVars)};
}

void QueryPlanCache::invalidate(std::string_view dataSource) {
  std::unique_lock guard(_mutex);
  for (auto it = _entries.begin(); it != _entries.end(); /* no hoisting */) {
    auto const& value = (*it).second;
    if (std::find(value.dataSources.begin(), value.dataSources.end(),
                  dataSource) != value.dataSources.end()) {
      it = _entries.erase(it);
    } else {
      ++it;
    }
  }
}

void QueryPlanCache::invalidateAll() {
  std::unique_lock guard(_mutex);
  _entries.clear();
}

std::shared_ptr<velocypack::Builder> QueryPlanCache::filterBindParameters(
    std::shared_ptr<velocypack::Builder> const& source) {
  // extract relevant bind vars from passed bind variables.
  // we intentionally ignore all value bind parameters here
  auto result = std::make_shared<velocypack::Builder>();
  result->openObject();
  if (source != nullptr) {
    for (auto it : VPackObjectIterator(source->slice())) {
      if (it.key.stringView().starts_with('@')) {
        // collection name bind parameter
        result->add(it.key.stringView(), it.value);
        // TODO: also handle attribute name bind parameters here
      }
    }
  }
  result->close();

  return result;
}

}  // namespace arangodb::aql
