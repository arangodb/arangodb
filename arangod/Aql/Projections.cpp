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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Projections.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <algorithm>

namespace {

/// @brief velocypack attribute name for serializing/unserializing projections
constexpr std::string_view projectionsKey("projections");

}  // namespace

namespace arangodb {
namespace aql {

Projections::Projections() {}

Projections::Projections(std::vector<arangodb::aql::AttributeNamePath> paths) {
  _projections.reserve(paths.size());
  for (auto& path : paths) {
    if (path.empty()) {
      // ignore all completely empty paths
      continue;
    }

    // categorize the projection, based on the attribute name.
    // we do this here only once in order to not do expensive string
    // comparisons at runtime later
    arangodb::aql::AttributeNamePath::Type type = path.type();
    // take over the projection
    _projections.emplace_back(
        Projection{std::move(path), kNoCoveringIndexPosition, 0, type});
  }

  TRI_ASSERT(_projections.size() <= paths.size());

  init();
}

Projections::Projections(
    std::unordered_set<arangodb::aql::AttributeNamePath> const& paths) {
  _projections.reserve(paths.size());
  for (auto& path : paths) {
    if (path.empty()) {
      // ignore all completely empty paths
      continue;
    }

    // categorize the projection, based on the attribute name.
    // we do this here only once in order to not do expensive string
    // comparisons at runtime later
    arangodb::aql::AttributeNamePath::Type type = path.type();
    // take over the projection
    _projections.emplace_back(
        Projection{std::move(path), kNoCoveringIndexPosition, 0, type});
  }

  TRI_ASSERT(_projections.size() <= paths.size());

  init();
}

bool Projections::isCoveringIndexPosition(uint16_t position) noexcept {
  return position != kNoCoveringIndexPosition;
}

/// @brief set the index context for projections using an index
void Projections::setCoveringContext(
    DataSourceId const& id, std::shared_ptr<arangodb::Index> const& index) {
  TRI_ASSERT(_index == nullptr);

  _datasourceId = id;
  _index = index;
}

/// @brief checks if we have a single attribute projection on the attribute
bool Projections::isSingle(std::string const& attribute) const noexcept {
  return _projections.size() == 1 && _projections[0].path[0] == attribute;
}

// return the covering index position for a specific attribute type.
// will throw if the index does not cover!
uint16_t Projections::coveringIndexPosition(
    aql::AttributeNamePath::Type type) const {
  TRI_ASSERT(usesCoveringIndex());

  for (auto const& it : _projections) {
    if (it.type == type) {
      TRI_ASSERT(isCoveringIndexPosition(it.coveringIndexPosition));
      return it.coveringIndexPosition;
    }
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "unable to determine covering index position");
}

void Projections::toVelocyPackFromDocument(
    arangodb::velocypack::Builder& b, arangodb::velocypack::Slice slice,
    transaction::Methods const* trxPtr) const {
  TRI_ASSERT(b.isOpenObject());
  TRI_ASSERT(slice.isObject());

  // the single-attribute projections are easy. we dispatch here based on the
  // attribute type there are a few optimized functions for retrieving _key,
  // _id, _from and _to.
  for (auto const& it : _projections) {
    if (it.type == AttributeNamePath::Type::IdAttribute) {
      // projection for "_id"
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], VPackValue(transaction::helpers::extractIdString(
                            trxPtr->resolver(), slice, slice)));
    } else if (it.type == AttributeNamePath::Type::KeyAttribute) {
      // projection for "_key"
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], transaction::helpers::extractKeyFromDocument(slice));
    } else if (it.type == AttributeNamePath::Type::FromAttribute) {
      // projection for "_from"
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], transaction::helpers::extractFromFromDocument(slice));
    } else if (it.type == AttributeNamePath::Type::ToAttribute) {
      // projection for "_to"
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], transaction::helpers::extractToFromDocument(slice));
    } else if (it.type == AttributeNamePath::Type::SingleAttribute) {
      // projection for any other top-level attribute
      TRI_ASSERT(it.path.size() == 1);
      VPackSlice found = slice.get(it.path.path[0]);
      if (found.isNone()) {
        // attribute not found
        b.add(it.path[0], VPackValue(VPackValueType::Null));
      } else {
        // attribute found
        b.add(it.path[0], found);
      }
    } else {
      // projection for a sub-attribute, e.g. a.b.c
      // this is a lot more complex, because we may need to open and close
      // multiple sub-objects, e.g. a projection on the sub-attribute a.b.c
      // needs to build
      //   { a: { b: { c: valueOfC } } }
      // when we get here it is guaranteed that there will be no projections for
      // sub-attributes with the same prefix, e.g. a.b.c and a.x.y. This would
      // complicate the logic here even further, so it is not supported.
      TRI_ASSERT(it.type == AttributeNamePath::Type::MultiAttribute);
      TRI_ASSERT(it.path.size() > 1);
      size_t level = 0;
      VPackSlice found = slice;
      size_t const n = it.path.size();
      while (level < n) {
        // look up attribute/sub-attribute
        found = found.get(it.path[level]);
        if (found.isNone()) {
          // not found. we can exit early
          b.add(it.path[level], VPackValue(VPackValueType::Null));
          break;
        }
        if (level < n - 1 && !found.isObject()) {
          // we would to recurse into a sub-attribute, but what we found
          // is no object... that means we can already stop here.
          b.add(it.path[level], VPackValue(VPackValueType::Null));
          break;
        }
        if (level == n - 1) {
          // target value
          b.add(it.path[level], found);
          break;
        }
        // recurse into sub-attribute object
        TRI_ASSERT(level < n - 1);
        b.add(it.path[level], VPackValue(VPackValueType::Object));
        ++level;
      }

      // close all that we opened ourselves, so that the next projection can
      // again start at the top level
      while (level-- > 0) {
        b.close();
      }
    }
  }
}

