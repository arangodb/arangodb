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

#include "Basics/StaticStrings.h"
#include "Inspection/Format.h"
#include "Inspection/Types.h"
#include "Transaction/Hints.h"
#include "Transaction/V8Context.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/AccessMode.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace pregel {
struct ExecutionNumber;
}
}  // namespace arangodb

namespace arangodb::pregel::statuswriter {

struct CollectionStatusWriter : StatusWriterInterface {
  CollectionStatusWriter(TRI_vocbase_t& vocbase,
                         ExecutionNumber& executionNumber)
      : _vocbaseGuard(vocbase), _executionNumber(executionNumber) {
    CollectionNameResolver resolver(_vocbaseGuard.database());
    auto logicalCollection =
        resolver.getCollection(StaticStrings::PregelCollection);
    if (logicalCollection == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                     StaticStrings::PregelCollection);
    }
    _logicalCollection = std::move(logicalCollection);
  };

  enum class OperationType {
    CREATE_DOCUMENT,
    READ_DOCUMENT,
    UPDATE_DOCUMENT,
    DELETE_DOCUMENT
  };

  [[nodiscard]] auto createResult(VPackSlice data) -> OperationResult override {
    OperationData opData(_executionNumber.value, data);
    return createOperation(OperationType::CREATE_DOCUMENT, opData);
  }
  [[nodiscard]] auto readResult(VPackSlice data) -> OperationResult override {
    OperationData opData(_executionNumber.value);
    return createOperation(OperationType::READ_DOCUMENT, opData);
  }
  [[nodiscard]] auto updateResult(VPackSlice data) -> OperationResult override {
    OperationData opData(_executionNumber.value, data);
    return createOperation(OperationType::UPDATE_DOCUMENT, opData);
  }
  [[nodiscard]] auto deleteResult(VPackSlice data) -> OperationResult override {
    OperationData opData(_executionNumber.value);
    return createOperation(OperationType::DELETE_DOCUMENT, opData);
  }

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
  [[nodiscard]] auto createOperation(OperationType operation,
                                     OperationData data) -> OperationResult {
    // Helper method: finishes transaction & handle potential errors
    auto handleOperationResult =
        [](SingleCollectionTransaction& trx, OperationOptions& options,
           Result& transactionResult,
           OperationResult&& opRes) -> OperationResult {
      transactionResult = trx.finish(opRes.result);
      if (transactionResult.fail() && opRes.ok()) {
        return OperationResult{std::move(transactionResult), options};
      }
      return opRes;
    };

    AccessMode::Type accessModeType;
    if (operation == OperationType::CREATE_DOCUMENT ||
        operation == OperationType::UPDATE_DOCUMENT ||
        operation == OperationType::DELETE_DOCUMENT) {
      accessModeType = AccessMode::Type::WRITE;
    } else {
      TRI_ASSERT(operation == OperationType::READ_DOCUMENT);
      accessModeType = AccessMode::Type::READ;
    }

    // transaction options
    SingleCollectionTransaction trx(ctx(), StaticStrings::PregelCollection,
                                    accessModeType);
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
    OperationOptions options(ExecContext::current());

    // begin transaction
    Result transactionResult = trx.begin();
    if (transactionResult.fail()) {
      return OperationResult{std::move(transactionResult), options};
    }

    // execute transaction
    auto payload = inspection::serializeWithErrorT(data);
    switch (accessModeType) {
      case AccessMode::Type::WRITE: {
        if (operation == OperationType::UPDATE_DOCUMENT) {
          return handleOperationResult(
              trx, options, transactionResult,
              trx.update(StaticStrings::PregelCollection, payload->slice(),
                         {}));
        } else if (operation == OperationType::CREATE_DOCUMENT) {
          return handleOperationResult(
              trx, options, transactionResult,
              trx.insert(StaticStrings::PregelCollection, payload->slice(),
                         {}));
        } else {
          TRI_ASSERT(operation == OperationType::DELETE_DOCUMENT);
          return handleOperationResult(
              trx, options, transactionResult,
              trx.remove(StaticStrings::PregelCollection, payload->slice(),
                         {}));
        }
      }
      case AccessMode::Type::READ: {
        // Note: documentAsync can throw.
        return handleOperationResult(
            trx, options, transactionResult,
            trx.documentAsync(StaticStrings::PregelCollection, payload->slice(),
                              {})
                .get());
      }
      default: {
        TRI_ASSERT(false);
      }
    }

    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  [[nodiscard]] auto ctx() -> std::shared_ptr<transaction::Context> const {
    return transaction::V8Context::CreateWhenRequired(_vocbaseGuard.database(),
                                                      false);
  }

 private:
  DatabaseGuard _vocbaseGuard;
  ExecutionNumber _executionNumber;
  std::shared_ptr<LogicalCollection> _logicalCollection;
};

// Note: Put inspect methods into dedicated own header file.
// This will speed up compilation times. Only include where we actually
// need to serialize.
template<typename Inspector>
auto inspect(Inspector& f, CollectionStatusWriter::OperationData& x) {
  return f.object(x).fields(f.field("_key", x._key), f.field("data", x.data));
}

}  // namespace arangodb::pregel::statuswriter
