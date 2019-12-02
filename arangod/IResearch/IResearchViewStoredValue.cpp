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

#include <unordered_set>

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

const char FIELDS_DELIMITER = '\1';

bool IResearchViewStoredValue::toVelocyPack(velocypack::Builder& builder) const {
  if (!builder.isOpenArray()) {
    return false;
  }
  for (auto const& column : _storedColumns) {
    velocypack::ArrayBuilder arrayScope(&builder);
    for (auto const& field : column.fields) {
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
    std::unordered_set<std::string> uniqueColumns;
    std::unordered_set<irs::string_ref> uniqueFields;
    std::vector<irs::string_ref> fieldNames;
    std::vector<basics::AttributeName> field;
    for (auto columnSlice : VPackArrayIterator(slice)) {
      if (columnSlice.isArray()) {
        uniqueFields.clear();
        fieldNames.clear();
        size_t columnLength = 0;
        StoredColumn sc;
        sc.fields.reserve(columnSlice.length());
        for (auto fieldSlice : VPackArrayIterator(columnSlice)) {
          if (!fieldSlice.isString()) {
            clear();
            return false;
          }
          auto fieldName = arangodb::iresearch::getStringRef(slice);
          // check field uniqueness
          if (uniqueFields.find(fieldName) != uniqueFields.cend()) {
            continue;
          }
          uniqueFields.emplace_hint(uniqueFields.cend(), fieldName);
          columnLength += fieldName.size();
          fieldNames.emplace_back(std::move(fieldName));
          field.clear();
          try {
            // No expansions
            arangodb::basics::TRI_ParseAttributeString(fieldName, field, false);
          } catch (...) {
            error = "." + std::string(fieldName);
            clear();
            return false;
          }
          sc.fields.emplace_back(fieldName, std::move(field));
        }
        // check column uniqueness
        std::sort(fieldNames.begin(), fieldNames.end());
        std::string columnName;
        columnName.reserve(columnLength);
        for (auto const& fieldName : fieldNames) {
          if (!columnName.empty()) {
            columnName += FIELDS_DELIMITER;
          }
          columnName += fieldName;
        }
        if (uniqueColumns.find(columnName) != uniqueColumns.cend()) {
          continue;
        }
        uniqueColumns.emplace_hint(uniqueColumns.cend(), columnName);
        sc.name = std::move(columnName);
        _storedColumns.emplace_back(std::move(sc));
      } else if (columnSlice.isString()) {
        auto fieldName = arangodb::iresearch::getStringRef(slice);
        field.clear();
        try {
          arangodb::basics::TRI_ParseAttributeString(fieldName, field, false);
        } catch (...) {
          error = "." + std::string(fieldName);
          return false;
        }
        if (uniqueColumns.find(fieldName) != uniqueColumns.cend()) {
          continue;
        }
        uniqueColumns.emplace_hint(uniqueColumns.cend(), fieldName);
        StoredColumn sc;
        sc.fields.emplace_back(fieldName, std::move(field));
        sc.name = std::move(fieldName);
        _storedColumns.emplace_back(std::move(sc));
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
    size += column.name.size();
    size += sizeof(std::pair<std::string, std::vector<basics::AttributeName>>)*column.fields.size();
    for (auto const& field : column.fields) {
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
