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

#include "Helpers.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/encoding.h"
#include "Cluster/ClusterMethods.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/BatchOptions.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/ComputedValues.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <string_view>

namespace arangodb::transaction {
namespace helpers {
namespace {

bool isSystemAttribute(std::string_view key) noexcept {
  switch (key.size()) {
    case 3:
      return key[0] == '_' &&
             (key == StaticStrings::IdString || key == StaticStrings::ToString);
    case 4:
      return key[0] == '_' && (key == StaticStrings::KeyString ||
                               key == StaticStrings::RevString);
    case 5:
      return key == StaticStrings::FromString;
    default:
      return false;
  }
}

}  // namespace

/// @brief quick access to the _key attribute in a database document
/// the document must have at least two attributes, and _key is supposed to
/// be the first one
VPackSlice extractKeyFromDocument(VPackSlice slice) {
  slice = slice.resolveExternal();
  TRI_ASSERT(slice.isObject());

  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // a regular document must have at least the three attributes
  // _key, _id and _rev (in this order). _key must be the first attribute
  // however this method may also be called for remove markers, which only
  // have _key and _rev. therefore the only assertion that we can make
  // here is that the document at least has two attributes

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());

  if (*p == basics::VelocyPackHelper::KeyAttribute) {
    // the + 1 is required so that we can skip over the attribute name
    // and point to the attribute value
    return VPackSlice(p + 1);
  }

