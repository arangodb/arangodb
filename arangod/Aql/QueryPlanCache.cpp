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

#include "Aql/QueryOptions.h"
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/system-functions.h"
#include "Logger/Logger.h"
#include "VocBase/vocbase.h"

#include <boost/container_hash/hash.hpp>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>

namespace arangodb::aql {

size_t QueryPlanCache::Key::hash() const {
  return QueryPlanCache::KeyHasher{}(*this);
}

size_t QueryPlanCache::KeyHasher::operator()(
    QueryPlanCache::Key const& key) const noexcept {
  size_t hash = 0;
  boost::hash_combine(hash, key.queryString.hash());
  boost::hash_combine(hash, key.bindParameters->slice().normalizedHash());
  // arbitrary integer values used here from https://hexwords.netlify.app/
  // for fullcount=true / fullcount=false
  boost::hash_combine(hash, key.fullCount ? 0xB16F007 : 0xB0BCA7);
  return hash;
}

bool QueryPlanCache::KeyEqual::operator()(
    QueryPlanCache::Key const& lhs,
    QueryPlanCache::Key const& rhs) const noexcept {
  if (lhs.fullCount != rhs.fullCount) {
    return false;
  }

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

std::shared_ptr<velocypack::UInt8Buffer> QueryPlanCache::lookup(
    QueryPlanCache::Key const& key) const {
  std::shared_lock guard(_mutex);
  if (auto it = _entries.find(key); it != _entries.end()) {
    return (*it).second.serializedPlan;
  }
  return nullptr;
}

void QueryPlanCache::store(
    Key&& key, std::unordered_map<std::string, std::string>&& dataSourceGuids,
    std::shared_ptr<velocypack::UInt8Buffer> serializedPlan) {
  Value value{std::move(dataSourceGuids), std::move(serializedPlan),
              TRI_microtime()};

  std::unique_lock guard(_mutex);

  _entries.insert_or_assign(std::move(key), std::move(value));
}

QueryPlanCache::Key QueryPlanCache::createCacheKey(
    QueryString const& queryString,
    std::shared_ptr<velocypack::Builder> const& bindVars,
    QueryOptions const& queryOptions) const {
  // TODO: avoid full copy of queryString here
  return {queryString, filterBindParameters(bindVars), queryOptions.fullCount};
}

void QueryPlanCache::invalidate(std::string const& dataSourceGuid) {
  std::unique_lock guard(_mutex);
  for (auto it = _entries.begin(); it != _entries.end(); /* no hoisting */) {
    auto const& value = (*it).second;
    if (value.dataSourceGuids.contains(dataSourceGuid)) {
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

void QueryPlanCache::toVelocyPack(velocypack::Builder& builder) const {
  std::unique_lock guard(_mutex);

  builder.openArray(true);
  for (auto const& it : _entries) {
    auto const& key = it.first;
    auto const& value = it.second;

    builder.openObject();
    builder.add("hash", VPackValue(key.hash()));
    builder.add("query", VPackValue(key.queryString.string()));
    builder.add("queryHash", VPackValue(key.queryString.hash()));
    if (key.bindParameters == nullptr) {
      builder.add("bindVars", VPackSlice::emptyObjectSlice());
    } else {
      builder.add("bindVars", key.bindParameters->slice());
    }
    builder.add("fullCount", VPackValue(key.fullCount));

    builder.add("dataSources", VPackValue(VPackValueType::Array));
    for (auto const& ds : value.dataSourceGuids) {
      builder.add(VPackValue(ds.second));
    }
    builder.close();  // dataSources

    builder.add("created", VPackValue(TRI_StringTimeStamp(
                               value.dateCreated, Logger::getUseLocalTime())));

    builder.close();
  }
  builder.close();
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
      }
    }
  }
  result->close();

  return result;
}

}  // namespace arangodb::aql
