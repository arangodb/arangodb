////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "DistributeExecutor.h"

#include "Aql/AqlCallStack.h"
#include "Aql/ClusterNodes.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/IdExecutor.h"
#include "Aql/RegisterPlan.h"
#include "Aql/ShadowAqlItemRow.h"
#include "Aql/SkipResult.h"
#include "Basics/StaticStrings.h"
#include "Transaction/Helpers.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

DistributeExecutorInfos::DistributeExecutorInfos(
    std::vector<std::string> clientIds, Collection const* collection,
    RegisterId regId, RegisterId alternativeRegId, bool allowSpecifiedKeys,
    bool allowKeyConversionToObject, bool createKeys, bool fixupGraphInput,
     ScatterNode::ScatterType type)
    : ClientsExecutorInfos(std::move(clientIds)),
      _regId(regId),
      _alternativeRegId(alternativeRegId),
      _allowKeyConversionToObject(allowKeyConversionToObject),
      _createKeys(createKeys),
      _usesDefaultSharding(collection->usesDefaultSharding()),
      _allowSpecifiedKeys(allowSpecifiedKeys),
      _fixupGraphInput(fixupGraphInput),
      _collection(collection),
      _logCol(collection->getCollection()),
      _type(type) {}

auto DistributeExecutorInfos::registerId() const noexcept -> RegisterId {
  TRI_ASSERT(_regId != RegisterPlan::MaxRegisterId);
  return _regId;
}
auto DistributeExecutorInfos::hasAlternativeRegister() const noexcept -> bool {
  return _alternativeRegId != RegisterPlan::MaxRegisterId;
}
auto DistributeExecutorInfos::alternativeRegisterId() const noexcept -> RegisterId {
  TRI_ASSERT(_alternativeRegId != RegisterPlan::MaxRegisterId);
  return _alternativeRegId;
}

auto DistributeExecutorInfos::allowKeyConversionToObject() const noexcept -> bool {
  return _allowKeyConversionToObject;
}

auto DistributeExecutorInfos::createKeys() const noexcept -> bool {
  return _createKeys;
}
auto DistributeExecutorInfos::usesDefaultSharding() const noexcept -> bool {
  return _usesDefaultSharding;
}
auto DistributeExecutorInfos::allowSpecifiedKeys() const noexcept -> bool {
  return _allowSpecifiedKeys;
}

auto DistributeExecutorInfos::needsToFixGraphInput() const -> bool {
  return _fixupGraphInput;
}

auto DistributeExecutorInfos::scatterType() const noexcept -> ScatterNode::ScatterType {
  return _type;
}

auto DistributeExecutorInfos::getResponsibleClient(arangodb::velocypack::Slice value) const
    -> ResultT<std::string> {
  std::string shardId;
  int res = _logCol->getResponsibleShard(value, true, shardId);

  if (res != TRI_ERROR_NO_ERROR) {
    return Result{res};
  }

  TRI_ASSERT(!shardId.empty());
  if (_type == ScatterNode::ScatterType::SERVER) {
    // Special case for server based distribution.
    shardId = _collection->getServerForShard(shardId);
    TRI_ASSERT(!shardId.empty());
  }
  return shardId;
}

/// @brief create a new document key
auto DistributeExecutorInfos::createKey(VPackSlice input) const -> std::string {
  return _logCol->createKey(input);
}

DistributeExecutor::DistributeExecutor(DistributeExecutorInfos const& infos)
    : _infos(infos) {}

auto DistributeExecutor::distributeBlock(SharedAqlItemBlockPtr block, SkipResult skipped,
                                         std::unordered_map<std::string, ClientBlockData>& blockMap)
    -> void {
  std::unordered_map<std::string, std::vector<std::size_t>> choosenMap;
  choosenMap.reserve(blockMap.size());

  if (block != nullptr) {
    for (size_t i = 0; i < block->numRows(); ++i) {
      if (block->isShadowRow(i)) {
        // ShadowRows need to be added to all Clients
        for (auto const& [key, value] : blockMap) {
          choosenMap[key].emplace_back(i);
        }
      } else {
        auto client = getClient(block, i);
        // We can only have clients we are prepared for
        TRI_ASSERT(blockMap.find(client) != blockMap.end());
        choosenMap[client].emplace_back(i);
      }
    }

    // We cannot have more in choosen than we have blocks
    TRI_ASSERT(choosenMap.size() <= blockMap.size());
    for (auto& [key, target] : blockMap) {
      auto value = choosenMap.find(key);
      if (value != choosenMap.end()) {
        // we have data in this block
        target.addBlock(skipped, block, std::move(value->second));
      } else {
        // No data, add skipped
        target.addBlock(skipped, nullptr, {});
      }
    }
  } else {
    for (auto& [key, target] : blockMap) {
      // No data, add skipped
      target.addBlock(skipped, nullptr, {});
    }
  }
}