/// @brief projections from a covering index
void Projections::toVelocyPackFromIndex(
    arangodb::velocypack::Builder& b, IndexIteratorCoveringData& covering,
    transaction::Methods const* trxPtr) const {
  TRI_ASSERT(_index != nullptr);
  TRI_ASSERT(b.isOpenObject());

  bool const isArray = covering.isArray();
  for (auto const& it : _projections) {
    if (isArray) {
      // _id cannot be part of a user-defined index
      TRI_ASSERT(it.type != AttributeNamePath::Type::IdAttribute);

      // we will get a Slice with an array of index values. now we need
      // to look up the array values from the correct positions to
      // populate the result with the projection values. this case will
      // be triggered for indexes that can be set up on any number of
      // attributes (persistent/hash/skiplist)
      TRI_ASSERT(isCoveringIndexPosition(it.coveringIndexPosition));
      VPackSlice found = covering.at(it.coveringIndexPosition);
      if (found.isNone()) {
        found = VPackSlice::nullSlice();
      }
      size_t const n =
          std::min(it.path.size(), static_cast<size_t>(it.coveringIndexCutoff));
      TRI_ASSERT(n > 0);
      for (size_t i = 0; i < n - 1; ++i) {
        b.add(it.path[i], VPackValue(VPackValueType::Object));
      }
      b.add(it.path[n - 1], found);
      for (size_t i = 0; i < n - 1; ++i) {
        b.close();
      }
    } else {
      // no array Slice... this case will be triggered for indexes that
      // contain simple string values, such as the primary index or the
      // edge index
      auto slice = covering.value();
      if (it.type == AttributeNamePath::Type::IdAttribute) {
        b.add(it.path[0], VPackValue(transaction::helpers::makeIdFromParts(
                              trxPtr->resolver(), _datasourceId, slice)));
      } else {
        if (slice.isNone()) {
          slice = VPackSlice::nullSlice();
        }
        b.add(it.path[0], slice);
      }
    }
  }
}

void Projections::toVelocyPack(arangodb::velocypack::Builder& b) const {
  toVelocyPack(b, ::projectionsKey);
}

void Projections::toVelocyPack(arangodb::velocypack::Builder& b,
                               std::string_view key) const {
  b.add(key, VPackValue(VPackValueType::Array));
  for (auto const& it : _projections) {
    if (it.path.size() == 1) {
      // projection on a top-level attribute. will be returned as a string
      // for downwards-compatibility
      b.add(VPackValue(it.path[0]));
    } else {
      // projection on a nested attribute (e.g. a.b.c). will be returned as an
      // array. this kind of projection did not exist before 3.7
      b.openArray();
      for (auto const& attribute : it.path.path) {
        b.add(VPackValue(attribute));
      }
      b.close();
    }
  }
  b.close();
}

