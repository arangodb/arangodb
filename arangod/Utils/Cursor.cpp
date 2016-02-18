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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Cursor.h"
#include "Basics/JsonHelper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Utils/CollectionExport.h"
#include "VocBase/document-collection.h"
#include "VocBase/shaped-json.h"
#include "VocBase/vocbase.h"
#include "VocBase/VocShaper.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

Cursor::Cursor(CursorId id, size_t batchSize,
               std::shared_ptr<VPackBuilder> extra, double ttl, bool hasCount)
    : _id(id),
      _batchSize(batchSize),
      _position(0),
      _extra(extra),
      _ttl(ttl),
      _expires(TRI_microtime() + _ttl),
      _hasCount(hasCount),
      _isDeleted(false),
      _isUsed(false) {}

Cursor::~Cursor() {
}

VPackSlice Cursor::extra() const {
  if (_extra == nullptr) {
    VPackSlice empty;
    return empty;
  }
  return _extra->slice();
}

JsonCursor::JsonCursor(TRI_vocbase_t* vocbase, CursorId id,
                       std::shared_ptr<VPackBuilder> json, size_t batchSize,
                       std::shared_ptr<VPackBuilder> extra, double ttl,
                       bool hasCount, bool cached)
    : Cursor(id, batchSize, extra, ttl, hasCount),
      _vocbase(vocbase),
      _json(json),
      _size(json->slice().length()),
      _cached(cached) {
  TRI_ASSERT(json->slice().isArray());
  TRI_UseVocBase(vocbase);
}

