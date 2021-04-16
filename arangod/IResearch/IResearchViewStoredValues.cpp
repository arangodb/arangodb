////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#include "Basics/AttributeNameParser.h"
#include "IResearchViewStoredValues.h"

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

const char* FIELD_COLUMN_PARAM = "fields";
const char* COMPRESSION_COLUMN_PARAM = "compression";
}

namespace arangodb {
namespace iresearch {

const char IResearchViewStoredValues::FIELDS_DELIMITER = '\1';

bool IResearchViewStoredValues::toVelocyPack(velocypack::Builder& builder) const {
  if (!builder.isOpenArray()) {
    return false;
  }
  for (auto const& column : _storedColumns) {
    velocypack::ObjectBuilder objectScope(&builder);
    {
      velocypack::ArrayBuilder arrayScope(&builder, FIELD_COLUMN_PARAM);
      for (auto const& field : column.fields) {
        builder.add(VPackValue(field.first));
      }
    }
    irs::string_ref encodedCompression = columnCompressionToString(column.compression);
    TRI_ASSERT(!encodedCompression.null());
    if (ADB_LIKELY(!encodedCompression.null())) {
      addStringRef(builder, COMPRESSION_COLUMN_PARAM, encodedCompression);
    }
  }
  return true;
}

bool IResearchViewStoredValues::buildStoredColumnFromSlice(
    velocypack::Slice const& columnSlice,
    std::unordered_set<std::string>& uniqueColumns,
    std::vector<irs::string_ref>& fieldNames,
    irs::type_info::type_id compression) {
  if (columnSlice.isArray()) {
    // skip empty column
    if (columnSlice.length() == 0) {
      return true;
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
      }
      catch (...) {
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
      // cppcheck-suppress accessMoved
      sc.fields.emplace_back(fieldName, std::move(field));
      columnLength += fieldName.size() + 1; // + 1 for FIELDS_DELIMITER
      // cppcheck-suppress accessMoved
      fieldNames.emplace_back(std::move(fieldName));
    }
    // skip empty column
    if (fieldNames.empty()) {
      return true;
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
      return true;
    }
    sc.name = std::move(columnName);
    sc.compression = compression;
    _storedColumns.emplace_back(std::move(sc));
  } else {
    return false;
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
    int idx = -1;
    for (auto columnSlice : VPackArrayIterator(slice)) {
      ++idx;
      if (columnSlice.isObject()) {
        if (ADB_LIKELY(columnSlice.hasKey(FIELD_COLUMN_PARAM))) {
          auto compression = getDefaultCompression();
          if (columnSlice.hasKey(COMPRESSION_COLUMN_PARAM)) {
            auto compressionKey = columnSlice.get(COMPRESSION_COLUMN_PARAM);
            if (ADB_LIKELY(compressionKey.isString())) {
              auto decodedCompression = columnCompressionFromString(iresearch::getStringRef(compressionKey));
              if (ADB_LIKELY(decodedCompression != nullptr)) {
                compression = decodedCompression;
              } else {
                errorField = "[" + std::to_string(idx) + "]." + COMPRESSION_COLUMN_PARAM;
                return false;
              }
            } else {
              errorField = "[" + std::to_string(idx) + "]." + COMPRESSION_COLUMN_PARAM;
              return false;
            }
          }
          if (!buildStoredColumnFromSlice(columnSlice.get(FIELD_COLUMN_PARAM),
                                          uniqueColumns, fieldNames, compression)) {
            errorField = "[" + std::to_string(idx) + "]." + FIELD_COLUMN_PARAM;
            return false;
          }
        } else {
          errorField = "[" + std::to_string(idx) + "]";
          return false;
        }
      } else {
        if (!buildStoredColumnFromSlice(columnSlice, uniqueColumns,
                                        fieldNames, getDefaultCompression())) {
          errorField = "[" + std::to_string(idx) + "]." + FIELD_COLUMN_PARAM;
          return false;
        }
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