/*static*/ Projections Projections::fromVelocyPack(
    arangodb::velocypack::Slice slice) {
  return fromVelocyPack(slice, ::projectionsKey);
}

/*static*/ Projections Projections::fromVelocyPack(
    arangodb::velocypack::Slice slice, std::string_view key) {
  std::vector<arangodb::aql::AttributeNamePath> projections;

  VPackSlice p = slice.get(key);
  if (p.isArray()) {
    for (auto const& it : arangodb::velocypack::ArrayIterator(p)) {
      if (it.isString()) {
        projections.emplace_back(
            arangodb::aql::AttributeNamePath(it.copyString()));
      } else if (it.isArray()) {
        arangodb::aql::AttributeNamePath path;
        for (auto const& it2 : arangodb::velocypack::ArrayIterator(it)) {
          path.path.emplace_back(it2.copyString());
        }
        projections.emplace_back(std::move(path));
      }
    }
  }

  return arangodb::aql::Projections(std::move(projections));
}

/// @brief shared init function
void Projections::init() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // validate that no projection contains an empty attribute
  std::for_each(_projections.begin(), _projections.end(),
                [](auto const& it) { TRI_ASSERT(!it.path.empty()); });
#endif

  if (_projections.size() <= 1) {
    return;
  }

  // sort projections by attribute path, so we have similar prefixes next to
  // each other, e.g.
  //   a
  //   a.b
  //   a.b.c
  //   a.c
  //   b
  //   b.a
  //   ...
  std::sort(
      _projections.begin(), _projections.end(),
      [](auto const& lhs, auto const& rhs) { return lhs.path < rhs.path; });

  removeSharedPrefixes();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // make sure all our projections are for distinct top-level attributes.
  // we currently don't support multiple projections for the same top-level
  // attribute, e.g. a.b.c and a.x.y.
  // this would complicate the logic for attribute extraction considerably,
  // so in this case we will just have combined the two projections into a
  // single projection on attribute a before.
  for (auto it = _projections.begin(); it != _projections.end(); ++it) {
    // we expect all our projections to not have any common prefixes,
    // because that would immensely complicate the logic inside the
    // actual projection data extraction functions
    auto const& current = (*it).path;
    // this is a quadratic algorithm (:gut:), but it is only activated
    // as a safety check in maintainer mode, plus we are guaranteed to have
    // at most five projections right now
    auto it2 = std::find_if(_projections.begin(), _projections.end(),
                            [&current](auto const& other) {
                              return other.path[0] == current.path[0] &&
                                     other.path.size() != current.path.size();
                            });
    TRI_ASSERT(it2 == _projections.end());
  }

  // validate that no projection contains an empty attribute
  std::for_each(_projections.begin(), _projections.end(),
                [](auto const& it) { TRI_ASSERT(!it.path.empty()); });
#endif
}

/// @brief clean up projections, so that there are no 2 projections with a
/// shared prefix
void Projections::removeSharedPrefixes() {
  TRI_ASSERT(_projections.size() >= 2);

  auto current = _projections.begin();

  while (current != _projections.end()) {
    auto next = current + 1;
    if (next == _projections.end()) {
      // done
      break;
    }

    size_t commonPrefixLength =
        arangodb::aql::AttributeNamePath::commonPrefixLength((*current).path,
                                                             (*next).path);
    if (commonPrefixLength > 0) {
      // common prefix detected. now remove the longer of the paths
      if ((*current).path.size() > (*next).path.size()) {
        // shorten the other attribute to the shard prefix length
        (*next).path.shortenTo(commonPrefixLength);
        (*next).type = (*next).path.type();
        // current already adjusted
        current = _projections.erase(current);
      } else {
        TRI_ASSERT(current != next);
        (*current).path.shortenTo(commonPrefixLength);
        (*current).type = (*current).path.type();
        _projections.erase(next);
        // don't move current forward here
      }
    } else {
      ++current;
      // move to next element
    }
  }
}

Projections::Projection const& Projections::operator[](size_t index) const {
  TRI_ASSERT(index < size());
  return _projections[index];
}

Projections::Projection& Projections::operator[](size_t index) {
  TRI_ASSERT(index < size());
  return _projections[index];
}

}  // namespace aql
}  // namespace arangodb
