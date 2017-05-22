////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBCollectionExport.h"
#include "Basics/WriteLocker.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

RocksDBCollectionExport::RocksDBCollectionExport(
    TRI_vocbase_t* vocbase, std::string const& name,
    CollectionExport::Restrictions const& restrictions)
    : _collection(nullptr),
      _name(name),
      _resolver(vocbase),
      _restrictions(restrictions) {
  // prevent the collection from being unloaded while the export is ongoing
  // this may throw
  _guard.reset(new arangodb::CollectionGuard(vocbase, _name.c_str(), false));

  _collection = _guard->collection();
  TRI_ASSERT(_collection != nullptr);
}

RocksDBCollectionExport::~RocksDBCollectionExport() {}

void RocksDBCollectionExport::run(size_t limit) {
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(_collection->vocbase()), _name,
      AccessMode::Type::READ);

  // already locked by guard above
  trx.addHint(transaction::Hints::Hint::NO_USAGE_LOCK);
  Result res = trx.begin();

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  _vpack.reserve(limit);

  ManagedDocumentResult mmdr;
  trx.invokeOnAllElements(
      _collection->name(),
      [this, &limit, &trx, &mmdr](DocumentIdentifierToken const& token) {
        if (limit == 0) {
          return false;
        }
        if (_collection->readDocument(&trx, token, mmdr)) {
          _vpack.emplace_back(VPackSlice(mmdr.vpack()));
          --limit;
        }
        return true;
      });

  trx.finish(res.errorNumber());

  // delete guard right now as we're about to return
  // if we would continue holding the guard's collection lock and return,
  // and the export object gets later freed in a different thread, then all
  // would be lost. so we'll release the lock here and rely on the cleanup
  // thread not unloading the collection (as we've acquired a document ditch
  // for the collection already - this will prevent unloading of the
  // collection's
  // datafiles etc.)
  _guard.reset();
}
