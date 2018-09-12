//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "IResearchCommon.h"
#include "IResearchDocument.h"
#include "IResearchFeature.h"
#include "IResearchFilterFactory.h"
#include "IResearchLink.h"
#include "IResearchLinkHelper.h"

#include "StorageEngine/TransactionState.h"
#include "StorageEngine/StorageEngine.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "velocypack/Iterator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalView.h"
#include "Logger/LogMacros.h"

#include "IResearchView.h"
#include "IResearchViewSingleServer.h"

namespace arangodb {
namespace iresearch {

/*static*/ std::shared_ptr<LogicalView> IResearchViewSingleServer::make(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    bool isNew,
    uint64_t planVersion,
    LogicalView::PreCommitCallback const& preCommit /*= {}*/
) {
  auto& properties = info.isObject() ? info : emptyObjectSlice(); // if no 'info' then assume defaults
  std::string error;

  bool hasLinks = properties.hasKey("links");

  if (hasLinks && isNew) {

    // check link auth as per https://github.com/arangodb/backlog/issues/459
    if (arangodb::ExecContext::CURRENT) {

      // check new links
      if (info.hasKey(StaticStrings::LinksField)) {
        for (arangodb::velocypack::ObjectIterator itr(info.get(StaticStrings::LinksField)); itr.valid(); ++itr) {
          if (!itr.key().isString()) {
            continue; // not a resolvable collection (invalid jSON)
          }

          auto collection= vocbase.lookupCollection(itr.key().copyString());

          if (collection
              && !arangodb::ExecContext::CURRENT->canUseCollection(vocbase.name(), collection->name(), arangodb::auth::Level::RO)) {
            return nullptr;
          }
        }
      }
    }
  }

  auto view = IResearchView::make(vocbase, info, isNew, planVersion, preCommit);

  // create links - "on a best-effort basis"
  if (properties.hasKey("links") && isNew) {

      std::unordered_set<TRI_voc_cid_t> collections;
      auto result = IResearchLinkHelper::updateLinks(
        collections, vocbase, *view.get(), properties.get("links")
      );

      if (result.fail()) {
        TRI_set_errno(result.errorNumber());
        LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
          << "Failure to construct links on new view in database '" << vocbase.id() << "', error: " << error;
      }
  }

  return view;
}

}
}


