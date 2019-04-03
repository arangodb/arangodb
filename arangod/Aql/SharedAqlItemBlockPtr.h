////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_SHAREDAQLITEMBLOCKPTR_H
#define ARANGOD_AQL_SHAREDAQLITEMBLOCKPTR_H

#include "AqlItemBlock.h"

namespace arangodb {
namespace aql {

class SharedAqlItemBlockPtr {
 public:
  // TODO add as many noexcepts as possible
  inline explicit SharedAqlItemBlockPtr(AqlItemBlock& aqlItemBlock);
  inline ~SharedAqlItemBlockPtr();
  inline SharedAqlItemBlockPtr(SharedAqlItemBlockPtr const& sharedAqlItemBlockPtr);

  // We can't assign with a reference member. Maybe we want to allow it and use a pointer instead?
  inline SharedAqlItemBlockPtr& operator=(SharedAqlItemBlockPtr const& sharedAqlItemBlockPtr) = delete;

  // Move constructors aren't useful: We had to allow _aqlItemBlock to be a
  // nullptr and then set the moved-from ptr to null, instead of increasing the
  // refcount. This would also incur an additional check in the destructor and
  // can thus not
  inline SharedAqlItemBlockPtr(SharedAqlItemBlockPtr&& sharedAqlItemBlockPtr) = delete;
  inline SharedAqlItemBlockPtr& operator=(SharedAqlItemBlockPtr&& sharedAqlItemBlockPtr) = delete;

  inline AqlItemBlock& operator*() noexcept;
  inline AqlItemBlock* operator->() noexcept;

 private:
  inline void incrRefCount() const noexcept;
  inline void decrRefCount() const noexcept;
  inline AqlItemBlock& block() const noexcept;
  inline AqlItemBlockManager& itemBlockManager() const noexcept;

 private:
  AqlItemBlock& _aqlItemBlock;
};

arangodb::aql::SharedAqlItemBlockPtr::SharedAqlItemBlockPtr(arangodb::aql::AqlItemBlock& aqlItemBlock)
    : _aqlItemBlock(aqlItemBlock) {
  incrRefCount();
}

SharedAqlItemBlockPtr::~SharedAqlItemBlockPtr() {
  decrRefCount();
  if (block().getRefCount() == 0) {
    AqlItemBlock* blockPtr = &_aqlItemBlock;
    itemBlockManager().returnBlock(blockPtr);
  }
}

SharedAqlItemBlockPtr::SharedAqlItemBlockPtr(SharedAqlItemBlockPtr const& sharedAqlItemBlockPtr)
    : _aqlItemBlock(sharedAqlItemBlockPtr._aqlItemBlock) {
  incrRefCount();
}

AqlItemBlock& SharedAqlItemBlockPtr::operator*() noexcept {
  return block();
}

AqlItemBlock* SharedAqlItemBlockPtr::operator->() noexcept {
  return &block();
}

void SharedAqlItemBlockPtr::incrRefCount() const noexcept {
  block().incrRefCount();
}

void SharedAqlItemBlockPtr::decrRefCount() const noexcept {
  block().decrRefCount();
}

AqlItemBlock& SharedAqlItemBlockPtr::block() const noexcept {
  return _aqlItemBlock;
}

AqlItemBlockManager& SharedAqlItemBlockPtr::itemBlockManager() const noexcept {
  return _aqlItemBlock.aqlItemBlockManager();
}

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_SHAREDAQLITEMBLOCKPTR_H
