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

#ifndef ARANGOD_UTILS_SINGLE_COLLECTION_READ_ONLY_TRANSACTION_H
#define ARANGOD_UTILS_SINGLE_COLLECTION_READ_ONLY_TRANSACTION_H 1

#include "Basics/Common.h"

#include "Utils/CollectionNameResolver.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/transaction.h"

struct TRI_vocbase_t;

namespace arangodb {

class SingleCollectionReadOnlyTransaction : public SingleCollectionTransaction {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the transaction, using a collection object
  ///
  /// A self-contained read transaction is a transaction on a single collection
  /// that only allows read operations. Write operations are not supported.
  //////////////////////////////////////////////////////////////////////////////

  SingleCollectionReadOnlyTransaction(TransactionContext* transactionContext,
                                      TRI_vocbase_t* vocbase, TRI_voc_cid_t cid)
      : SingleCollectionTransaction(transactionContext, vocbase, cid,
                                    TRI_TRANSACTION_READ) {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief same as above, but create using collection name
  //////////////////////////////////////////////////////////////////////////////

  SingleCollectionReadOnlyTransaction(TransactionContext* transactionContext,
                                      TRI_vocbase_t* vocbase,
                                      std::string const& name)
      : SingleCollectionTransaction(transactionContext, vocbase, name,
                                    TRI_TRANSACTION_READ) {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief end the transaction
  //////////////////////////////////////////////////////////////////////////////

  ~SingleCollectionReadOnlyTransaction() {}
};
}

#endif
