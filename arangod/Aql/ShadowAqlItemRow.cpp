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

#include "ShadowAqlItemRow.h"

#include "Basics/VelocyPackHelper.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;

ShadowAqlItemRow::ShadowAqlItemRow(SharedAqlItemBlockPtr block, size_t baseIndex)
    : _block(std::move(block)), _baseIndex(baseIndex) {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(_baseIndex < _block->numRows());
  TRI_ASSERT(_block->isShadowRow(_baseIndex));
}

RegisterCount ShadowAqlItemRow::getNumRegisters() const noexcept {
  return block().numRegisters();
}

bool ShadowAqlItemRow::isRelevant() const noexcept { return getDepth() == 0; }

bool ShadowAqlItemRow::isInitialized() const {
  return _block != nullptr;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
bool ShadowAqlItemRow::internalBlockIs(SharedAqlItemBlockPtr const& other, size_t index) const {
  return _block == other && _baseIndex == index;
}
#endif

AqlValue const& ShadowAqlItemRow::getValue(RegisterId registerId) const {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(registerId.isRegularRegister());
  TRI_ASSERT(registerId < getNumRegisters());
  return block().getValueReference(_baseIndex, registerId.value());
}

AqlValue ShadowAqlItemRow::stealAndEraseValue(RegisterId registerId) {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(registerId < getNumRegisters());
  // caller needs to take immediate ownership.
  return block().stealAndEraseValue(_baseIndex, registerId);
}

size_t ShadowAqlItemRow::getShadowDepthValue() const {
  TRI_ASSERT(isInitialized());
  return block().getShadowRowDepth(_baseIndex);
}

uint64_t ShadowAqlItemRow::getDepth() const {
  TRI_ASSERT(isInitialized());
  return static_cast<uint64_t>(block().getShadowRowDepth(_baseIndex));
}

AqlItemBlock& ShadowAqlItemRow::block() noexcept {
  TRI_ASSERT(_block != nullptr);
  return *_block;
}

AqlItemBlock const& ShadowAqlItemRow::block() const noexcept {
  TRI_ASSERT(_block != nullptr);
  return *_block;
}

bool ShadowAqlItemRow::isSameBlockAndIndex(ShadowAqlItemRow const& other) const noexcept {
    return this->_block == other._block && this->_baseIndex == other._baseIndex;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
bool ShadowAqlItemRow::equates(ShadowAqlItemRow const& other,
                               velocypack::Options const* options) const noexcept {
  if (!isInitialized() || !other.isInitialized()) {
    return isInitialized() == other.isInitialized();
  }
  TRI_ASSERT(getNumRegisters() == other.getNumRegisters());
  if (getNumRegisters() != other.getNumRegisters()) {
    return false;
  }
  if (getDepth() != other.getDepth()) {
    return false;
  }
  auto const eq = [options](auto left, auto right) {
    return 0 == AqlValue::Compare(options, left, right, false);
  };
  for (RegisterId::value_t i = 0; i < getNumRegisters(); ++i) {
    if (!eq(getValue(i), other.getValue(i))) {
      return false;
    }
  }

  return true;
}
#endif
