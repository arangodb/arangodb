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
#include "Transaction/Context.h"
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

Cursor::~Cursor() {}

VPackSlice Cursor::extra() const {
  if (_extra == nullptr) {
    return VPackSlice();
  }
  return _extra->slice();
}

VelocyPackCursor::VelocyPackCursor(TRI_vocbase_t* vocbase, CursorId id,
                                   aql::QueryResult&& result, size_t batchSize,
                                   std::shared_ptr<VPackBuilder> extra,
                                   double ttl, bool hasCount)
    : Cursor(id, batchSize, extra, ttl, hasCount),
      _vocbaseGuard(vocbase),
      _result(std::move(result)),
      _iterator(_result.result->slice()),
      _cached(_result.cached) {
  TRI_ASSERT(_result.result->slice().isArray());
}

/// @brief check whether the cursor contains more data
bool VelocyPackCursor::hasNext() {
  if (_iterator.valid()) {
    return true;
  }

  _isDeleted = true;
  return false;
}

/// @brief return the next element
VPackSlice VelocyPackCursor::next() {
  TRI_ASSERT(_result.result != nullptr);
  TRI_ASSERT(_iterator.valid());
  VPackSlice slice = _iterator.value();
  _iterator.next();
  return slice;
}

/// @brief return the cursor size
size_t VelocyPackCursor::count() const { return _iterator.size(); }

void VelocyPackCursor::dump(VPackBuilder& builder) {
  try {
    size_t const n = batchSize();
    size_t num = n;
    if (num == 0) {
      num = 1;
    } else if (num >= 10000) {
      num = 10000;
    }
    // reserve an arbitrary number of bytes for the result to save
    // some reallocs
    // (not accurate, but the actual size is unknown anyway)
    builder.buffer()->reserve(num * 32);

    VPackOptions const* oldOptions = builder.options;

    builder.options = _result.context->getVPackOptionsForDump();

    builder.add("result", VPackValue(VPackValueType::Array));
    for (size_t i = 0; i < n; ++i) {
      if (!hasNext()) {
        break;
      }
      builder.add(next());
    }
    builder.close();

    // builder.add("hasMore", VPackValue(hasNext() ? "true" : "false"));
    // //shoudl not be string
    builder.add("hasMore", VPackValue(hasNext()));

    if (hasNext()) {
      builder.add("id", VPackValue(std::to_string(id())));
    }

    if (hasCount()) {
      builder.add("count", VPackValue(static_cast<uint64_t>(count())));
    }

    if (extra().isObject()) {
      builder.add("extra", extra());
    }

    builder.add("cached", VPackValue(_cached));

    if (!hasNext()) {
      // mark the cursor as deleted
      this->deleted();
    }
    builder.options = oldOptions;
  } catch (arangodb::basics::Exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "internal error during VPackCursor::dump");
  }
}
