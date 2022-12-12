////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Basics/ReadWriteLock.h"
#include "Basics/RecursiveLocker.h"

#include <memory>
#include <vector>

namespace arangodb {
class Index;
class PhysicalCollection;

class IndexesSnapshot {
 public:
  explicit IndexesSnapshot(PhysicalCollection& collection);
  ~IndexesSnapshot();

  IndexesSnapshot(IndexesSnapshot const& other) = delete;
  IndexesSnapshot& operator=(IndexesSnapshot const& other) = delete;
  IndexesSnapshot(IndexesSnapshot&& other) = default;
  IndexesSnapshot& operator=(IndexesSnapshot&& other) = delete;

  std::vector<std::shared_ptr<Index>> const& getIndexes() const noexcept;
  void release();

  bool hasSecondaryIndex() const noexcept;

 private:
  std::vector<std::shared_ptr<Index>> _indexes;
  RecursiveReadLocker<basics::ReadWriteLock> _locker;
  bool _valid;
};

}  // namespace arangodb
