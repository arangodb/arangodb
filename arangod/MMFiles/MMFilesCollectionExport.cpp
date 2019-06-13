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

#include "MMFilesCollectionExport.h"
#include "Basics/WriteLocker.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesDitch.h"
#include "MMFiles/MMFilesEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/ExecContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

MMFilesCollectionExport::MMFilesCollectionExport(TRI_vocbase_t& vocbase,
                                                 std::string const& name,
                                                 CollectionExport::Restrictions const& restrictions)
    : _collection(nullptr),
      _ditch(nullptr),
      _name(name),
      _resolver(vocbase),
      _restrictions(restrictions) {
  // prevent the collection from being unloaded while the export is ongoing
  // this may throw
  _guard.reset(new arangodb::CollectionGuard(&vocbase, _name));

  _collection = _guard->collection();
  TRI_ASSERT(_collection != nullptr);
}

MMFilesCollectionExport::~MMFilesCollectionExport() {
  if (_ditch != nullptr) {
    _ditch->ditches()->freeMMFilesDocumentDitch(_ditch, false);
  }
}

void MMFilesCollectionExport::run(uint64_t maxWaitTime, size_t limit) {
  MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);

  // try to acquire the exclusive lock on the compaction
  engine->preventCompaction(&(_collection->vocbase()), [this](TRI_vocbase_t* vocbase) {
    // create a ditch under the compaction lock
    _ditch = arangodb::MMFilesCollection::toMMFilesCollection(_collection)
                 ->ditches()
                 ->createMMFilesDocumentDitch(false, __FILE__, __LINE__);
  });

  // now we either have a ditch or not
  if (_ditch == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  {
    static uint64_t const SleepTime = 10000;
    uint64_t tries = 0;
    uint64_t const maxTries = maxWaitTime / SleepTime;

    MMFilesCollection* mmColl = MMFilesCollection::toMMFilesCollection(_collection);

    while (++tries < maxTries) {
      if (mmColl->isFullyCollected()) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(SleepTime));
    }
  }

  auto guard = scopeGuard([this]() {
    // delete guard right now as we're about to return
    // if we would continue holding the guard's collection lock and return,
    // and the export object gets later freed in a different thread, then all
    // would be lost. so we'll release the lock here and rely on the cleanup
    // thread not unloading the collection (as we've acquired a document ditch
    // for the collection already - this will prevent unloading of the
    // collection's datafiles etc.)
    _guard.reset();
  });

  auto ctx = transaction::StandaloneContext::Create(_collection->vocbase());
  SingleCollectionTransaction trx(ctx, _name, AccessMode::Type::READ);

  // already locked by _guard instance variable
  trx.addHint(transaction::Hints::Hint::NO_USAGE_LOCK);

  Result res = trx.begin();

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  size_t maxDocuments =
      _collection->numberDocuments(&trx, transaction::CountType::Normal);

  if (limit > 0 && limit < maxDocuments) {
    maxDocuments = limit;
  } else {
    limit = maxDocuments;
  }

  _vpack.reserve(maxDocuments);

  MMFilesCollection* mmColl = MMFilesCollection::toMMFilesCollection(_collection);
  ManagedDocumentResult mmdr;
  trx.invokeOnAllElements(_collection->name(), [this, &limit, &trx, &mmdr,
                                                mmColl](LocalDocumentId const& token) {
    if (limit == 0) {
      return false;
    }
    if (mmColl->readDocumentConditional(&trx, token, 0, mmdr)) {
      _vpack.emplace_back(mmdr.vpack());
      --limit;
    }
    return true;
  });

  trx.finish(res);
}