  // fall back to the regular lookup method
  return slice.get(StaticStrings::KeyString);
}

/** @brief extract the _key attribute from a slice. If slice is an Object,
 * _key is read from the attribute. If the read attribute is a String, it is
 * returned, otherwise, the empty string is returned. If the given slice is a
 * String, the substring after '/' or the whole string if '/' does not appear is
 * returned.
 *
 * @param slice can be Object or String, otherwise an empty StringRef is
 * returned.
 * @return The _key attribute
 */
std::string_view extractKeyPart(velocypack::Slice slice, bool& keyPresent) {
  slice = slice.resolveExternal();
  keyPresent = false;

  // extract _key
  if (slice.isObject()) {
    VPackSlice k = slice.get(StaticStrings::KeyString);
    keyPresent = !k.isNone();
    if (!k.isString()) {
      return std::string_view();  // fail
    }
    return k.stringView();
  }
  if (slice.isString()) {
    keyPresent = true;
    return extractKeyPart(slice.stringView());
  }
  return std::string_view();
}

std::string_view extractKeyPart(velocypack::Slice slice) {
  [[maybe_unused]] bool unused;
  return extractKeyPart(slice, unused);
}

/** @brief Given a string, returns the substring after the first '/' or
 *          the whole string if it contains no '/'.
 */
std::string_view extractKeyPart(std::string_view key) {
  size_t pos = key.find('/');
  if (pos == std::string::npos) {
    return key;
  }
  return key.substr(pos + 1);
}

/// @brief extract the _id attribute from a slice, and convert it into a
/// string, static method
std::string extractIdString(CollectionNameResolver const* resolver,
                            VPackSlice slice, VPackSlice base) {
  VPackSlice id;

  slice = slice.resolveExternal();

  if (slice.isObject()) {
    // extract id attribute from object
    if (slice.isEmptyObject()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    }

    uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
    if (*p == basics::VelocyPackHelper::KeyAttribute) {
      // skip over attribute name
      ++p;
      VPackSlice key = VPackSlice(p);
      // skip over attribute value
      p += key.byteSize();

      if (*p == basics::VelocyPackHelper::IdAttribute) {
        id = VPackSlice(p + 1);
        if (id.isCustom()) {
          // we should be pointing to a custom value now
          TRI_ASSERT(id.head() == 0xf3);

          return makeIdFromCustom(resolver, id, key);
        }
        if (id.isString()) {
          return id.copyString();
        }
      }
    }

    // in case the quick access above did not work out, use the slow path...
    id = slice.get(StaticStrings::IdString);
  } else {
    id = slice;
  }

  if (id.isString()) {
    // already a string...
    return id.copyString();
  }
  if (!id.isCustom() || id.head() != 0xf3) {
    // invalid type for _id
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  // we now need to extract the _key attribute
  VPackSlice key;
  if (slice.isObject()) {
    key = slice.get(StaticStrings::KeyString);
  } else if (base.isObject()) {
    key = extractKeyFromDocument(base);
  } else if (base.isExternal()) {
    key = base.resolveExternal().get(StaticStrings::KeyString);
  }

  if (!key.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  return makeIdFromCustom(resolver, id, key);
}

/// @brief quick access to the _id attribute in a database document
/// the document must have at least two attributes, and _id is supposed to
/// be the second one
/// note that this may return a Slice of type Custom!
VPackSlice extractIdFromDocument(VPackSlice slice) {
  slice = slice.resolveExternal();
  TRI_ASSERT(slice.isObject());

  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // a regular document must have at least the three attributes
  // _key, _id and _rev (in this order). _id must be the second attribute

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());

  if (*p == basics::VelocyPackHelper::KeyAttribute) {
    // skip over _key
    ++p;
    // skip over _key value
    p += VPackSlice(p).byteSize();
    if (*p == basics::VelocyPackHelper::IdAttribute) {
      // the + 1 is required so that we can skip over the attribute name
      // and point to the attribute value
      return VPackSlice(p + 1);
    }
  }

  // fall back to the regular lookup method
  return slice.get(StaticStrings::IdString);
}

/// @brief quick access to the _from attribute in a database document
/// the document must have at least five attributes: _key, _id, _from, _to
/// and _rev (in this order)
VPackSlice extractFromFromDocument(VPackSlice slice) {
  slice = slice.resolveExternal();
  TRI_ASSERT(slice.isObject());

  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // this method must only be called on edges
  // this means we must have at least the attributes  _key, _id, _from, _to and
  // _rev

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;

  while (*p <= basics::VelocyPackHelper::FromAttribute && ++count <= 3) {
    if (*p == basics::VelocyPackHelper::FromAttribute) {
      // the + 1 is required so that we can skip over the attribute name
      // and point to the attribute value
      return VPackSlice(p + 1);
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }

  // fall back to the regular lookup method
  return slice.get(StaticStrings::FromString);
}

/// @brief quick access to the _to attribute in a database document
/// the document must have at least five attributes: _key, _id, _from, _to
/// and _rev (in this order)
VPackSlice extractToFromDocument(VPackSlice slice) {
  slice = slice.resolveExternal();
  TRI_ASSERT(slice.isObject());

  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // this method must only be called on edges
  // this means we must have at least the attributes  _key, _id, _from, _to and
  // _rev
  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;

  while (*p <= basics::VelocyPackHelper::ToAttribute && ++count <= 4) {
    if (*p == basics::VelocyPackHelper::ToAttribute) {
      // the + 1 is required so that we can skip over the attribute name
      // and point to the attribute value
      return VPackSlice(p + 1);
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }

  // fall back to the regular lookup method
  return slice.get(StaticStrings::ToString);
}

/// @brief extract _key and _rev from a document, in one go
/// this is an optimized version used when loading collections, WAL
/// collection and compaction
void extractKeyAndRevFromDocument(VPackSlice slice, VPackSlice& keySlice,
                                  RevisionId& revisionId) {
  slice = slice.resolveExternal();
  TRI_ASSERT(slice.isObject());
  TRI_ASSERT(slice.length() >= 2);

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;
  bool foundKey = false;
  bool foundRev = false;

  while (*p <= basics::VelocyPackHelper::ToAttribute && ++count <= 5) {
    if (*p == basics::VelocyPackHelper::KeyAttribute) {
      keySlice = VPackSlice(p + 1);
      if (foundRev) {
        return;
      }
      foundKey = true;
    } else if (*p == basics::VelocyPackHelper::RevAttribute) {
      VPackSlice revSlice(p + 1);
      revisionId = RevisionId::fromSlice(revSlice);
      if (foundKey) {
        return;
      }
      foundRev = true;
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }

  // fall back to regular lookup
  {
    keySlice = slice.get(StaticStrings::KeyString);
    revisionId = RevisionId::fromString(
        slice.get(StaticStrings::RevString).stringView());
  }
}

/// @brief extract _rev from a database document
RevisionId extractRevFromDocument(VPackSlice slice) {
  TRI_ASSERT(slice.isObject());
  TRI_ASSERT(slice.length() >= 2);

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;

  while (*p <= basics::VelocyPackHelper::ToAttribute && ++count <= 5) {
    if (*p == basics::VelocyPackHelper::RevAttribute) {
      VPackSlice revSlice(p + 1);
      return RevisionId::fromSlice(revSlice);
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }

  // fall back to regular lookup
  return RevisionId::fromSlice(slice);
}

VPackSlice extractRevSliceFromDocument(VPackSlice slice) {
  TRI_ASSERT(slice.isObject());
  TRI_ASSERT(slice.length() >= 2);

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;

  while (*p <= basics::VelocyPackHelper::ToAttribute && ++count <= 5) {
    if (*p == basics::VelocyPackHelper::RevAttribute) {
      return VPackSlice(p + 1);
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }

  // fall back to regular lookup
  return slice.get(StaticStrings::RevString);
}

std::string_view extractCollectionFromId(std::string_view id) {
  std::size_t index = id.find('/');
  if (index == std::string::npos) {
    // can't find the '/' to split, bail out with only logical response
    return id;
  }
  return std::string_view(id.data(), index);
}

OperationResult buildCountResult(
    OperationOptions const& options,
    std::vector<std::pair<std::string, uint64_t>> const& count, CountType type,
    uint64_t& total) {
  total = 0;
  VPackBuilder resultBuilder;

  if (type == CountType::kDetailed) {
    resultBuilder.openObject();
    for (auto const& it : count) {
      total += it.second;
      resultBuilder.add(it.first, VPackValue(it.second));
    }
    resultBuilder.close();
  } else {
    uint64_t result = 0;
    for (auto const& it : count) {
      total += it.second;
      result += it.second;
    }
    resultBuilder.add(VPackValue(result));
  }
  return OperationResult(Result(), resultBuilder.steal(), options);
}

/// @brief creates an id string from a custom _id value and the _key string
std::string makeIdFromCustom(CollectionNameResolver const* resolver,
                             VPackSlice id, VPackSlice key) {
  TRI_ASSERT(id.isCustom() && id.head() == 0xf3);
  TRI_ASSERT(key.isString());

  DataSourceId cid{
      encoding::readNumber<uint64_t>(id.begin() + 1, sizeof(uint64_t))};
  return makeIdFromParts(resolver, cid, key);
}

/// @brief creates an id string from a collection name and the _key string
std::string makeIdFromParts(CollectionNameResolver const* resolver,
                            DataSourceId const& cid, VPackSlice key) {
  TRI_ASSERT(key.isString());

  std::string resolved = resolver->getCollectionNameCluster(cid);
#ifdef USE_ENTERPRISE
  ClusterMethods::realNameFromSmartName(resolved);
#endif
  VPackValueLength keyLength;
  char const* p = key.getString(keyLength);
  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid _key value");
  }
  resolved.reserve(resolved.size() + 1 + keyLength);
  resolved.push_back('/');
  resolved.append(p, static_cast<size_t>(keyLength));
  return resolved;
}

/// @brief merge two objects for update, oldValue must have correctly set
/// _key and _id attributes
Result mergeObjectsForUpdate(Methods& trx, LogicalCollection& collection,
                             velocypack::Slice oldValue,
                             velocypack::Slice newValue, bool isNoOpUpdate,
                             RevisionId previousRevisionId,
                             RevisionId& revisionId,
                             velocypack::Builder& builder,
                             OperationOptions const& options,
                             BatchOptions& batchOptions) {
  BuilderLeaser b(&trx);
  b->openObject();

  VPackSlice keySlice = oldValue.get(StaticStrings::KeyString);
  VPackSlice idSlice = oldValue.get(StaticStrings::IdString);
  TRI_ASSERT(!keySlice.isNone());
  TRI_ASSERT(!idSlice.isNone());

  // Find the attributes in the newValue object:
  VPackSlice fromSlice;
  VPackSlice toSlice;

  containers::FlatHashMap<std::string_view, VPackSlice> newValues;
  {
    VPackObjectIterator it(newValue, true);
    while (it.valid()) {
      auto current = *it;
      auto key = current.key.stringView();
      if (isSystemAttribute(key)) {
        // note _from and _to and ignore _id, _key and _rev
        if (collection.type() == TRI_COL_TYPE_EDGE) {
          if (key == StaticStrings::FromString) {
            fromSlice = current.value;
          } else if (key == StaticStrings::ToString) {
            toSlice = current.value;
          }
        }  // else do nothing
      } else {
        // regular attribute
        newValues.emplace(key, current.value);
      }

      it.next();
    }
  }

  // add system attributes first, in this order:
  // _key, _id, _from, _to, _rev

  // _key
  b->add(StaticStrings::KeyString, keySlice);

  // _id
  b->add(StaticStrings::IdString, idSlice);

  // _from, _to
  if (collection.type() == TRI_COL_TYPE_EDGE) {
    bool extendedNames = trx.vocbase().extendedNames();

    if (fromSlice.isNone()) {
      fromSlice = oldValue.get(StaticStrings::FromString);
    } else if (!isValidEdgeAttribute(fromSlice, extendedNames)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }
    if (toSlice.isNone()) {
      toSlice = oldValue.get(StaticStrings::ToString);
    } else if (!isValidEdgeAttribute(toSlice, extendedNames)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }

    TRI_ASSERT(fromSlice.isString());
    TRI_ASSERT(toSlice.isString());
    b->add(StaticStrings::FromString, fromSlice);
    b->add(StaticStrings::ToString, toSlice);
  }

  // _rev
  bool handled = false;
  if (isNoOpUpdate) {
    // an update that doesn't update anything - reuse revision id
    char ridBuffer[basics::maxUInt64StringSize];
    b->add(StaticStrings::RevString, previousRevisionId.toValuePair(ridBuffer));
    revisionId = previousRevisionId;
    handled = true;
  } else if (options.isRestore) {
    // copy revision id verbatim
    VPackSlice s = newValue.get(StaticStrings::RevString);
    if (s.isString()) {
      b->add(StaticStrings::RevString, s);
      revisionId = RevisionId::fromString(s.stringView());
      handled = true;
    }
  }
  if (!handled) {
    // temporary buffer for stringifying revision ids
    char ridBuffer[basics::maxUInt64StringSize];
    revisionId = collection.newRevisionId();
    b->add(StaticStrings::RevString, revisionId.toValuePair(ridBuffer));
  }

  containers::FlatHashSet<std::string_view> keysWritten;

  // add other attributes after the system attributes
  {
    VPackObjectIterator it(oldValue, true);
    while (it.valid()) {
      auto current = (*it);
      auto key = current.key.stringView();
      // exclude system attributes in old value now
      if (isSystemAttribute(key)) {
        it.next();
        continue;
      }

      auto found = newValues.find(key);

      if (found == newValues.end()) {
        // use old value
        b->addUnchecked(key, current.value);
        if (batchOptions.computedValues != nullptr) {
          keysWritten.emplace(key);
        }
      } else if (options.mergeObjects && current.value.isObject() &&
                 (*found).second.isObject()) {
        // merge both values
        auto& value = (*found).second;
        if (options.keepNull || (!value.isNone() && !value.isNull())) {
          b->add(VPackValue(key, VPackValueType::String));
          VPackCollection::merge(*b, current.value, value, true,
                                 !options.keepNull);
          if (batchOptions.computedValues != nullptr) {
            keysWritten.emplace(key);
          }
        }
        // clear the value in the map so its not added again
        (*found).second = VPackSlice();
      } else {
        // use new value
        auto& value = (*found).second;
        if (options.keepNull || (!value.isNone() && !value.isNull())) {
          b->addUnchecked(key, value);
          if (batchOptions.computedValues != nullptr) {
            keysWritten.emplace(key);
          }
        }
        // clear the value in the map so its not added again
        (*found).second = VPackSlice();
      }
      it.next();
    }
  }

  // add remaining values that were only in new object
  for (auto const& it : newValues) {
    VPackSlice s = it.second;
    if (s.isNone()) {
      continue;
    }
    if (!options.keepNull && s.isNull()) {
      continue;
    }
    if (!options.keepNull && s.isObject()) {
      b->add(VPackValue(it.first));
      VPackCollection::merge(*b, VPackSlice::emptyObjectSlice(), s, true, true);
    } else {
      b->addUnchecked(it.first, s);
    }

    if (batchOptions.computedValues != nullptr) {
      keysWritten.emplace(it.first);
    }
  }

  b->close();

  if (batchOptions.computedValues != nullptr) {
    // add all remaining computed attributes, if we need to
    batchOptions.ensureComputedValuesContext(trx, collection);
    batchOptions.computedValues->mergeComputedAttributes(
        *batchOptions.computedValuesContext, trx, b->slice(), keysWritten,
        ComputeValuesOn::kUpdate, builder);
  } else {
    // add document as is
    builder.add(b->slice());
  }

  TRI_ASSERT(revisionId.isSet());
  return {};
}

/// @brief new object for insert, computes the hash of the key
Result newObjectForInsert(Methods& trx, LogicalCollection& collection,
                          std::string_view key, velocypack::Slice value,
                          RevisionId& revisionId, velocypack::Builder& builder,
                          OperationOptions const& options,
                          BatchOptions& batchOptions) {
  BuilderLeaser b(&trx);

  b->openObject();

  // add system attributes first, in this order:
  // _key, _id, _from, _to, _rev

  // _key
  b->add(StaticStrings::KeyString, key);

  // _id
  uint8_t* p = b->add(StaticStrings::IdString,
                      VPackValuePair(9ULL, VPackValueType::Custom));

  *p++ = 0xf3;  // custom type for _id

  if (trx.state()->isDBServer() && !collection.system()) {
    // db server in cluster, note: the local collections _statistics,
    // _statisticsRaw and _statistics15 (which are the only system
    // collections)
    // must not be treated as shards but as local collections
    encoding::storeNumber<uint64_t>(p, collection.planId().id(),
                                    sizeof(uint64_t));
  } else {
    // local server
    encoding::storeNumber<uint64_t>(p, collection.id().id(), sizeof(uint64_t));
  }

  // _from and _to
  if (collection.type() == TRI_COL_TYPE_EDGE) {
    bool extendedNames = trx.vocbase().extendedNames();

    VPackSlice fromSlice = value.get(StaticStrings::FromString);
    if (!isValidEdgeAttribute(fromSlice, extendedNames)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }

    VPackSlice toSlice = value.get(StaticStrings::ToString);
    if (!isValidEdgeAttribute(toSlice, extendedNames)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }

    TRI_ASSERT(fromSlice.isString());
    TRI_ASSERT(toSlice.isString());
    b->add(StaticStrings::FromString, fromSlice);
    b->add(StaticStrings::ToString, toSlice);
  }

  // _rev
  bool handled = false;
  bool isRestore = options.isRestore;
  TRI_IF_FAILURE("Insert::useRev") { isRestore = true; }
  if (isRestore) {
    // copy revision id verbatim
    auto s = value.get(StaticStrings::RevString);
    if (s.isString()) {
      b->add(StaticStrings::RevString, s);
      revisionId = RevisionId::fromString(s.stringView());
      handled = true;
    }
  }
  if (!handled) {
    // temporary buffer for stringifying revision ids
    char ridBuffer[basics::maxUInt64StringSize];
    revisionId = collection.newRevisionId();
    b->add(StaticStrings::RevString, revisionId.toValuePair(ridBuffer));
  }

  containers::FlatHashSet<std::string_view> keysWritten;

  // add other attributes after the system attributes
  VPackObjectIterator it(value, true);
  while (it.valid()) {
    auto key = it.key().stringView();
    // _id, _key, _rev, _from, _to. minimum size here is 3
    if (key.size() < 3 || key[0] != '_' ||
        (key != StaticStrings::KeyString && key != StaticStrings::IdString &&
         key != StaticStrings::RevString && key != StaticStrings::FromString &&
         key != StaticStrings::ToString)) {
      b->add(key, it.value());
      if (batchOptions.computedValues != nullptr) {
        // track which attributes we have produced so that they are not
        // added again by the computed attributes later.
        keysWritten.emplace(key);
      }
    }
    it.next();
  }

  b->close();

  if (batchOptions.computedValues != nullptr) {
    // add all remaining computed attributes, if we need to
    batchOptions.ensureComputedValuesContext(trx, collection);
    batchOptions.computedValues->mergeComputedAttributes(
        *batchOptions.computedValuesContext, trx, b->slice(), keysWritten,
        ComputeValuesOn::kInsert, builder);
  } else {
    // add document as is
    builder.add(b->slice());
  }

  TRI_ASSERT(revisionId.isSet());
  return {};
}

/// @brief new object for replace, oldValue must have _key and _id correctly
/// set
Result newObjectForReplace(Methods& trx, LogicalCollection& collection,
                           VPackSlice oldValue, VPackSlice newValue,
                           bool isNoOpReplace, RevisionId previousRevisionId,
                           RevisionId& revisionId, VPackBuilder& builder,
                           OperationOptions const& options,
                           BatchOptions& batchOptions) {
  BuilderLeaser b(&trx);
  b->openObject();

  // add system attributes first, in this order:
  // _key, _id, _from, _to, _rev

  // _key
  VPackSlice s = oldValue.get(StaticStrings::KeyString);
  TRI_ASSERT(!s.isNone());
  b->add(StaticStrings::KeyString, s);

  // _id
  s = oldValue.get(StaticStrings::IdString);
  TRI_ASSERT(!s.isNone());
  b->add(StaticStrings::IdString, s);

  // _from and _to
  if (collection.type() == TRI_COL_TYPE_EDGE) {
    bool extendedNames = trx.vocbase().extendedNames();

    VPackSlice fromSlice = newValue.get(StaticStrings::FromString);
    if (!isValidEdgeAttribute(fromSlice, extendedNames)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }

    VPackSlice toSlice = newValue.get(StaticStrings::ToString);
    if (!isValidEdgeAttribute(toSlice, extendedNames)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }

    TRI_ASSERT(fromSlice.isString());
    TRI_ASSERT(toSlice.isString());
    b->add(StaticStrings::FromString, fromSlice);
    b->add(StaticStrings::ToString, toSlice);
  }

  // _rev
  bool handled = false;
  if (isNoOpReplace) {
    // an replace that doesn't update anything - reuse revision id
    char ridBuffer[basics::maxUInt64StringSize];
    b->add(StaticStrings::RevString, previousRevisionId.toValuePair(ridBuffer));
    revisionId = previousRevisionId;
    handled = true;
  } else if (options.isRestore) {
    // copy revision id verbatim
    s = newValue.get(StaticStrings::RevString);
    if (s.isString()) {
      b->add(StaticStrings::RevString, s);
      revisionId = RevisionId::fromString(s.stringView());
      handled = true;
    }
  }
  if (!handled) {
    // temporary buffer for stringifying revision ids
    char ridBuffer[basics::maxUInt64StringSize];
    revisionId = collection.newRevisionId();
    b->add(StaticStrings::RevString, revisionId.toValuePair(&ridBuffer[0]));
  }

  containers::FlatHashSet<std::string_view> keysWritten;

  // add other attributes after the system attributes
  VPackObjectIterator it(newValue, true);
  while (it.valid()) {
    auto key = it.key().stringView();

    // _id, _key, _rev, _from, _to. minimum size here is 3
    if (key.size() < 3 || key[0] != '_' ||
        (key != StaticStrings::KeyString && key != StaticStrings::IdString &&
         key != StaticStrings::RevString && key != StaticStrings::FromString &&
         key != StaticStrings::ToString)) {
      b->add(key, it.value());
      if (batchOptions.computedValues != nullptr) {
        // track which attributes we have produced so that they are not
        // added again by the computed attributes later.
        keysWritten.emplace(key);
      }
    }
    it.next();
  }

  b->close();

  if (batchOptions.computedValues != nullptr) {
    // add all remaining computed attributes, if we need to
    batchOptions.ensureComputedValuesContext(trx, collection);
    batchOptions.computedValues->mergeComputedAttributes(
        *batchOptions.computedValuesContext, trx, b->slice(), keysWritten,
        ComputeValuesOn::kReplace, builder);
  } else {
    // add document as is
    builder.add(b->slice());
  }

  TRI_ASSERT(revisionId.isSet());
  return {};
}

bool isValidEdgeAttribute(velocypack::Slice slice, bool allowExtendedNames) {
  if (!slice.isString()) {
    return false;
  }

  // validate id string
  VPackValueLength len;
  char const* docId = slice.getStringUnchecked(len);
  [[maybe_unused]] size_t split = 0;
  return KeyGeneratorHelper::validateId(docId, static_cast<size_t>(len),
                                        allowExtendedNames, split);
}

}  // namespace helpers

StringLeaser::StringLeaser(Context* transactionContext)
    : _transactionContext(transactionContext),
      _string(_transactionContext->leaseString()) {}

StringLeaser::StringLeaser(Methods* trx)
    : StringLeaser{trx->transactionContextPtr()} {}

StringLeaser::~StringLeaser() {
  if (_string != nullptr) {
    _transactionContext->returnString(_string);
  }
}

BuilderLeaser::BuilderLeaser(Context* transactionContext)
    : _transactionContext{transactionContext},
      _builder{_transactionContext->leaseBuilder()} {
  TRI_ASSERT(_builder != nullptr);
}

BuilderLeaser::BuilderLeaser(Methods* trx)
    : BuilderLeaser{trx->transactionContextPtr()} {}

BuilderLeaser::~BuilderLeaser() { clear(); }

void BuilderLeaser::clear() {
  if (_builder != nullptr) {
    _transactionContext->returnBuilder(_builder);
    _builder = nullptr;
  }
}

}  // namespace arangodb::transaction
