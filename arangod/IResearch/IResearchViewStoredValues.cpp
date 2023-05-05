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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#include "Basics/AttributeNameParser.h"
#include "IResearchViewStoredValues.h"
#include "IResearchCommon.h"

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
}  // namespace

namespace arangodb {
namespace iresearch {

const char IResearchViewStoredValues::FIELDS_DELIMITER = '\1';

bool IResearchViewStoredValues::toVelocyPack(
    velocypack::Builder& builder) const {
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
    std::string_view encodedCompression =
        columnCompressionToString(column.compression);
    TRI_ASSERT(!irs::IsNull(encodedCompression));
    if (ADB_LIKELY(!irs::IsNull(encodedCompression))) {
      addStringRef(builder, COMPRESSION_COLUMN_PARAM, encodedCompression);
    }
#ifdef USE_ENTERPRISE
    // do not output falses to keep old definitions unchanged
    if (column.cached) {
      builder.add(StaticStrings::kCacheField, VPackValue(column.cached));
    }
#endif
  }
  return true;
}

bool IResearchViewStoredValues::buildStoredColumnFromSlice(
    velocypack::Slice const& columnSlice,
    containers::FlatHashSet<std::string>& uniqueColumns,
    std::vector<std::string_view>& fieldNames,
    irs::type_info::type_id compression, bool cached) {
  TRI_ASSERT(columnSlice.isArray() || columnSlice.isString());
  fieldNames.clear();
  size_t columnLength = 0;
  StoredColumn sc;
  auto addColumn = [&](std::string_view fieldName) {
    if (fieldName.empty()) {
      return true;
    }
    std::vector<basics::AttributeName> field;
    try {
      // no expansions
      basics::TRI_ParseAttributeString(fieldName, field, false);
    } catch (...) {
      return false;
    }
    // check field uniqueness
    auto fieldSize = field.size();
    size_t i = 0;
    for (auto& f : sc.fields) {
      if (f.second.size() == fieldSize) {
        if (basics::AttributeName::isIdentical(f.second, field, false)) {
          return true;
        }
      } else if (f.second.size() < fieldSize) {
        if (isPrefix(f.second, field)) {
          return true;
        }
      } else {  // f.second.size() > fieldSize
        if (isPrefix(field, f.second)) {
          // take shortest path field (obj.a is better than obj.a.sub_a)
          columnLength += fieldName.size() - f.second.size();
          f.first = fieldName;
          f.second = std::move(field);
          fieldNames[i] = fieldName;
          return true;
        }
      }
      ++i;
    }
    sc.fields.emplace_back(fieldName, std::move(field));
    columnLength += fieldName.size() + 1;  // + 1 for FIELDS_DELIMITER
    fieldNames.emplace_back(fieldName);
    return true;
  };
  if (columnSlice.isArray()) {
    sc.fields.reserve(columnSlice.length());
    for (auto fieldSlice : VPackArrayIterator(columnSlice)) {
      if (!fieldSlice.isString() || !addColumn(fieldSlice.stringView())) {
        clear();
        return false;
      }
    }
  } else if (!addColumn(columnSlice.stringView())) {
    clear();
    return false;
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
    columnName += FIELDS_DELIMITER;  // a prefix for EXISTS()
    columnName += fieldName;
  }
  if (!uniqueColumns.emplace(columnName).second) {
    return true;
  }
  sc.name = std::move(columnName);
  sc.compression = compression;
  sc.cached = cached;
  _storedColumns.emplace_back(std::move(sc));
  return true;
}

bool IResearchViewStoredValues::fromVelocyPack(velocypack::Slice slice,
                                               std::string& errorField) {
  clear();
  if (!slice.isArray()) {
    return false;
  }
  _storedColumns.reserve(slice.length());
  containers::FlatHashSet<std::string> uniqueColumns;
  std::vector<std::string_view> fieldNames;
  int idx = -1;
  for (auto columnSlice : VPackArrayIterator(slice)) {
    ++idx;
    if (columnSlice.isObject()) {
      auto data = columnSlice.get(FIELD_COLUMN_PARAM);
      if (ADB_LIKELY(!data.isNone())) {
        auto compression = getDefaultCompression();
        if (columnSlice.hasKey(COMPRESSION_COLUMN_PARAM)) {
          auto compressionKey = columnSlice.get(COMPRESSION_COLUMN_PARAM);
          if (ADB_LIKELY(compressionKey.isString())) {
            auto decodedCompression = columnCompressionFromString(
                iresearch::getStringRef(compressionKey));
            if (ADB_LIKELY(decodedCompression != nullptr)) {
              compression = decodedCompression;
            } else {
              errorField =
                  "[" + std::to_string(idx) + "]." + COMPRESSION_COLUMN_PARAM;
              return false;
            }
          } else {
            errorField =
                "[" + std::to_string(idx) + "]." + COMPRESSION_COLUMN_PARAM;
            return false;
          }
        }
        bool cached = false;
#ifdef USE_ENTERPRISE
        auto cachedField = columnSlice.get(StaticStrings::kCacheField);
        if (!cachedField.isNone()) {
          if (!cachedField.isBool()) {
            errorField = "[" + std::to_string(idx) + "]." +
                         std::string(StaticStrings::kCacheField);
            return false;
          }
          cached = cachedField.getBool();
        }
#endif
        if (!data.isArray() ||
            !buildStoredColumnFromSlice(data, uniqueColumns, fieldNames,
                                        compression, cached)) {
          errorField = "[" + std::to_string(idx) + "]." + FIELD_COLUMN_PARAM;
          return false;
        }
      } else {
        errorField = "[" + std::to_string(idx) + "]";
        return false;
      }
    } else {
      if (!(columnSlice.isArray() || columnSlice.isString()) ||
          !buildStoredColumnFromSlice(columnSlice, uniqueColumns, fieldNames,
                                      getDefaultCompression(), false)) {
        errorField = "[" + std::to_string(idx) + "]." + FIELD_COLUMN_PARAM;
        return false;
      }
    }
  }
  return true;
}

size_t IResearchViewStoredValues::memory() const noexcept {
  size_t size = sizeof(IResearchViewStoredValues);
  size += sizeof(StoredColumn) * _storedColumns.size();
  for (auto const& column : _storedColumns) {
    size += column.name.size();
    size += sizeof(std::pair<std::string, std::vector<basics::AttributeName>>) *
            column.fields.size();
    for (auto const& field : column.fields) {
      size += field.first.size();
      size += sizeof(basics::AttributeName) * field.second.size();
      for (auto const& attribute : field.second) {
        size += attribute.name.size();
      }
    }
  }
  return size;
}

}  // namespace iresearch
}  // namespace arangodb
