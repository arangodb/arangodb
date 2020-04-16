////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_UTILS_CURSOR_H
#define ARANGOD_UTILS_CURSOR_H 1

#include "Aql/ExecutionState.h"
#include "Basics/Common.h"
#include "Basics/system-functions.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/voc-types.h"

#include <velocypack/Iterator.h>

namespace arangodb {

namespace transaction {
class Context;
}

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

typedef TRI_voc_tick_t CursorId;

class Cursor {
 public:
  Cursor(Cursor const&) = delete;
  Cursor& operator=(Cursor const&) = delete;

  Cursor(CursorId id, size_t batchSize, double ttl, bool hasCount)
      : _id(id),
        _batchSize(batchSize),
        _ttl(ttl),
        _expires(TRI_microtime() + _ttl),
        _hasCount(hasCount),
        _isDeleted(false),
        _isUsed(false) {}

  virtual ~Cursor() = default;

 public:
  CursorId id() const { return _id; }

  inline size_t batchSize() const { return _batchSize; }

  inline bool hasCount() const { return _hasCount; }

  inline double ttl() const { return _ttl; }

  inline double expires() const { return _expires; }

  inline bool isUsed() const { return _isUsed; }

  inline bool isDeleted() const { return _isDeleted; }

  void setDeleted() { _isDeleted = true; }

  void use() {
    TRI_ASSERT(!_isDeleted);
    TRI_ASSERT(!_isUsed);

    _isUsed = true;
    _expires = TRI_microtime() + _ttl;
  }

  void release() {
    TRI_ASSERT(_isUsed);
    _isUsed = false;
  }

  virtual void kill() {}

  virtual size_t count() const = 0;

  virtual std::shared_ptr<transaction::Context> context() const = 0;

  /**
   * @brief Dump the cursor result, async version. The caller needs to be
   * contiueable
   *
   * @param result The Builder to write the result to
   * @param continueHandler The function that is posted on scheduler to contiue
   * this execution.
   *
   * @return First: ExecutionState either DONE or WAITING. On Waiting we need to
   * free this thread on DONE we have a result. Second: Result If State==DONE
   * this contains Error information or NO_ERROR. On NO_ERROR result is filled.
   */
  virtual std::pair<aql::ExecutionState, Result> dump(velocypack::Builder& result) = 0;

  /**
   * @brief Dump the cursor result. This is guaranteed to return the result in
   * this thread.
   *
   * @param result the Builder to write the result to
   *
   * @return ErrorResult, if something goes wrong
   */
  virtual Result dumpSync(velocypack::Builder& result) = 0;
  
  /// Set wakeup handler on streaming cursor
  virtual void setWakeupHandler(std::function<bool()> const& cb) {}
  virtual void resetWakeupHandler() {}

 protected:
  CursorId const _id;
  size_t const _batchSize;
  double _ttl;
  double _expires;
  bool const _hasCount;
  bool _isDeleted;
  bool _isUsed;
};
}  // namespace arangodb

#endif
