////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/ResourceUsage.h"
#include "Basics/debugging.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <algorithm>
#include <iostream>

namespace {

/// @brief velocypack attribute name for serializing/unserializing projections
constexpr std::string_view projectionsKey("projections");

}  // namespace

namespace arangodb::aql {

Projections::Projections() = default;

Projections::Projections(std::vector<AttributeNamePath> paths) {
  init(std::move(paths));
}

Projections::Projections(std::unordered_set<AttributeNamePath> paths) {
  init(std::move(paths));
}

Projections::Projections(containers::FlatHashSet<AttributeNamePath> paths) {
  init(std::move(paths));
}

bool Projections::isCoveringIndexPosition(uint16_t position) noexcept {
  return position != kNoCoveringIndexPosition;
}

void Projections::clear() noexcept {
  _projections.clear();
  _datasourceId = DataSourceId::none();
  _index.reset();
}

/// @brief set the index context for projections using an index
void Projections::setCoveringContext(DataSourceId const& id,
                                     std::shared_ptr<Index> const& index) {
  _datasourceId = id;
  _index = index;
}

bool Projections::contains(Projection const& other) const noexcept {
  return std::any_of(_projections.begin(), _projections.end(),
                     [&](Projection const& p) { return p.path == other.path; });
}

/// @brief checks if we have a single attribute projection on the attribute
bool Projections::isSingle(std::string_view attribute) const noexcept {
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
    velocypack::Builder& b, velocypack::Slice slice,
    transaction::Methods const* trxPtr) const {
  TRI_ASSERT(b.isOpenObject());
  TRI_ASSERT(slice.isObject());

  size_t levelsOpen = 0;

  // the single-attribute projections are easy. we dispatch here based on the
  // attribute type there are a few optimized functions for retrieving _key,
  // _id, _from and _to.
  for (auto const& it : _projections) {
    if (it.type == AttributeNamePath::Type::IdAttribute) {
      // projection for "_id"
      TRI_ASSERT(levelsOpen == 0);
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], VPackValue(transaction::helpers::extractIdString(
                            trxPtr->resolver(), slice, slice)));

    } else if (it.type == AttributeNamePath::Type::KeyAttribute) {
      // projection for "_key"
      TRI_ASSERT(levelsOpen == 0);
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], transaction::helpers::extractKeyFromDocument(slice));
    } else if (it.type == AttributeNamePath::Type::FromAttribute) {
      // projection for "_from"
      TRI_ASSERT(levelsOpen == 0);
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], transaction::helpers::extractFromFromDocument(slice));
    } else if (it.type == AttributeNamePath::Type::ToAttribute) {
      // projection for "_to"
      TRI_ASSERT(levelsOpen == 0);
      TRI_ASSERT(it.path.size() == 1);
      b.add(it.path[0], transaction::helpers::extractToFromDocument(slice));
    } else if (it.type == AttributeNamePath::Type::SingleAttribute) {
      // projection for any other top-level attribute
      TRI_ASSERT(levelsOpen == 0);
      TRI_ASSERT(it.path.size() == 1);
      VPackSlice found = slice.get(it.path._path.at(0));
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
      TRI_ASSERT(levelsOpen <= it.startsAtLevel);
      TRI_ASSERT(it.type == AttributeNamePath::Type::MultiAttribute);
      TRI_ASSERT(it.path.size() > 1);

      VPackSlice found = slice;
      VPackSlice prev = found;
      size_t level = 0;
      while (level < it.path.size()) {
        found = found.get(it.path[level]);
        if (found.isNone() || level == it.path.size() - 1 ||
            !found.isObject()) {
          break;
        }
        if (level >= levelsOpen) {
          b.add(it.path[level], VPackValue(VPackValueType::Object));
          ++levelsOpen;
        }
        ++level;
        prev = found;
      }
      if (level >= it.startsAtLevel) {
        if (found.isCustom()) {
          b.add(it.path[level],
                VPackValue(transaction::helpers::extractIdString(
                    trxPtr->resolver(), found, prev)));
        } else {
          if (found.isNone()) {
            found = VPackSlice::nullSlice();
          }
          b.add(it.path[level], found);
        }
      }

      TRI_ASSERT(it.path.size() > it.levelsToClose);
      size_t closeUntil = it.path.size() - it.levelsToClose;
      while (levelsOpen >= closeUntil) {
        b.close();
        --levelsOpen;
      }
    }
  }

  TRI_ASSERT(levelsOpen == 0);
}

/// @brief projections from a covering index
void Projections::toVelocyPackFromIndex(
    velocypack::Builder& b, IndexIteratorCoveringData& covering,
    transaction::Methods const* trxPtr) const {
  TRI_ASSERT(_index != nullptr);
  TRI_ASSERT(b.isOpenObject());

  size_t levelsOpen = 0;

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

      TRI_ASSERT(levelsOpen <= it.startsAtLevel);
      size_t level = 0;
      size_t const n =
          std::min(it.path.size(), static_cast<size_t>(it.coveringIndexCutoff));
      while (level < n) {
        if (level == n - 1) {
          break;
        }
        if (level >= levelsOpen) {
          b.add(it.path[level], VPackValue(VPackValueType::Object));
          ++levelsOpen;
        }
        ++level;
      }
      if (level >= it.startsAtLevel) {
        b.add(it.path[level], found);
      }

      TRI_ASSERT(it.path.size() > it.levelsToClose);
      size_t closeUntil = it.path.size() - it.levelsToClose;
      while (levelsOpen >= closeUntil) {
        b.close();
        --levelsOpen;
      }
    } else {
      // no array Slice... this case will be triggered for indexes that
      // contain simple string values, such as the primary index or the
      // edge index
      TRI_ASSERT(levelsOpen == 0);
      TRI_ASSERT(it.path.size() == 1);

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

  TRI_ASSERT(levelsOpen == 0);
}

