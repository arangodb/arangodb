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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/RocksDBExportCursor.h"
#include "Basics/WriteLocker.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include "Logger/Logger.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

RocksDBExportCursor::RocksDBExportCursor(
    TRI_vocbase_t* vocbase, std::string const& name,
    CollectionExport::Restrictions const& restrictions, CursorId id,
    size_t limit, size_t batchSize, double ttl, bool hasCount)
    : Cursor(id, batchSize, nullptr, ttl, hasCount),
      _vocbaseGuard(vocbase),
      _resolver(vocbase),
      _restrictions(restrictions),
      _name(name),
      _mdr() {
  // prevent the collection from being unloaded while the export is ongoing
  // this may throw
  _collectionGuard.reset(
      new arangodb::CollectionGuard(vocbase, _name.c_str(), false));

  _collection = _collectionGuard->collection();

  _trx.reset(new SingleCollectionTransaction(
      transaction::StandaloneContext::Create(_collection->vocbase()), _name,
      AccessMode::Type::READ));

  // already locked by guard above
  _trx->addHint(transaction::Hints::Hint::NO_USAGE_LOCK);
  Result res = _trx->begin();

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto rocksCollection =
      static_cast<RocksDBCollection*>(_collection->getPhysical());
  _iter = rocksCollection->getAllIterator(_trx.get(), &_mdr, false);

  _size = _collection->numberDocuments(_trx.get());
  if (limit > 0 && limit < _size) {
    _size = limit;
  }
}

RocksDBExportCursor::~RocksDBExportCursor() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the cursor contains more data
////////////////////////////////////////////////////////////////////////////////

bool RocksDBExportCursor::hasNext() {
  if (_iter.get() == nullptr) {
    return false;
  }

  return (_position < _size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next element (not implemented)
////////////////////////////////////////////////////////////////////////////////

VPackSlice RocksDBExportCursor::next() {
  // should not be called directly
  return VPackSlice();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cursor size
////////////////////////////////////////////////////////////////////////////////

size_t RocksDBExportCursor::count() const { return _size; }

void RocksDBExportCursor::dump(VPackBuilder& builder) {
  auto transactionContext =
      std::make_shared<transaction::StandaloneContext>(_vocbaseGuard.vocbase());

  VPackOptions const* oldOptions = builder.options;

  builder.options = transactionContext->getVPackOptions();

  TRI_ASSERT(_iter.get() != nullptr);

  auto const restrictionType = _restrictions.type;

  try {
    builder.add("result", VPackValue(VPackValueType::Array));
    size_t const n = batchSize();

    auto cb = [&, this](DocumentIdentifierToken const& token, VPackSlice slice) {
      if (_position == _size) {
        return false;
      }
      builder.openObject();
      // Copy over shaped values
      for (auto const& entry : VPackObjectIterator(slice)) {
        std::string key(entry.key.copyString());

        if (!CollectionExport::IncludeAttribute(restrictionType,
                                                _restrictions.fields, key)) {
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
      _position++;
      return true;
    };

    _iter->nextDocument(cb, n);

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

    if (extra().isObject()) {
      builder.add("extra", extra());
    }

    if (!hasNext()) {
      // mark the cursor as deleted
      _iter.reset();
      this->deleted();
    }
  } catch (arangodb::basics::Exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "internal error during RocksDBExportCursor::dump");
  }
  builder.options = oldOptions;
}