auto DistributeExecutor::getClientByIdSlice(VPackSlice input) -> std::string {
  // Need to fix this document.
  // We need id and key as input.
  auto keyPart = transaction::helpers::extractKeyPart(input);
  auto res = _infos.getResponsibleClient(buildKeyObject(VPackStringRef(keyPart)));
  // If _key is invalid, we will throw here.
  THROW_ARANGO_EXCEPTION_IF_FAIL(res.result());
  return res.get();
}

auto DistributeExecutor::getClient(SharedAqlItemBlockPtr block, size_t rowIndex)
    -> std::string {
  InputAqlItemRow row{block, rowIndex};
   
  // check first input register
  AqlValue val = row.getValue(_infos.registerId());
  bool usedAlternativeRegId = false;

  // if empty, check alternative input register
  if (_infos.hasAlternativeRegister() && val.isNull(true)) {
    // value is set, but null
    // check if there is a second input register available (UPSERT makes use of
    // two input registers,
    // one for the search document, the other for the insert document)
    val = row.getValue(_infos.alternativeRegisterId());
    usedAlternativeRegId = true;
  }

  VPackSlice input = val.slice();  // will throw when wrong type

  if (_infos.needsToFixGraphInput()) {
    if (input.isString()) {
      return getClientByIdSlice(input);
    }

    // check input value
    VPackSlice idSlice;
    bool valid = input.isObject();
    if (valid) {
      idSlice = input.get(StaticStrings::IdString);
      // no _id, no cookies
      valid = !idSlice.isNone();
    }

    if (!valid) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                     "invalid start vertex. Must either be "
                                     "an _id string value or an object with _id. "
                                     "Instead got: " + input.toJson());
    }

    if (!input.hasKey(StaticStrings::KeyString)) {
      // The input is an object, but only contains an _id, not a _key value that could be extracted.
      // We can work with _id value only however so let us do this.
      return getClientByIdSlice(idSlice);
    }

    // If the value is valid (like a document) it will simply work.
    auto res = _infos.getResponsibleClient(input);
    THROW_ARANGO_EXCEPTION_IF_FAIL(res.result());
    return res.get();
  }
  
  TRI_ASSERT(!_infos.needsToFixGraphInput());
  
  auto canUseCustomKey = [this](bool usedAlternativeRegId) {
    if (_infos.usesDefaultSharding()) {
      return true;
    }
    // the collection is not sharded by _key
    return (!usedAlternativeRegId && _infos.allowSpecifiedKeys());
  };

  bool buildNewObject = false;
    
  if (!input.isObject()) {
    // input is not an object.
    // if this happens, it must be a string key
    if (!input.isString() || !_infos.allowKeyConversionToObject()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    }
       
    // check if custom keys are allowed in this context
    if (!canUseCustomKey(usedAlternativeRegId)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY);
    }
    
    // convert string key into object with { _key: "string" }
    TRI_ASSERT(_infos.allowKeyConversionToObject());
    input = buildKeyObject(input.stringRef());
    buildNewObject = true;
  } 

  if (_infos.createKeys()) {
    TRI_ASSERT(input.isObject());

    buildNewObject = !input.hasKey(StaticStrings::KeyString);
    // we are responsible for creating keys if none present

    if (!buildNewObject && !canUseCustomKey(usedAlternativeRegId)) {
      // the collection is not sharded by _key, but there is a _key present in the data.
      // a _key was given, but user is not allowed to specify _key
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY);
    }

    if (buildNewObject) {
      VPackSlice newKeyObject = buildKeyObject(VPackStringRef(_infos.createKey(input)));

      _objectBuilder.clear();
      VPackCollection::merge(_objectBuilder, input, newKeyObject, true);
      input = _objectBuilder.slice();
    }
  }

  if (buildNewObject) {
    TRI_ASSERT(_infos.createKeys() || _infos.allowKeyConversionToObject());
    // clear the previous value and overwrite with new value, in place!
    auto reg = usedAlternativeRegId ? _infos.alternativeRegisterId()
                                    : _infos.registerId();
    block->destroyValue(rowIndex, reg);
    block->emplaceValue(rowIndex, reg, input);
  }

  auto res = _infos.getResponsibleClient(input);
  THROW_ARANGO_EXCEPTION_IF_FAIL(res.result());
  return res.get();
}
  
// build a temporary object with just a "_key" attribute. the object can
// be pointed to afterwards until _keyBuilder gets modified again
auto DistributeExecutor::buildKeyObject(VPackStringRef key) -> VPackSlice {
  _keyBuilder.clear();
  _keyBuilder.openObject(true);
  _keyBuilder.add(StaticStrings::KeyString, VPackValuePair(key.data(), key.size(), VPackValueType::String));
  _keyBuilder.close();
  return _keyBuilder.slice();
}

ExecutionBlockImpl<DistributeExecutor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                           DistributeNode const* node,
                                                           RegisterInfos registerInfos,
                                                           DistributeExecutorInfos&& executorInfos)
    : BlocksWithClientsImpl(engine, node, std::move(registerInfos),
                            std::move(executorInfos)) {}
