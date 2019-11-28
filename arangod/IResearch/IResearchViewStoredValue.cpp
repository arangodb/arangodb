////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewStoredValue.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include "VelocyPackHelper.h"

namespace arangodb {
namespace iresearch {

/*
{"links" : {
            "mycol1" : {"fields" : {"str" : {"analyzers" : ["text_en"]}}, "includeAllFields" : true, "storeValues" : "value",
                       "storedFields": [["obj.foo.val1", "obj.foo.val2"], ["obj.bar.val1", "obj.bar.val2"]]}, // string

            "mycol2" : {"fields" : {"str" : {"analyzers" : ["text_en"]}}, "includeAllFields" : true, "storeValues" : "value"}
           }
}
*/

bool IResearchViewStoredValue::toVelocyPack(velocypack::Builder& builder) const {
  if (!builder.isOpenArray()) {
    return false;
  }
  for (auto const& column : _storedColumns) {
    velocypack::ArrayBuilder arrayScope(&builder);
    for (auto const& field : column) {
      builder.add(VPackValue(field.first));
    }
  }
  return true;
}

bool IResearchViewStoredValue::fromVelocyPack(
    velocypack::Slice slice, std::string& error) {
  clear();
  if (slice.isArray()) {
    _storedColumns.reserve(slice.length());
    for (auto columnSlice : VPackArrayIterator(slice)) {
      if (columnSlice.isArray()) {
        StoredColumn sc;
        sc.reserve(columnSlice.length());
        for (auto fieldSlice : VPackArrayIterator(columnSlice)) {
          if (!fieldSlice.isString()) {
            clear();
            return false;
          }
          auto fieldName = arangodb::iresearch::getStringRef(slice);
          std::vector<basics::AttributeName> field;
          try {
            arangodb::basics::TRI_ParseAttributeString(fieldName, field, false);
          } catch (...) {
            error = "." + std::string(fieldName);
            clear();
            return false;
          }
          sc.emplace_back(fieldName, std::move(field));
        }
        _storedColumns.emplace_back(std::move(sc));
      } else if (columnSlice.isString()) {
        auto fieldName = arangodb::iresearch::getStringRef(slice);
        std::vector<basics::AttributeName> field;
        try {
          arangodb::basics::TRI_ParseAttributeString(fieldName, field, false);
        } catch (...) {
          error = "." + std::string(fieldName);
          return false;
        }
        _storedColumns.emplace_back(StoredColumn{{fieldName, std::move(field)}});
      } else {
        clear();
        return false;
      }
    }
    return true;
  }
  return false;
}

size_t IResearchViewStoredValue::memory() const noexcept {
  size_t size = sizeof(IResearchViewStoredValue);
  size += sizeof(StoredColumn)*_storedColumns.size();
  for (auto const& column : _storedColumns) {
    size += sizeof(std::pair<std::string, std::vector<basics::AttributeName>>)*column.size();
    for (auto const& field : column) {
      size += field.first.size();
      size += sizeof(basics::AttributeName)*field.second.size();
      for (auto const& attribute : field.second) {
        size += attribute.name.size();
      }
    }
  }
  return size;
}

} // iresearch
} // arangodb
