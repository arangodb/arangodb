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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_MODIFICATION_EXECUTOR_TRAITS_H
#define ARANGOD_AQL_MODIFICATION_EXECUTOR_TRAITS_H

#include "Aql/ModificationExecutor.h"

#include "velocypack/Iterator.h"
#include "velocypack/velocypack-aliases.h"

namespace arangodb {
namespace aql {
enum class ModOperationType : uint8_t {
  IGNORE_SKIP = 0,    // do not apply, do not produce a result - used for
                      // skipping over suppressed errors
  IGNORE_RETURN = 1,  // do not apply, but pass the row to the next block -
                      // used for smart graphs and such
  APPLY_RETURN = 2,   // apply it and return the result, used for all
                      // non-UPSERT operations
  APPLY_UPDATE = 3,  // apply it and return the result, used only used for UPSERT
  APPLY_INSERT = 4,  // apply it and return the result, used only used for UPSERT
};

inline std::string toString(Insert&){ return "Insert"; };
inline std::string toString(Remove&){ return "Remove"; };
inline std::string toString(Update&){ return "Update"; };
inline std::string toString(Upsert&){ return "Upsert"; };
inline std::string toString(Replace&){ return "Replace"; };

struct ModificationBase {
  ModificationBase()
      : _operationResultIterator(VPackSlice::emptyArraySlice()),
        _last_not_skip(std::numeric_limits<decltype(_last_not_skip)>::max()) {}

  std::size_t _defaultBlockSize = ExecutionBlock::DefaultBatchSize();
  velocypack::Builder _tmpBuilder;  // default
  std::size_t _blockIndex = 0;  // cursor to the current positon
  SharedAqlItemBlockPtr _block = nullptr;

  OperationResult _operationResult;
  VPackSlice _operationResultArraySlice = VPackSlice::nullSlice();
  velocypack::ArrayIterator _operationResultIterator;  // ctor init list

  std::vector<ModOperationType> _operations;
  std::size_t _last_not_skip;
  bool _justCopy = false;

  void reset() {
    // MUST not reset _block
    _justCopy = false;
    _last_not_skip = std::numeric_limits<decltype(_last_not_skip)>::max();
    _blockIndex = 0;

    _tmpBuilder.clear();

    _operationResult = OperationResult();
    _operationResultArraySlice = velocypack::Slice::emptyArraySlice();
    _operationResultIterator = VPackArrayIterator(VPackSlice::emptyArraySlice());

    _operations.clear();
    TRI_ASSERT(_block != nullptr);
    _operations.reserve(_block->size());
  }

  static void setOperationResult(OperationResult&& result, OperationResult& target,
                                 VPackSlice& slice, VPackArrayIterator& iter) {
    target = std::move(result);
    if (target.buffer && target.slice().isArray()) {
      slice = target.slice();
      iter = VPackArrayIterator(slice);
    }
  }

  void setOperationResult(OperationResult&& result) {
    setOperationResult(std::move(result), _operationResult,
                       _operationResultArraySlice, _operationResultIterator);
  }
};

struct Insert : ModificationBase {
  bool doModifications(ModificationExecutorInfos&, ModificationStats&);
  bool doOutput(ModificationExecutorInfos&, OutputAqlItemRow&);
};

struct Remove : ModificationBase {
  bool doModifications(ModificationExecutorInfos&, ModificationStats&);
  bool doOutput(ModificationExecutorInfos&, OutputAqlItemRow&);
};

struct Upsert : ModificationBase {
  bool doModifications(ModificationExecutorInfos&, ModificationStats&);
  bool doOutput(ModificationExecutorInfos&, OutputAqlItemRow&);

  OperationResult _operationResultUpdate;
  VPackSlice _operationResultArraySliceUpdate = VPackSlice::nullSlice();
  velocypack::ArrayIterator _operationResultUpdateIterator;  // ctor init list

  Upsert() : _operationResultUpdateIterator(VPackSlice::emptyArraySlice()) {
    _defaultBlockSize = 1;
  }

  VPackBuilder _updateBuilder;
  VPackBuilder _insertBuilder;

  void reset() {
    ModificationBase::reset();
    _updateBuilder.clear();
    _insertBuilder.clear();

    _operationResultUpdate = OperationResult();
    _operationResultArraySliceUpdate = velocypack::Slice::emptyArraySlice();
    _operationResultUpdateIterator = VPackArrayIterator(VPackSlice::emptyArraySlice());
  };

  void setOperationResultUpdate(OperationResult&& result) {
    setOperationResult(std::move(result), _operationResultUpdate,
                       _operationResultArraySliceUpdate, _operationResultUpdateIterator);
  }
};

template <typename ModType>
struct UpdateReplace : ModificationBase {
  using ModificationType = ModType;
  using MethodPtr = OperationResult (transaction::Methods::*)(std::string const& collectionName,
                                                              VPackSlice const updateValue,
                                                              OperationOptions const& options);
  bool doModifications(ModificationExecutorInfos&, ModificationStats&);
  bool doOutput(ModificationExecutorInfos&, OutputAqlItemRow&);

  UpdateReplace() = delete;
  UpdateReplace(MethodPtr method, std::string name)
      : _method(method), _name(std::move(name)) {}

  void reset() {
    ModificationBase::reset();
    _updateOrReplaceBuilder.clear();
  };

  MethodPtr _method;
  std::string _name;
  VPackBuilder _updateOrReplaceBuilder;
};

struct Update : UpdateReplace<Update> {
  Update() : UpdateReplace(&transaction::Methods::update, "UPDATE") {}
};
struct Replace : UpdateReplace<Replace> {
  Replace() : UpdateReplace(&transaction::Methods::replace, "REPLACE") {}
};

}  // namespace aql
}  // namespace arangodb

#endif
