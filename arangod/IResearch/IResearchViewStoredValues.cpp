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

#include "Basics/AttributeNameParser.h"
#include "IResearchViewStoredValues.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include "VelocyPackHelper.h"

#include <unordered_set>

namespace {
bool isPrefix(std::vector<arangodb::basics::AttributeName> const& prefix,
              std::vector<arangodb::basics::AttributeName> const& attrs) {
  TRI_ASSERT(prefix.size() < attrs.size());

  for (size_t i = 0; i < prefix.size(); ++i) {
    TRI_ASSERT(!prefix[i].shouldExpand);
    if (prefix[i].name != attrs[i].name) {
      return false;
    }
  }

  return true;
}
}

namespace arangodb {
namespace iresearch {

const char IResearchViewStoredValues::FIELDS_DELIMITER = '\1';

bool IResearchViewStoredValues::toVelocyPack(velocypack::Builder& builder) const {
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

bool IResearchViewStoredValues::fromVelocyPack(
    velocypack::Slice slice, std::string& errorField) {
  clear();
  if (slice.isArray()) {
    _storedColumns.reserve(slice.length());
    std::unordered_set<std::string> uniqueColumns;
    std::vector<irs::string_ref> fieldNames;
    for (auto columnSlice : VPackArrayIterator(slice)) {
      if (columnSlice.isArray()) {
        // skip empty column
        if (columnSlice.length() == 0) {
          continue;
        }
        fieldNames.clear();
        size_t columnLength = 0;
        StoredColumn sc;
        sc.fields.reserve(columnSlice.length());
        for (auto fieldSlice : VPackArrayIterator(columnSlice)) {
          if (!fieldSlice.isString()) {
            clear();
            return false;
          }
          auto fieldName = getStringRef(fieldSlice);
          // skip empty field
          if (fieldName.empty()) {
            continue;
          }
          std::vector<basics::AttributeName> field;
          try {
            // no expansions
            basics::TRI_ParseAttributeString(fieldName, field, false);
          } catch (...) {
            errorField = fieldName;
            clear();
            return false;
          }
          // check field uniqueness
          auto newField = true;
          auto fieldSize = field.size();
          size_t i = 0;
          for (auto& f : sc.fields) {
            if (f.second.size() == fieldSize) {
              if (basics::AttributeName::isIdentical(f.second, field, false)) {
                newField = false;
                break;
              }
            } else if (f.second.size() < fieldSize) {
              if (isPrefix(f.second, field)) {
                newField = false;
                break;
              }
            } else { // f.second.size() > fieldSize
              if (isPrefix(field, f.second)) {
                // take shortest path field (obj.a is better than obj.a.sub_a)
                columnLength += fieldName.size() - f.second.size();
                f.first = fieldName;
                f.second = std::move(field);
                fieldNames[i] = std::move(fieldName);
                newField = false;
                break;
              }
            }
            ++i;
          }
          if (!newField) {
            continue;
          }
          sc.fields.emplace_back(fieldName, std::move(field));
          columnLength += fieldName.size() + 1; // + 1 for FIELDS_DELIMITER
          fieldNames.emplace_back(std::move(fieldName));
        }
        // skip empty column
        if (fieldNames.empty()) {
          continue;
        }
        // check column uniqueness
        std::sort(fieldNames.begin(), fieldNames.end());
        std::string columnName;
        TRI_ASSERT(columnLength > 1);
        columnName.reserve(columnLength);
        for (auto const& fieldName : fieldNames) {
          columnName += FIELDS_DELIMITER; // a prefix for EXISTS()
          columnName += fieldName;
        }
        if (!uniqueColumns.emplace(columnName).second) {
          continue;
        }
        sc.name = std::move(columnName);
        _storedColumns.emplace_back(std::move(sc));
      } else if (columnSlice.isString()) {
        auto fieldName = getStringRef(columnSlice);
        // skip empty field
        if (fieldName.empty()) {
          continue;
        }
        std::vector<basics::AttributeName> field;
        try {
          // no expansions
          basics::TRI_ParseAttributeString(fieldName, field, false);
        } catch (...) {
          errorField = fieldName;
          clear();
          return false;
        }
        if (!uniqueColumns.emplace(fieldName).second) {
          continue;
        }
        std::string columnName;
        columnName.reserve(fieldName.size() + 1);
        columnName += FIELDS_DELIMITER; // a prefix for EXISTS()
        columnName += fieldName;
        StoredColumn sc;
        sc.fields.emplace_back(fieldName, std::move(field));
        sc.name = std::move(columnName);
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

size_t IResearchViewStoredValues::memory() const noexcept {
  size_t size = sizeof(IResearchViewStoredValues);
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
