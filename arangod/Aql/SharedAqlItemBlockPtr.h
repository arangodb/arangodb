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

#ifndef ARANGOD_AQL_SHAREDAQLITEMBLOCKPTR_H
#define ARANGOD_AQL_SHAREDAQLITEMBLOCKPTR_H

#include "Aql/AqlItemBlock.h"
#include "Basics/debugging.h"

namespace arangodb {
namespace aql {

class SharedAqlItemBlockPtr {
 public:
  inline explicit SharedAqlItemBlockPtr(AqlItemBlock* aqlItemBlock) noexcept;

  // allow implicit cast from nullptr:
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions) cppcheck-suppress noExplicitConstructor
  constexpr inline SharedAqlItemBlockPtr(std::nullptr_t) noexcept;

  constexpr inline SharedAqlItemBlockPtr() noexcept;

  inline ~SharedAqlItemBlockPtr() noexcept;

  inline SharedAqlItemBlockPtr(SharedAqlItemBlockPtr const& other) noexcept;

  inline SharedAqlItemBlockPtr(SharedAqlItemBlockPtr&& other) noexcept;

  inline SharedAqlItemBlockPtr& operator=(SharedAqlItemBlockPtr const& other) noexcept;

  inline SharedAqlItemBlockPtr& operator=(SharedAqlItemBlockPtr&& other) noexcept;

  inline SharedAqlItemBlockPtr& operator=(std::nullptr_t) noexcept;

  inline AqlItemBlock& operator*() noexcept;
  inline AqlItemBlock* operator->() noexcept;

  inline AqlItemBlock const& operator*() const noexcept;
  inline AqlItemBlock const* operator->() const noexcept;

  inline AqlItemBlock* get() const noexcept;

  inline void reset(AqlItemBlock*) noexcept;

  inline void swap(SharedAqlItemBlockPtr& other) noexcept;

  inline bool operator==(std::nullptr_t) const noexcept;
  inline bool operator!=(std::nullptr_t) const noexcept;

  inline bool operator==(SharedAqlItemBlockPtr const&) const noexcept;
  inline bool operator!=(SharedAqlItemBlockPtr const&) const noexcept;

 private:
  inline void incrRefCount() const noexcept;
  // decrRefCount returns ("frees") _aqlItemBlock if the ref count reaches 0
  inline void decrRefCount() noexcept;

  AqlItemBlockManager& itemBlockManager() const noexcept;

  void returnBlock() noexcept;

