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

#include "PhysicalCollection.h"

#include "Basics/StaticStrings.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief merge two objects for update, oldValue must have correctly set
/// _key and _id attributes

void PhysicalCollection::mergeObjectsForUpdate(
    transaction::Methods* trx, VPackSlice const& oldValue,
    VPackSlice const& newValue, bool isEdgeCollection, std::string const& rev,
    bool mergeObjects, bool keepNull, VPackBuilder& b) {
  b.openObject();

  VPackSlice keySlice = oldValue.get(StaticStrings::KeyString);
  VPackSlice idSlice = oldValue.get(StaticStrings::IdString);
  TRI_ASSERT(!keySlice.isNone());
  TRI_ASSERT(!idSlice.isNone());

  // Find the attributes in the newValue object:
  VPackSlice fromSlice;
  VPackSlice toSlice;

  std::unordered_map<std::string, VPackSlice> newValues;
  {
    VPackObjectIterator it(newValue, true);
    while (it.valid()) {
      std::string key = it.key().copyString();
      if (!key.empty() && key[0] == '_' &&
          (key == StaticStrings::KeyString || key == StaticStrings::IdString ||
           key == StaticStrings::RevString ||
           key == StaticStrings::FromString ||
           key == StaticStrings::ToString)) {
        // note _from and _to and ignore _id, _key and _rev
        if (key == StaticStrings::FromString) {
          fromSlice = it.value();
        } else if (key == StaticStrings::ToString) {
          toSlice = it.value();
        }  // else do nothing
      } else {
        // regular attribute
        newValues.emplace(std::move(key), it.value());
      }

      it.next();
    }
  }

  if (isEdgeCollection) {
    if (fromSlice.isNone()) {
      fromSlice = oldValue.get(StaticStrings::FromString);
    }
    if (toSlice.isNone()) {
      toSlice = oldValue.get(StaticStrings::ToString);
    }
  }

  // add system attributes first, in this order:
  // _key, _id, _from, _to, _rev

  // _key
  b.add(StaticStrings::KeyString, keySlice);

  // _id
  b.add(StaticStrings::IdString, idSlice);

  // _from, _to
  if (isEdgeCollection) {
    TRI_ASSERT(!fromSlice.isNone());
    TRI_ASSERT(!toSlice.isNone());
    b.add(StaticStrings::FromString, fromSlice);
    b.add(StaticStrings::ToString, toSlice);
  }

  // _rev
  b.add(StaticStrings::RevString, VPackValue(rev));

  // add other attributes after the system attributes
  {
    VPackObjectIterator it(oldValue, true);
    while (it.valid()) {
      std::string key = it.key().copyString();
      // exclude system attributes in old value now
      if (!key.empty() && key[0] == '_' &&
          (key == StaticStrings::KeyString || key == StaticStrings::IdString ||
           key == StaticStrings::RevString ||
           key == StaticStrings::FromString ||
           key == StaticStrings::ToString)) {
        it.next();
        continue;
      }

      auto found = newValues.find(key);

      if (found == newValues.end()) {
        // use old value
        b.add(key, it.value());
      } else if (mergeObjects && it.value().isObject() &&
                 (*found).second.isObject()) {
        // merge both values
        auto& value = (*found).second;
        if (keepNull || (!value.isNone() && !value.isNull())) {
          VPackBuilder sub =
              VPackCollection::merge(it.value(), value, true, !keepNull);
          b.add(key, sub.slice());
        }
        // clear the value in the map so its not added again
        (*found).second = VPackSlice();
      } else {
        // use new value
        auto& value = (*found).second;
        if (keepNull || (!value.isNone() && !value.isNull())) {
          b.add(key, value);
        }
        // clear the value in the map so its not added again
        (*found).second = VPackSlice();
      }
      it.next();
    }
  }

  // add remaining values that were only in new object
  for (auto const& it : newValues) {
    VPackSlice const& s = it.second;
    if (s.isNone()) {
      continue;
    }
    if (!keepNull && s.isNull()) {
      continue;
    }
    b.add(it.first, s);
  }

  b.close();
}

/// @brief new object for replace, oldValue must have _key and _id correctly
/// set
void PhysicalCollection::newObjectForReplace(
    transaction::Methods* trx, VPackSlice const& oldValue,
    VPackSlice const& newValue, VPackSlice const& fromSlice,
    VPackSlice const& toSlice, bool isEdgeCollection, std::string const& rev,
    VPackBuilder& builder) {
  builder.openObject();

  // add system attributes first, in this order:
  // _key, _id, _from, _to, _rev

  // _key
  VPackSlice s = oldValue.get(StaticStrings::KeyString);
  TRI_ASSERT(!s.isNone());
  builder.add(StaticStrings::KeyString, s);

  // _id
  s = oldValue.get(StaticStrings::IdString);
  TRI_ASSERT(!s.isNone());
  builder.add(StaticStrings::IdString, s);

  // _from and _to here
  if (isEdgeCollection) {
    TRI_ASSERT(!fromSlice.isNone());
    TRI_ASSERT(!toSlice.isNone());
    builder.add(StaticStrings::FromString, fromSlice);
    builder.add(StaticStrings::ToString, toSlice);
  }

  // _rev
  builder.add(StaticStrings::RevString, VPackValue(rev));

  // add other attributes after the system attributes
  TRI_SanitizeObjectWithEdges(newValue, builder);

  builder.close();
}
