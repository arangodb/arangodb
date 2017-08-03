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

#ifndef ARANGOD_AQL_DOCUMENT_PRODUCING_BLOCK_H
#define ARANGOD_AQL_DOCUMENT_PRODUCING_BLOCK_H 1

#include "Basics/Common.h"

#include <velocypack/Slice.h>

#include <functional>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {
class AqlItemBlock;
class DocumentProducingNode;
struct Variable;
  
class DocumentProducingBlock {
 public:
  typedef std::function<void(AqlItemBlock*, arangodb::velocypack::Slice, size_t, size_t&, size_t)> DocumentProducingFunction;

  DocumentProducingBlock(DocumentProducingNode const* node, transaction::Methods* trx);
  virtual ~DocumentProducingBlock() = default;

 public:
  bool produceResult() const { return _produceResult; }

 private:
  DocumentProducingFunction buildCallback() const;

 private:
  transaction::Methods* _trxPtr;
  
  DocumentProducingNode const* _node;

  /// @brief hether or not we want to build a result
  bool const _produceResult;

  /// @brief whether or not we are allowed to pass documents via raw pointers only
  bool const _useRawDocumentPointers;

 protected:  
  DocumentProducingFunction const _documentProducer;
};

}
}

#endif
