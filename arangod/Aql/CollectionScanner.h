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

#ifndef ARANGOD_AQL_COLLECTION_SCANNER_H
#define ARANGOD_AQL_COLLECTION_SCANNER_H 1

#include "Basics/Common.h"
#include "Utils/AqlTransaction.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace aql {

struct CollectionScanner {
  CollectionScanner(arangodb::AqlTransaction*, TRI_transaction_collection_t*);

  virtual ~CollectionScanner();

  virtual int scan(std::vector<TRI_doc_mptr_copy_t>&, size_t) = 0;

  virtual void reset() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief forwards the cursor n elements. Does not read the data.
  ///        Will at most forward to the last element.
  ///        In the second parameter we add how many elements are
  ///        really skipped
  //////////////////////////////////////////////////////////////////////////////

  virtual int forward(size_t, size_t&) = 0;

  arangodb::AqlTransaction* trx;
  TRI_transaction_collection_t* trxCollection;
  uint64_t totalCount;
  arangodb::basics::BucketPosition position;
};

struct RandomCollectionScanner final : public CollectionScanner {
  RandomCollectionScanner(arangodb::AqlTransaction*,
                          TRI_transaction_collection_t*);

  int scan(std::vector<TRI_doc_mptr_copy_t>&, size_t) override;

  void reset() override;

  int forward(size_t, size_t&) override;

  arangodb::basics::BucketPosition initialPosition;
  uint64_t step;
};

struct LinearCollectionScanner final : public CollectionScanner {
  LinearCollectionScanner(arangodb::AqlTransaction*,
                          TRI_transaction_collection_t*);

  int scan(std::vector<TRI_doc_mptr_copy_t>&, size_t) override;

  void reset() override;

  int forward(size_t, size_t&) override;
};
}
}

#endif
