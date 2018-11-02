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

#include "MMFilesExportCursor.h"
#include "Aql/ExecutionState.h"
#include "MMFiles/MMFilesCollectionExport.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

MMFilesExportCursor::MMFilesExportCursor(
    TRI_vocbase_t& vocbase,
    CursorId id,
    arangodb::MMFilesCollectionExport* ex,
    size_t batchSize,
    double ttl,
    bool hasCount
)
    : Cursor(id, batchSize, ttl, hasCount),
      _guard(vocbase),
      _ex(ex),
      _position(0),
      _size(ex->_vpack.size()) {}

MMFilesExportCursor::~MMFilesExportCursor() { delete _ex; }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the cursor contains more data
////////////////////////////////////////////////////////////////////////////////

bool MMFilesExportCursor::hasNext() {
  if (_ex == nullptr) {
    return false;
  }

  return (_position < _size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next element (not implemented)
////////////////////////////////////////////////////////////////////////////////

VPackSlice MMFilesExportCursor::next() {
  // should not be called directly
  return VPackSlice();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cursor size
////////////////////////////////////////////////////////////////////////////////

size_t MMFilesExportCursor::count() const { return _size; }

std::pair<aql::ExecutionState, Result> MMFilesExportCursor::dump(VPackBuilder& builder,
                                                                 std::function<void()> const&) {
  return {aql::ExecutionState::DONE, dumpSync(builder)};
}

Result MMFilesExportCursor::dumpSync(VPackBuilder& builder) {
  auto ctx = transaction::StandaloneContext::Create(_guard.database());
  VPackOptions const* oldOptions = builder.options;

  builder.options = ctx->getVPackOptions();
  TRI_ASSERT(_ex != nullptr);

  auto const restrictionType = _ex->_restrictions.type;

  try {
    builder.add("result", VPackValue(VPackValueType::Array));
    size_t const n = batchSize();

    for (size_t i = 0; i < n; ++i) {
      if (!hasNext()) {
        break;
      }

      VPackSlice const slice(
          reinterpret_cast<char const*>(_ex->_vpack.at(_position++)));

      builder.openObject();

      // Copy over shaped values
      for (auto const& entry : VPackObjectIterator(slice)) {
        std::string key(entry.key.copyString());

        if (!CollectionExport::IncludeAttribute(restrictionType, _ex->_restrictions.fields,
                              key)) {
          // Ignore everything that should be excluded or not included
          continue;
        }
        // If we get here we need this entry in the final result
        if (entry.value.isCustom()) {
          builder.add(key,
                      VPackValue(builder.options->customTypeHandler->toString(
                          entry.value, builder.options, slice)));
        } else {
          builder.add(key, entry.value);
        }
      }
      builder.close();
    }
    builder.close();  // close Array

    // builder.add("hasMore", VPackValue(hasNext() ? "true" : "false"));
    // //should not be string
    builder.add("hasMore", VPackValue(hasNext()));

    if (hasNext()) {
      builder.add("id", VPackValue(std::to_string(id())));
    }

    if (hasCount()) {
      builder.add("count", VPackValue(static_cast<uint64_t>(count())));
    }

    if (!hasNext()) {
      // mark the cursor as deleted
      delete _ex;
      _ex = nullptr;
      this->setDeleted();
    }
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "internal error during MMFilesExportCursor::dump");
  }
  builder.options = oldOptions;
  return TRI_ERROR_NO_ERROR;
}

std::shared_ptr<transaction::Context> MMFilesExportCursor::context() const {
  return transaction::StandaloneContext::Create(_guard.database()); // likely not used
}

} // arangodb