 private:
  AqlItemBlock* _aqlItemBlock;
};

arangodb::aql::SharedAqlItemBlockPtr::SharedAqlItemBlockPtr(arangodb::aql::AqlItemBlock* aqlItemBlock) noexcept
    : _aqlItemBlock(aqlItemBlock) {
  // This constructor should only be used for fresh AqlItemBlocks in the
  // AqlItemBlockManager. All other places should already have a
  // SharedAqlItemBlockPtr.
  TRI_ASSERT(_aqlItemBlock != nullptr);
  TRI_ASSERT(_aqlItemBlock->getRefCount() == 0);
  _aqlItemBlock->incrRefCount();
}

constexpr arangodb::aql::SharedAqlItemBlockPtr::SharedAqlItemBlockPtr(std::nullptr_t) noexcept
    : _aqlItemBlock(nullptr) {}

constexpr arangodb::aql::SharedAqlItemBlockPtr::SharedAqlItemBlockPtr() noexcept
    : _aqlItemBlock(nullptr) {}

SharedAqlItemBlockPtr::~SharedAqlItemBlockPtr() noexcept { decrRefCount(); }

SharedAqlItemBlockPtr::SharedAqlItemBlockPtr(SharedAqlItemBlockPtr const& other) noexcept
    : _aqlItemBlock(other._aqlItemBlock) {
  TRI_ASSERT(this != &other);
  incrRefCount();
}

SharedAqlItemBlockPtr::SharedAqlItemBlockPtr(SharedAqlItemBlockPtr&& other) noexcept
    : _aqlItemBlock(other._aqlItemBlock) {
  TRI_ASSERT(this != &other);
  other._aqlItemBlock = nullptr;
}

SharedAqlItemBlockPtr& SharedAqlItemBlockPtr::operator=(SharedAqlItemBlockPtr const& other) noexcept {
  other.incrRefCount();
  decrRefCount();
  _aqlItemBlock = other._aqlItemBlock;
  return *this;
}

SharedAqlItemBlockPtr& SharedAqlItemBlockPtr::operator=(SharedAqlItemBlockPtr&& other) noexcept {
  TRI_ASSERT(this != &other);
  decrRefCount();
  _aqlItemBlock = other._aqlItemBlock;
  other._aqlItemBlock = nullptr;
  return *this;
}

SharedAqlItemBlockPtr& SharedAqlItemBlockPtr::operator=(std::nullptr_t) noexcept {
  decrRefCount();
  _aqlItemBlock = nullptr;
  return *this;
}

AqlItemBlock& SharedAqlItemBlockPtr::operator*() noexcept {
  TRI_ASSERT(_aqlItemBlock != nullptr);
  TRI_ASSERT(_aqlItemBlock->getRefCount() > 0);
  return *_aqlItemBlock;
}

AqlItemBlock* SharedAqlItemBlockPtr::operator->() noexcept {
  TRI_ASSERT(_aqlItemBlock != nullptr);
  TRI_ASSERT(_aqlItemBlock->getRefCount() > 0);
  return _aqlItemBlock;
}

AqlItemBlock const& SharedAqlItemBlockPtr::operator*() const noexcept {
  TRI_ASSERT(_aqlItemBlock != nullptr);
  TRI_ASSERT(_aqlItemBlock->getRefCount() > 0);
  return *_aqlItemBlock;
}

AqlItemBlock const* SharedAqlItemBlockPtr::operator->() const noexcept {
  TRI_ASSERT(_aqlItemBlock != nullptr);
  TRI_ASSERT(_aqlItemBlock->getRefCount() > 0);
  return _aqlItemBlock;
}

void SharedAqlItemBlockPtr::incrRefCount() const noexcept {
  if (_aqlItemBlock != nullptr) {
    _aqlItemBlock->incrRefCount();
  }
}

bool SharedAqlItemBlockPtr::operator==(std::nullptr_t) const noexcept {
  return _aqlItemBlock == nullptr;
}

bool SharedAqlItemBlockPtr::operator!=(std::nullptr_t) const noexcept {
  return _aqlItemBlock != nullptr;
}

bool SharedAqlItemBlockPtr::operator==(SharedAqlItemBlockPtr const& other) const noexcept {
  return _aqlItemBlock == other._aqlItemBlock;
}

bool SharedAqlItemBlockPtr::operator!=(SharedAqlItemBlockPtr const& other) const noexcept {
  return _aqlItemBlock != other._aqlItemBlock;
}

AqlItemBlock* SharedAqlItemBlockPtr::get() const noexcept {
  TRI_ASSERT(_aqlItemBlock == nullptr || _aqlItemBlock->getRefCount() > 0);
  return _aqlItemBlock;
}

void SharedAqlItemBlockPtr::reset(AqlItemBlock* other) noexcept {
  TRI_ASSERT(_aqlItemBlock != other);
  decrRefCount();
  _aqlItemBlock = other;
  incrRefCount();
}

void SharedAqlItemBlockPtr::swap(SharedAqlItemBlockPtr& other) noexcept {
  AqlItemBlock* tmp = _aqlItemBlock;
  _aqlItemBlock = other._aqlItemBlock;
  other._aqlItemBlock = tmp;
}

void arangodb::aql::SharedAqlItemBlockPtr::decrRefCount() noexcept {
  if (_aqlItemBlock != nullptr &&
      _aqlItemBlock->decrRefCount() == 0) {
    returnBlock();
  }
}

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_SHAREDAQLITEMBLOCKPTR_H
