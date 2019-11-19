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

#include "ShadowAqlItemRow.h"

#include "Basics/VelocyPackHelper.h"
#include "Transaction/Methods.h"
#include "Transaction/Context.h"

using namespace arangodb;
using namespace arangodb::aql;

ShadowAqlItemRow::ShadowAqlItemRow(SharedAqlItemBlockPtr block, size_t baseIndex)
    : _block(std::move(block)), _baseIndex(baseIndex) {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(_baseIndex < _block->size());
  TRI_ASSERT(_block->isShadowRow(_baseIndex));
}

RegisterCount ShadowAqlItemRow::getNrRegisters() const noexcept {
  return block().getNrRegs();
}

bool ShadowAqlItemRow::isRelevant() const noexcept { return getDepth() == 0; }

bool ShadowAqlItemRow::isInitialized() const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_block != nullptr) {
    // The value needs to always be a positive integer.
    auto depthVal = block().getShadowRowDepth(_baseIndex);
    TRI_ASSERT(depthVal.isNumber());
    TRI_ASSERT(depthVal.toInt64() >= 0);
  }
#endif
  return _block != nullptr;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
bool ShadowAqlItemRow::internalBlockIs(SharedAqlItemBlockPtr const& other) const {
  return _block == other;
}
#endif

AqlValue const& ShadowAqlItemRow::getValue(RegisterId registerId) const {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(registerId < getNrRegisters());
  return block().getValueReference(_baseIndex, registerId);
}

AqlValue const& ShadowAqlItemRow::getShadowDepthValue() const {
  TRI_ASSERT(isInitialized());
  return block().getShadowRowDepth(_baseIndex);
}

uint64_t ShadowAqlItemRow::getDepth() const {
  TRI_ASSERT(isInitialized());
  auto value = block().getShadowRowDepth(_baseIndex);
  TRI_ASSERT(value.toInt64() >= 0);
  return static_cast<uint64_t>(value.toInt64());
}

AqlItemBlock& ShadowAqlItemRow::block() noexcept {
  TRI_ASSERT(_block != nullptr);
  return *_block;
}

AqlItemBlock const& ShadowAqlItemRow::block() const noexcept {
  TRI_ASSERT(_block != nullptr);
  return *_block;
}

bool ShadowAqlItemRow::operator==(ShadowAqlItemRow const& other) const noexcept {
  return this->_block == other._block && this->_baseIndex == other._baseIndex;
}

bool ShadowAqlItemRow::operator!=(ShadowAqlItemRow const& other) const noexcept {
  return !(*this == other);
}

bool ShadowAqlItemRow::equates(ShadowAqlItemRow const& other,
                               velocypack::Options const* options) const noexcept {
  if (!isInitialized() || !other.isInitialized()) {
    return isInitialized() == other.isInitialized();
  }
  TRI_ASSERT(getNrRegisters() == other.getNrRegisters());
  if (getNrRegisters() != other.getNrRegisters()) {
    return false;
  }
  if (getDepth() != other.getDepth()) {
    return false;
  }
  auto const eq = [options](auto left, auto right) {
    return 0 == AqlValue::Compare(options, left, right, false);
  };
  for (RegisterId i = 0; i < getNrRegisters(); ++i) {
    if (!eq(getValue(i), other.getValue(i))) {
      return false;
    }
  }

  return true;
}
