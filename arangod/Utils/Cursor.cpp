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
#include "Basics/VelocyPackDumper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Utils/CollectionExport.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/TransactionContext.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
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

VelocyPackCursor::VelocyPackCursor(TRI_vocbase_t* vocbase, CursorId id,
                                   aql::QueryResult&& result, size_t batchSize,
                                   std::shared_ptr<VPackBuilder> extra,
                                   double ttl, bool hasCount)
    : Cursor(id, batchSize, extra, ttl, hasCount),
      _vocbase(vocbase),
      _result(std::move(result)),
      _iterator(_result.result->slice()),
      _cached(_result.cached) {
  TRI_ASSERT(_result.result->slice().isArray());
  TRI_UseVocBase(vocbase);
}

VelocyPackCursor::~VelocyPackCursor() {
  TRI_ReleaseVocBase(_vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the cursor contains more data
////////////////////////////////////////////////////////////////////////////////

bool VelocyPackCursor::hasNext() {
  if (_iterator.valid()) {
    return true;
  }

  _isDeleted = true;
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next element
////////////////////////////////////////////////////////////////////////////////

VPackSlice VelocyPackCursor::next() {
  TRI_ASSERT(_result.result != nullptr);
  TRI_ASSERT(_iterator.valid());
  VPackSlice slice = _iterator.value();
  _iterator.next();
  return slice;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cursor size
////////////////////////////////////////////////////////////////////////////////

size_t VelocyPackCursor::count() const { return _iterator.size(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the cursor contents into a string buffer
////////////////////////////////////////////////////////////////////////////////

void VelocyPackCursor::dump(arangodb::basics::StringBuffer& buffer) {
  buffer.appendText("\"result\":[");

  size_t const n = batchSize();

  // reserve 48 bytes per result document by default, but only
  // if the specified batch size does not get out of hand
  // otherwise specifying a very high batch size would make the allocation fail
  // in every case, even if there were much less documents in the collection
  size_t num = n;
  if (num == 0) {
    num = 1;
  } else if (num >= 10000) {
    num = 10000;
  }
  int res = buffer.reserve(num * 48);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  arangodb::basics::VelocyPackDumper dumper(&buffer, _result.context->getVPackOptions());

  try {

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

      dumper.dumpValue(row);
    }
  } catch (arangodb::basics::Exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
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
    buffer.appendText(",\"extra\":");
    dumper.dumpValue(extraSlice);
  }

  buffer.appendText(",\"cached\":");
  buffer.appendText(_cached ? "true" : "false");

  if (!hasNext()) {
    // mark the cursor as deleted
    this->deleted();
  }
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
  auto transactionContext = std::make_shared<StandaloneTransactionContext>(_vocbase);
  VPackOptions* options = transactionContext->getVPackOptions();

  TRI_ASSERT(_ex != nullptr);
  auto const restrictionType = _ex->_restrictions.type;

  buffer.appendText("\"result\":[");

  VPackBuilder result;

  size_t const n = batchSize();

  for (size_t i = 0; i < n; ++i) {
    if (!hasNext()) {
      break;
    }

    if (i > 0) {
      buffer.appendChar(',');
    }

    VPackSlice const slice(reinterpret_cast<char const*>(_ex->_documents->at(_position++)));

    {
      result.clear();

      VPackObjectBuilder b(&result);
      // Copy over shaped values
      for (auto const& entry : VPackObjectIterator(slice)) {
        std::string key(entry.key.copyString());

        if (!IncludeAttribute(restrictionType, _ex->_restrictions.fields, key)) {
          // Ignore everything that should be excluded or not included
          continue;
        }
        // If we get here we need this entry in the final result
        if (entry.value.isCustom()) {
          result.add(key, VPackValue(options->customTypeHandler->toString(entry.value, options, slice)));
        } else {
          result.add(key, entry.value);
        }
      }
    }

    arangodb::basics::VPackStringBufferAdapter bufferAdapter(buffer.stringBuffer());

    try {
      VPackDumper dumper(&bufferAdapter, options);
      dumper.dump(result.slice());
    } catch (arangodb::basics::Exception const& ex) {
      THROW_ARANGO_EXCEPTION_MESSAGE(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
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