JsonCursor::~JsonCursor() {
  freeJson();

  TRI_ReleaseVocBase(_vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the cursor contains more data
////////////////////////////////////////////////////////////////////////////////

bool JsonCursor::hasNext() {
  if (_position < _size) {
    return true;
  }

  freeJson();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next element
////////////////////////////////////////////////////////////////////////////////

VPackSlice JsonCursor::next() {
  TRI_ASSERT(_json != nullptr);
  TRI_ASSERT(_position < _size);
  VPackSlice slice = _json->slice();
  return slice.at(_position++);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cursor size
////////////////////////////////////////////////////////////////////////////////

size_t JsonCursor::count() const { return _size; }

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the cursor contents into a string buffer
////////////////////////////////////////////////////////////////////////////////

void JsonCursor::dump(arangodb::basics::StringBuffer& buffer) {
  buffer.appendText("\"result\":[");

  size_t const n = batchSize();

  // reserve 48 bytes per result document by default, but only
  // if the specified batch size does not get out of hand
  // otherwise specifying a very high batch size would make the allocation fail
  // in every case, even if there were much less documents in the collection
  if (n <= 50000) {
    int res = buffer.reserve(n * 48);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  for (size_t i = 0; i < n; ++i) {
    if (!hasNext()) {
      break;
    }

    if (i > 0) {
      buffer.appendChar(',');
    }

    auto row = next();
    if (row.isNone()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    arangodb::basics::VPackStringBufferAdapter bufferAdapter(
        buffer.stringBuffer());
    VPackDumper dumper(&bufferAdapter);
    try {
      dumper.dump(row);
    } catch (...) {
      /// TODO correct error Handling!
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  }

  buffer.appendText("],\"hasMore\":");
  buffer.appendText(hasNext() ? "true" : "false");

  if (hasNext()) {
    // only return cursor id if there are more documents
    buffer.appendText(",\"id\":\"");
    buffer.appendInteger(id());
    buffer.appendText("\"");
  }

  if (hasCount()) {
    buffer.appendText(",\"count\":");
    buffer.appendInteger(static_cast<uint64_t>(count()));
  }

  VPackSlice const extraSlice = extra();

  if (extraSlice.isObject()) {
    arangodb::basics::VPackStringBufferAdapter bufferAdapter(
        buffer.stringBuffer());
    VPackDumper dumper(&bufferAdapter);
    buffer.appendText(",\"extra\":");
    dumper.dump(extraSlice);
  }

  buffer.appendText(",\"cached\":");
  buffer.appendText(_cached ? "true" : "false");

  if (!hasNext()) {
    // mark the cursor as deleted
    this->deleted();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the internals
////////////////////////////////////////////////////////////////////////////////

void JsonCursor::freeJson() {
  _json = nullptr;

  _isDeleted = true;
}

ExportCursor::ExportCursor(TRI_vocbase_t* vocbase, CursorId id,
                           arangodb::CollectionExport* ex, size_t batchSize,
                           double ttl, bool hasCount)
    : Cursor(id, batchSize, nullptr, ttl, hasCount),
      _vocbase(vocbase),
      _ex(ex),
      _size(ex->_documents->size()) {
  TRI_UseVocBase(vocbase);
}

ExportCursor::~ExportCursor() {
  delete _ex;
  TRI_ReleaseVocBase(_vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the cursor contains more data
////////////////////////////////////////////////////////////////////////////////

bool ExportCursor::hasNext() {
  if (_ex == nullptr) {
    return false;
  }

  return (_position < _size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next element (not implemented)
////////////////////////////////////////////////////////////////////////////////

VPackSlice ExportCursor::next() {
  // should not be called directly
  VPackSlice slice;
  return slice;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cursor size
////////////////////////////////////////////////////////////////////////////////

size_t ExportCursor::count() const { return _size; }

static bool IncludeAttribute(
    CollectionExport::Restrictions::Type const restrictionType,
    std::unordered_set<std::string> const& fields, std::string const& key) {
  if (restrictionType == CollectionExport::Restrictions::RESTRICTION_INCLUDE ||
      restrictionType == CollectionExport::Restrictions::RESTRICTION_EXCLUDE) {
    bool const keyContainedInRestrictions =
        (fields.find(key) !=
         fields.end());
    if ((restrictionType ==
             CollectionExport::Restrictions::RESTRICTION_INCLUDE &&
         !keyContainedInRestrictions) ||
        (restrictionType ==
             CollectionExport::Restrictions::RESTRICTION_EXCLUDE &&
         keyContainedInRestrictions)) {
      // exclude the field
      return false;
    }
    // include the field
    return true;
  } else {
    // no restrictions
    TRI_ASSERT(restrictionType ==
               CollectionExport::Restrictions::RESTRICTION_NONE);
    return true;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the cursor contents into a string buffer
////////////////////////////////////////////////////////////////////////////////

void ExportCursor::dump(arangodb::basics::StringBuffer& buffer) {
  TRI_ASSERT(_ex != nullptr);

  auto shaper = _ex->_document->getShaper();
  auto const restrictionType = _ex->_restrictions.type;

  buffer.appendText("\"result\":[");

  size_t const n = batchSize();

  for (size_t i = 0; i < n; ++i) {
    if (!hasNext()) {
      break;
    }

    if (i > 0) {
      buffer.appendChar(',');
    }

    auto marker =
        static_cast<TRI_df_marker_t const*>(_ex->_documents->at(_position++));

    TRI_shaped_json_t shaped;
    TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, marker);
    // Only Temporary wait for Shaped ==> VPack
    std::unique_ptr<TRI_json_t> tmp(TRI_JsonShapedJson(shaper, &shaped)); 
    std::shared_ptr<VPackBuilder> builder = arangodb::basics::JsonHelper::toVelocyPack(tmp.get());

    if (builder == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    VPackSlice const shapedSlice = builder->slice();
    VPackBuilder result;
    {
      VPackObjectBuilder b(&result);
      // Copy over shaped values
      for (auto const& entry : VPackObjectIterator(shapedSlice)) {
        std::string key = entry.key.copyString();
        if (key == TRI_VOC_ATTRIBUTE_ID || key == TRI_VOC_ATTRIBUTE_KEY ||
            key == TRI_VOC_ATTRIBUTE_FROM || key == TRI_VOC_ATTRIBUTE_TO ||
            key == TRI_VOC_ATTRIBUTE_REV) {
          // This if excludes all internal values. Just to make sure they are not present when added later.
          continue;
        }
        if (!IncludeAttribute(restrictionType, _ex->_restrictions.fields, key)) {
          // Ignore everything that should be excluded or not included
          continue;
        }
        // If we get here we need this entry in the final result
        result.add(key, entry.value);
      }
      // append the internal attributes
   
      // _id, _key, _rev
      char const* key = TRI_EXTRACT_MARKER_KEY(marker);
      if (IncludeAttribute(restrictionType, _ex->_restrictions.fields, TRI_VOC_ATTRIBUTE_ID)) {
        std::string id(
            _ex->_resolver.getCollectionName(_ex->_document->_info.id()));
        id.push_back('/');
        id.append(key);
        result.add(TRI_VOC_ATTRIBUTE_ID, VPackValue(id));
      }
      if (IncludeAttribute(restrictionType, _ex->_restrictions.fields, TRI_VOC_ATTRIBUTE_KEY)) {
        result.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(key));
      }
      if (IncludeAttribute(restrictionType, _ex->_restrictions.fields, TRI_VOC_ATTRIBUTE_REV)) {
        std::string rev = std::to_string(TRI_EXTRACT_MARKER_RID(marker)); 
        result.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(rev));
      }

      if (TRI_IS_EDGE_MARKER(marker)) {
        if (IncludeAttribute(restrictionType, _ex->_restrictions.fields, TRI_VOC_ATTRIBUTE_FROM)) {
          // _from
          std::string from(_ex->_resolver.getCollectionNameCluster(
              TRI_EXTRACT_MARKER_FROM_CID(marker)));
          from.push_back('/');
          from.append(TRI_EXTRACT_MARKER_FROM_KEY(marker));
          result.add(TRI_VOC_ATTRIBUTE_FROM, VPackValue(from));
        }

        if (IncludeAttribute(restrictionType, _ex->_restrictions.fields, TRI_VOC_ATTRIBUTE_TO)) {
          // _to
          std::string to(_ex->_resolver.getCollectionNameCluster(
              TRI_EXTRACT_MARKER_TO_CID(marker)));
          to.push_back('/');
          to.append(TRI_EXTRACT_MARKER_TO_KEY(marker));
          result.add(TRI_VOC_ATTRIBUTE_FROM, VPackValue(to));
        }
      }
    }
    arangodb::basics::VPackStringBufferAdapter bufferAdapter(
        buffer.stringBuffer());
    VPackDumper dumper(&bufferAdapter);
    try {
      dumper.dump(result.slice());
    } catch (...) {
      /// TODO correct error Handling!
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  }

  buffer.appendText("],\"hasMore\":");
  buffer.appendText(hasNext() ? "true" : "false");

  if (hasNext()) {
    // only return cursor id if there are more documents
    buffer.appendText(",\"id\":\"");
    buffer.appendInteger(id());
    buffer.appendText("\"");
  }

  if (hasCount()) {
    buffer.appendText(",\"count\":");
    buffer.appendInteger(static_cast<uint64_t>(count()));
  }

  if (!hasNext()) {
    delete _ex;
    _ex = nullptr;

    // mark the cursor as deleted
    this->deleted();
  }
}