void Projections::toVelocyPack(velocypack::Builder& b) const {
  toVelocyPack(b, ::projectionsKey);
}

void Projections::toVelocyPack(velocypack::Builder& b,
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
      for (auto const& attribute : it.path._path) {
        b.add(VPackValue(attribute));
      }
      b.close();
    }
  }
  b.close();
}

/*static*/ Projections Projections::fromVelocyPack(
    velocypack::Slice slice, arangodb::ResourceMonitor& resourceMonitor) {
  return fromVelocyPack(slice, ::projectionsKey, resourceMonitor);
}

/*static*/ Projections Projections::fromVelocyPack(
    velocypack::Slice slice, std::string_view key,
    arangodb::ResourceMonitor& resourceMonitor) {
  std::vector<AttributeNamePath> projections;

  VPackSlice p = slice.get(key);
  if (p.isArray()) {
    for (auto const& it : velocypack::ArrayIterator(p)) {
      if (it.isString()) {
        projections.emplace_back(
            AttributeNamePath(it.copyString(), resourceMonitor));
      } else if (it.isArray()) {
        AttributeNamePath path;
        for (auto const& it2 : velocypack::ArrayIterator(it)) {
          path._path.emplace_back(it2.copyString());
        }
        projections.emplace_back(std::move(path));
      }
    }
  }

  return Projections(std::move(projections));
}

/// @brief shared init function
template<typename T>
void Projections::init(T paths) {
  _projections.reserve(paths.size());
  for (auto& path : paths) {
    if (path.empty()) {
      // ignore all completely empty paths
      continue;
    }

    // categorize the projection, based on the attribute name.
    // we do this here only once in order to not do expensive string
    // comparisons at runtime later
    AttributeNamePath::Type type = path.type();
    size_t length = path.size();
    if (length >= std::numeric_limits<uint16_t>::max()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "attribute path too long for projection");
    }
    // take over the projection
    _projections.emplace_back(
        Projection{std::move(path), kNoCoveringIndexPosition,
                   /*coveringIndexCutoff*/ 0, /*startsAtLevel*/ 0,
                   /*levelsToClose*/ static_cast<uint16_t>(length - 1), type});
  }

  TRI_ASSERT(_projections.size() <= paths.size());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // validate that no projection contains an empty attribute
  std::for_each(_projections.begin(), _projections.end(),
                [](auto const& it) { TRI_ASSERT(!it.path.empty()); });
#endif

  if (_projections.size() <= 1) {
    //    return;
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
  handleSharedPrefixes();
}

/// @brief clean up projections, so that there are no 2 projections where one
/// is a true prefix of another. also sets level attributes
void Projections::handleSharedPrefixes() {
  size_t levelsOpen = 0;
  auto current = _projections.begin();

  while (current != _projections.end()) {
    auto next = current + 1;

    (*current).startsAtLevel = static_cast<uint16_t>(levelsOpen);
    size_t const currentLength = (*current).path.size();
    TRI_ASSERT(currentLength >= 1);

    if (next == _projections.end()) {
      // done
      (*current).levelsToClose = static_cast<uint16_t>(currentLength - 1);
      break;
    }

    size_t commonPrefixLength =
        AttributeNamePath::commonPrefixLength((*current).path, (*next).path);
    if (commonPrefixLength > 0) {
      if (commonPrefixLength == currentLength &&
          currentLength == (*next).path.size()) {
        // both projections are exactly identical. remove the second one
        TRI_ASSERT(current != next);
        _projections.erase(next);
        // don't move current forward here
        continue;
      } else if (commonPrefixLength == currentLength &&
                 currentLength < (*next).path.size()) {
        // current is a true prefix of next
        TRI_ASSERT(current != next);
        _projections.erase(next);
        // don't move current forward here
        continue;
      }

      TRI_ASSERT(currentLength - commonPrefixLength >= 1);
      (*current).levelsToClose =
          static_cast<uint16_t>(currentLength - commonPrefixLength - 1);
      levelsOpen = commonPrefixLength;
    } else {
      (*current).levelsToClose = static_cast<uint16_t>(currentLength - 1);
      levelsOpen = 0;
    }
    ++current;
    // move to next element
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

std::vector<Projections::Projection> const& Projections::projections()
    const noexcept {
  return _projections;
}

std::ostream& operator<<(std::ostream& stream,
                         Projections::Projection const& projection) {
  stream << projection.path;
  return stream;
}

std::ostream& operator<<(std::ostream& stream, Projections const& projections) {
  stream << "[ ";
  size_t i = 0;
  for (auto const& it : projections.projections()) {
    if (i++ != 0) {
      stream << ", ";
    }
    stream << it;
  }
  stream << " ]";
  return stream;
}

template void Projections::init<std::vector<AttributeNamePath>>(
    std::vector<AttributeNamePath> paths);
template void Projections::init<std::unordered_set<AttributeNamePath>>(
    std::unordered_set<AttributeNamePath> paths);
template void Projections::init<containers::FlatHashSet<AttributeNamePath>>(
    containers::FlatHashSet<AttributeNamePath> paths);

}  // namespace arangodb::aql
