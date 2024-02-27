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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ErrorCode.h"
#include "Cache/CachedValue.h"

namespace arangodb::cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief A helper class for managing CachedValue lifecycles.
///
/// Returned to clients by Cache::find. Clients must destroy the Finding
/// object within a short period of time to allow proper memory management
/// within the cache system. If the underlying value needs to be retained for
/// any significant period of time, it must be copied so that the finding
/// object may be destroyed.
////////////////////////////////////////////////////////////////////////////////
class Finding {
 public:
  Finding() noexcept;
  explicit Finding(CachedValue* v) noexcept;
  explicit Finding(CachedValue* v, ::ErrorCode r) noexcept;
  Finding(Finding const& other) = delete;
  Finding(Finding&& other) noexcept;
  Finding& operator=(Finding const& other) = delete;
  Finding& operator=(Finding&& other) noexcept;
  ~Finding();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Changes the underlying CachedValue pointer.
  //////////////////////////////////////////////////////////////////////////////
  void reset(CachedValue* v) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Sets the underlying CachedValue pointer. Assumes that the Finding
  /// is currently empty
  //////////////////////////////////////////////////////////////////////////////
  void set(CachedValue* v) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Sets the error code.
  //////////////////////////////////////////////////////////////////////////////
  void reportError(::ErrorCode r) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Specifies whether the value was found. If not, value is nullptr.
  //////////////////////////////////////////////////////////////////////////////
  bool found() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the underlying value pointer.
  //////////////////////////////////////////////////////////////////////////////
  CachedValue const* value() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Creates a copy of the underlying value and returns a pointer.
  //////////////////////////////////////////////////////////////////////////////
  CachedValue* copy() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Releases the finding.
  //////////////////////////////////////////////////////////////////////////////
  void release() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the status result object associated with the lookup.
  /////////////////////////////////////////////////////////////////////////////
  ::ErrorCode result() const noexcept;

 private:
  CachedValue* _value;
  ::ErrorCode _result;
};

}  // end namespace arangodb::cache
