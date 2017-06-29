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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchView.h"

#include "IResearchKludge.h"
#include "IResearchLink.h"
#include "Indexes/Index.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "MMFiles/MMFilesDocumentPosition.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)
NS_BEGIN(kludge)

void insertDocument(
    arangodb::transaction::Methods& trx,
    arangodb::LogicalCollection& collection,
    arangodb::MMFilesDocumentPosition doc
) noexcept {
  try {
    for (auto& index: collection.getIndexes()) {
      try {
        if (!index
            || arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
          continue; // not an IResearchLink
        }

        // TODO FIXME find a better way to look up an iResearch View
        #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          auto* idx = dynamic_cast<IResearchLink*>(index.get());
        #else
          auto* idx = static_cast<IResearchLink*>(index.get());
        #endif

        auto fid = doc.fid();
        auto rid = doc.revisionId();
        VPackSlice document(reinterpret_cast<uint8_t const*>(doc.dataptr()));

        auto res = idx->insert(&trx, fid, rid, document);
        if (res.fail()) {
          LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failed to insert document '" << doc.revisionId() << "' from collection '" << collection.cid() << "' into IResearchLink '" << index->id() << "' via IResearchKludge, skipping";
          IR_EXCEPTION();
        }
      } catch(...) {
        LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while inserting document '" << doc.revisionId() << "' from collection '" << collection.cid() << "' into IResearchLink '" << index->id() << "' via IResearchKludge, skipping";
        IR_EXCEPTION();
      }
    }
  } catch(...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while inserting document '" << doc.revisionId() << "' from collection '" << collection.cid() << "' via IResearchKludge, skipping";
    IR_EXCEPTION();
  }
}

void persistFid(TRI_voc_fid_t fid) noexcept {
  try {
    arangodb::DatabaseFeature::DATABASE->enumerateDatabases(
      [fid](TRI_vocbase_t* vocbase)->void {
        if (!vocbase) {
          LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "vocbase not provided while persisting fid 'fid' via IResearchKludge, skipping";
          IR_EXCEPTION();
          return;
        }

        for (auto& logicalView: vocbase->views()) {
          if (!logicalView || !logicalView->getImplementation() || IResearchView::type() != logicalView->type()) {
            continue; // nit an IResearchView view
          }

          // TODO FIXME find a better way to look up an iResearch View
          #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
            auto* view = dynamic_cast<IResearchView*>(logicalView->getImplementation());
          #else
            auto* view = static_cast<IResearchView*>(logicalView->getImplementation());
          #endif

          if (TRI_ERROR_NO_ERROR != view->finish(fid)) {
            LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failed to persist fid '" << fid << "' in IResearchView '" << logicalView->name() << "' via IResearchKludge, skipping";
            IR_EXCEPTION();
          }
        }
    });
  } catch(...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while persisting fid '" << fid << "' via IResearchKludge";
    IR_EXCEPTION();
  }
}

void removeDocument(
    arangodb::transaction::Methods& trx,
    arangodb::LogicalCollection& collection,
    TRI_voc_rid_t rid
) noexcept {
  try {
    for (auto& index: collection.getIndexes()) {
      try {
        if (!index
            || arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
          continue; // not an IResearchLink
        }

        // TODO FIXME find a better way to look up an iResearch View
        #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          auto* idx = dynamic_cast<IResearchLink*>(index.get());
        #else
          auto* idx = static_cast<IResearchLink*>(index.get());
        #endif

        auto res = idx->remove(&trx, rid);
        if (res.fail()) {
          LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failed to remove document '" << rid << "' from collection '" << collection.cid() << "' from IResearchLink '" << index->id() << "' via IResearchKludge, skipping";
          IR_EXCEPTION();
        }
      } catch(...) {
        LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while removing document '" << rid << "' from collection '" << collection.cid() << "' from IResearchLink '" << index->id() << "' via IResearchKludge, skipping";
        IR_EXCEPTION();
      }
    }
  } catch(...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while removing document '" << rid << "' from collection '" << collection.cid() << "' via IResearchKludge, skipping";
    IR_EXCEPTION();
  }
}

NS_END // kludge
NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
