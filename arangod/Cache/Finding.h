////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_FINDING_H
#define ARANGODB_CACHE_FINDING_H

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Cache/CachedValue.h"

namespace arangodb {
namespace cache {

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
  Finding();
  explicit Finding(CachedValue* v);
  explicit Finding(CachedValue* v, Result const& r);
  Finding(Finding const& other) = delete;
  Finding(Finding&& other);
  Finding& operator=(Finding const& other) = delete;
  Finding& operator=(Finding&& other);
  ~Finding();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Changes the underlying CachedValue pointer.
  //////////////////////////////////////////////////////////////////////////////
  void reset(CachedValue* v);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Sets the underlying CachedValue pointer. Assumes that the Finding
  /// is currently empty
  //////////////////////////////////////////////////////////////////////////////
  void set(CachedValue* v);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Sets the error Result object.
  //////////////////////////////////////////////////////////////////////////////
  void reportError(Result const& r);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Specifies whether the value was found. If not, value is nullptr.
  //////////////////////////////////////////////////////////////////////////////
  bool found() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the underlying value pointer.
  //////////////////////////////////////////////////////////////////////////////
  CachedValue const* value() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Creates a copy of the underlying value and returns a pointer.
  //////////////////////////////////////////////////////////////////////////////
  CachedValue* copy() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Releases the finding.
  //////////////////////////////////////////////////////////////////////////////
  void release();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the status result object associated with the lookup.
  /////////////////////////////////////////////////////////////////////////////
  Result const& result() const;

 private:
  CachedValue* _value;
  Result _result;
};

};  // end namespace cache
};  // end namespace arangodb

#endif
