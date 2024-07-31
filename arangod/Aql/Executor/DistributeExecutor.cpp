////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/DistributeNode.h"
#include "Aql/Executor/IdExecutor.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SkipResult.h"
#include "Basics/StaticStrings.h"
#include "Containers/FlatHashMap.h"
#include "VocBase/LogicalCollection.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Slice.h>

namespace arangodb::aql {

DistributeExecutorInfos::DistributeExecutorInfos(
    std::vector<std::string> clientIds, Collection const* collection,
    RegisterId regId, std::vector<std::string> attribute,
    ScatterNode::ScatterType type, std::vector<aql::Collection*> satellites)
    : ClientsExecutorInfos(std::move(clientIds)),
      _regId(regId),
      _type(type),
      _attribute(std::move(attribute)),
      _collection(collection),
      _logCol(collection->getCollection()),
      _satellites(std::move(satellites)) {}

auto DistributeExecutorInfos::registerId() const noexcept -> RegisterId {
  TRI_ASSERT(_regId.isValid());
  return _regId;
}

auto DistributeExecutorInfos::attribute() const noexcept
    -> std::vector<std::string> const& {
  return _attribute;
}

auto DistributeExecutorInfos::scatterType() const noexcept
    -> ScatterNode::ScatterType {
  return _type;
}

auto DistributeExecutorInfos::getResponsibleClient(
    arangodb::velocypack::Slice value) const -> ResultT<std::string> {
  auto maybeShard = _logCol->getResponsibleShard(value, true);
  if (maybeShard.fail()) {
    return maybeShard.result();
  }

  if (_type == ScatterNode::ScatterType::SERVER) {
    // Special case for server based distribution.
    auto server = _collection->getServerForShard(maybeShard.get());
    TRI_ASSERT(!server.empty());
    return server;
  }
  return maybeShard.get();
}

auto DistributeExecutorInfos::shouldDistributeToAll(
    arangodb::velocypack::Slice value) const -> bool {
  if (_satellites.empty()) {
    // We can only distribute to all on Satellite Collections
    return false;
  }
  velocypack::Slice id;
  if (_attribute.empty()) {
    TRI_ASSERT(value.isObject());
    id = value.get(StaticStrings::IdString);
  } else if (_attribute.size() == 1 &&
             _attribute[0] == StaticStrings::IdString) {
    id = value;
  } else {
    id = VPackSlice::nullSlice();
  }
  if (!id.isString()) {
    // We can only distribute to all if we can detect the collection name
    return false;
  }

  // NOTE: Copy Paste code, shall be unified
  std::string_view vid = id.stringView();
  size_t pos = vid.find('/');
  if (pos == vid.npos) {
    // Invalid input. Let the sharding take care of it, one server shall
    // complain
    return false;
  }
  vid = vid.substr(0, pos);
  for (auto const& it : _satellites) {
    if (vid == it->name()) {
      // This vertex is from a satellite collection start everywhere!
      return true;
    }
  }
  return false;
}

DistributeExecutor::DistributeExecutor(DistributeExecutorInfos const& infos)
    : _infos(infos) {}

auto DistributeExecutor::distributeBlock(
    SharedAqlItemBlockPtr const& block, SkipResult skipped,
    std::unordered_map<std::string, ClientBlockData>& blockMap) -> void {
  if (block != nullptr) {
    containers::FlatHashMap<std::string, std::vector<std::size_t>> chosenMap;
    chosenMap.reserve(blockMap.size());

    for (size_t i = 0; i < block->numRows(); ++i) {
      if (block->isShadowRow(i)) {
        // ShadowRows need to be added to all Clients
        for (auto const& [key, value] : blockMap) {
          chosenMap[key].emplace_back(i);
        }
      } else {
        // check first input register
        AqlValue val = block->getValue(i, _infos.registerId());

        VPackSlice input = val.slice();  // will throw when wrong type
        if (input.isNone()) {
          continue;
        }
        if (!input.isObject() && _infos.attribute().empty()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
              "DistributeExecutor requires an object as input");
        }
        // NONE is ignored.
        // Object is processed
        // All others throw.
        TRI_ASSERT(input.isObject() || !_infos.attribute().empty());
        if (_infos.shouldDistributeToAll(input)) {
          // This input should be added to all clients
          for (auto const& [key, value] : blockMap) {
            chosenMap[key].emplace_back(i);
          }
        } else {
          auto client = getClient(input);
          if (!client.empty()) {
            // We can only have clients we are prepared for
            TRI_ASSERT(blockMap.contains(client));
            if (ADB_UNLIKELY(!blockMap.contains(client))) {
              THROW_ARANGO_EXCEPTION_MESSAGE(
                  TRI_ERROR_INTERNAL,
                  absl::StrCat("unexpected client id '", client,
                               "' found in blockMap"));
            }
            chosenMap[client].emplace_back(i);
          }
        }
      }
    }

    // We cannot have more in choosen than we have blocks
    TRI_ASSERT(chosenMap.size() <= blockMap.size());
    for (auto& [key, target] : blockMap) {
      auto value = chosenMap.find(key);
      if (value != chosenMap.end()) {
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

auto DistributeExecutor::getClient(VPackSlice input) const -> std::string {
  auto const& a = _infos.attribute();
  if (!a.empty()) {
    // create {"a":{"b":{"c":<input>}}} from <input> and <attribute> vector
    _temp.clear();
    _temp.openObject(/*unindexed*/ true);
    for (size_t i = 0; i < a.size(); ++i) {
      _temp.add(VPackValue(a[i]));
      if (i < a.size() - 1) {
        _temp.openObject(/*unindexed*/ true);
      }
    }
    _temp.add(input);
    for (size_t i = 0; i < a.size(); ++i) {
      _temp.close();
    }
    TRI_ASSERT(_temp.isClosed());
    input = _temp.slice();
  }

  auto res = _infos.getResponsibleClient(input);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(std::move(res).result());
  }
  return res.get();
}

ExecutionBlockImpl<DistributeExecutor>::ExecutionBlockImpl(
    ExecutionEngine* engine, DistributeNode const* node,
    RegisterInfos registerInfos, DistributeExecutorInfos&& executorInfos)
    : BlocksWithClientsImpl(engine, node, std::move(registerInfos),
                            std::move(executorInfos)) {}

}  // namespace arangodb::aql
