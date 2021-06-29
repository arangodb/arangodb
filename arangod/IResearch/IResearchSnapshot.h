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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
namespace iresearch {
//////////////////////////////////////////////////////////////////////////////
/// @brief a snapshot representation of the data-store
///        locked to prevent data store deallocation
//////////////////////////////////////////////////////////////////////////////
class Snapshot {
  public:
  Snapshot() = default;
  Snapshot(std::unique_lock<ReadMutex>&& lock,
            irs::directory_reader&& reader) noexcept
      : _lock(std::move(lock)), _reader(std::move(reader)) {
    TRI_ASSERT(_lock.owns_lock());
  }
  Snapshot(Snapshot&& rhs) noexcept
    : _lock(std::move(rhs._lock)),
      _reader(std::move(rhs._reader)) {
    TRI_ASSERT(_lock.owns_lock());
  }
  Snapshot& operator=(Snapshot&& rhs) noexcept {
    if (this != &rhs) {
      _lock = std::move(rhs._lock);
      _reader = std::move(rhs._reader);
    }
    TRI_ASSERT(_lock.owns_lock());
    return *this;
  }
  operator irs::directory_reader const&() const noexcept {
    return _reader;
  }

  private:
  std::unique_lock<ReadMutex> _lock; // lock preventing data store dealocation
  irs::directory_reader _reader;
};
} // namespace iresearch
} // namespace arangodb
