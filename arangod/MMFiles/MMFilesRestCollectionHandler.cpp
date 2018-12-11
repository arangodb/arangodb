////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesRestCollectionHandler.h"

#include "MMFiles/MMFilesCollection.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"

using namespace arangodb;

MMFilesRestCollectionHandler::MMFilesRestCollectionHandler(GeneralRequest* request,
                                             GeneralResponse* response)
    : RestCollectionHandler(request, response) {}

Result MMFilesRestCollectionHandler::handleExtraCommandPut(LogicalCollection& coll,
                                                           std::string const& command,
                                                           velocypack::Builder& builder) {
  
  if (command == "rotate") {
    auto ctx = transaction::StandaloneContext::Create(_vocbase);
    SingleCollectionTransaction trx(ctx, coll, AccessMode::Type::WRITE);
    
    Result res = trx.begin();
    
    if (res.ok()) {
    
      MMFilesCollection* mcoll = static_cast<MMFilesCollection*>(coll.getPhysical());
      
      try {
        res = mcoll->rotateActiveJournal();
      } catch (basics::Exception const& ex) {
        res.reset(ex.code(), ex.what());
      } catch (std::exception const& ex) {
        res.reset(TRI_ERROR_INTERNAL, ex.what());
      }
      
      res = trx.finish(res);
    }
    
    if (res.ok()) {
      builder.openObject();
      builder.add("result", VPackValue(true));
      builder.close();
    }
    return res;
  }
  
  return TRI_ERROR_NOT_IMPLEMENTED;
}
