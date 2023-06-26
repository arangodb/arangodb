////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "StatusWriter.h"

#include "Inspection/Format.h"
#include "Inspection/Types.h"
#include "Transaction/Hints.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "Utils/DatabaseGuard.h"
#include "Pregel/ExecutionNumber.h"

namespace arangodb {

namespace pregel {
struct ExecutionNumber;
}

namespace transaction {
class Context;
}
class SingleCollectionTransaction;

}  // namespace arangodb

namespace arangodb::pregel::statuswriter {

struct CollectionStatusWriter : StatusWriterInterface {
  CollectionStatusWriter(TRI_vocbase_t& vocbase,
                         ExecutionNumber& executionNumber,
                         transaction::Hints::Hint const& trxTypeHint);
  CollectionStatusWriter(TRI_vocbase_t& vocbase,
                         transaction::Hints::Hint const& trxTypeHint);

  [[nodiscard]] auto createResult(VPackSlice data) -> OperationResult override;
  [[nodiscard]] auto readResult() -> OperationResult override;
  [[nodiscard]] auto readAllNonExpiredResults() -> OperationResult override;
  [[nodiscard]] auto readAllResults() -> OperationResult override;
  [[nodiscard]] auto updateResult(VPackSlice data) -> OperationResult override;
  [[nodiscard]] auto deleteResult() -> OperationResult override;
  [[nodiscard]] auto deleteAllResults() -> OperationResult override;

 public:
  struct OperationData {
    OperationData(uint64_t executionNumber)
        : _key(std::to_string(executionNumber)), data(std::nullopt) {}
    OperationData(uint64_t executionNumber, std::optional<VPackSlice> data)
        : _key(std::to_string(executionNumber)), data(data) {}

    std::string _key;
    std::optional<VPackSlice> data;
  };

 private:
  [[nodiscard]] auto executeQuery(
      std::string queryString,
      std::optional<std::shared_ptr<VPackBuilder>> bindParameters)
      -> OperationResult;
  [[nodiscard]] auto handleOperationResult(SingleCollectionTransaction& trx,
                                           OperationOptions& options,
                                           Result& transactionResult,
                                           OperationResult&& opRes) const
      -> OperationResult;
  [[nodiscard]] auto ctx() -> std::shared_ptr<transaction::Context> const;

 private:
  DatabaseGuard _vocbaseGuard;
  ExecutionNumber _executionNumber;
  transaction::Hints::Hint _trxTypeHint;
  std::optional<std::string> _user;
  std::shared_ptr<LogicalCollection> _logicalCollection;
};

template<typename Inspector>
auto inspect(Inspector& f, CollectionStatusWriter::OperationData& x) {
  return f.object(x).fields(f.field("_key", x._key), f.field("data", x.data));
}

}  // namespace arangodb::pregel::statuswriter
