////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "ResultWriter.h"
#include "GraphStore.h"

#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/Transaction.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;

template <typename V, typename E>
void ResultWriter<V,E>::writeResults(TRI_vocbase_t *vocbase, GraphStore<V,E> *store) {
    SingleCollectionTransaction
    trx(StandaloneTransactionContext::Create(vocbase),
        _vertexCollection, TRI_TRANSACTION_WRITE);
    int res = trx.begin();
    
    if (res != TRI_ERROR_NO_ERROR) {
        LOG(ERR) << "cannot start transaction to load authentication";
        return;
    }*/
    /*
     OperationResult result;
     OperationOptions options;
     options.waitForSync = false;
     options.mergeObjects = true;
     for (auto const &pair : _vertices) {
     //TransactionBuilderLeaser b(&trx);
     VPackBuilder b;
     b.openObject();
     b.add(StaticStrings::KeyString,
     pair.second->_data.get(StaticStrings::KeyString));
     b.add("value", VPackValue(pair.second->_vertexState));
     b.close();
     LOG(INFO) << b.toJson();
     result = trx.update(_vertexCollection, b->slice(), options);
     if (!result.successful()) {
     THROW_ARANGO_EXCEPTION_FORMAT(result.code, "while looking up graph
     '%s'",
     _vertexCollection.c_str());
     }
     }*/
    // Commit or abort.
    /*res = trx.finish(result.code);
     
     if (res != TRI_ERROR_NO_ERROR) {
     THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up graph '%s'",
     _vertexCollection.c_str());
     }
}
