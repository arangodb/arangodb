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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "BasicBlocks.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

std::pair<ExecutionState, arangodb::Result> LimitBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  _state = State::INITFULLCOUNT;
  _count = 0;
  _remainingOffset = _offset;
  _result = nullptr;
  _limitSkipped = 0;

  return ExecutionBlock::initializeCursor(items, pos);
}

std::pair<ExecutionState, arangodb::Result> LimitBlock::getOrSkipSome(
    size_t atMost, bool skipping, AqlItemBlock*& result_, size_t& skipped_) {
  TRI_ASSERT(result_ == nullptr && skipped_ == 0);

  switch (_state) {
    case State::DONE:
      TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
      return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
    case State::INITFULLCOUNT: {
      if (_fullCount) {
        _engine->_stats.fullCount = 0;
      }
      _state = State::SKIPPING;
    }
    // intentionally falls through
    case State::SKIPPING: {
      while (_remainingOffset > 0) {
        auto res = _dependencies[0]->skipSome(_remainingOffset);
        if (res.first == ExecutionState::WAITING) {
          return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
        }
        if (_fullCount) {
          _engine->_stats.fullCount += static_cast<int64_t>(res.second);
        }
        TRI_ASSERT(_remainingOffset >= res.second);
        _remainingOffset -= res.second;
        if (res.first == ExecutionState::DONE) {
          break;
        }
      }
      _count = 0;
      if (_limit == 0 && !_fullCount) {
        // quick exit for limit == 0
        _state = State::DONE;
        TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
        return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
      }
      _state = State::RETURNING;
    }
    // intentionally falls through
    case State::RETURNING: {
      if (_count < _limit) {
        if (atMost > _limit - _count) {
          atMost = _limit - _count;
        }
        AqlItemBlock* res = nullptr;
        TRI_ASSERT(_limitSkipped == 0);
        auto state = ExecutionBlock::getOrSkipSome(atMost, skipping, res, _limitSkipped);
        TRI_ASSERT(_result == nullptr);
        _result.reset(res);
        if (state.first == ExecutionState::WAITING) {
          TRI_ASSERT(result_ == nullptr);
          // On WAITING we will continue here, on error we bail out
          return state;
        }

        if (_limitSkipped == 0) {
          _state = State::DONE;
          TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
          result_ = _result.release();
          // there should be no need to assign skipped_ = _limitSkipped and
          // _limitSkipped = 0 here:
          TRI_ASSERT(skipped_ == 0);
          return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
        }

        _count += _limitSkipped;
        if (_fullCount) {
          _engine->_stats.fullCount += static_cast<int64_t>(_limitSkipped);
        }
      }
      if (_count >= _limit) {
        if (!_fullCount) {
          _state = State::DONE;
        } else {
          // if fullCount is set, we must fetch all elements from the
          // dependency.

          // suck out all data from the dependencies
          while (_state != State::DONE) {
            AqlItemBlock* ignore = nullptr;
            // local skipped count
            size_t skipped = 0;
            // We're only counting here, so skip. Using the DefaultBatchSize
            // instead of atMost because we need to skip everything if we have
            // to calculate fullCount.
            auto res = ExecutionBlock::getOrSkipSome(DefaultBatchSize(), true, ignore, skipped);
            TRI_ASSERT(ignore == nullptr);

            ExecutionState state = res.first;
            Result result = res.second;
            if (state == ExecutionState::WAITING || result.fail()) {
              // On WAITING we will continue here, on error we bail out
              return res;
            }

            _engine->_stats.fullCount += skipped;

            if (skipped == 0) {
              _state = State::DONE;
            }
          }
        }
      }
    }
  }

  result_ = _result.release();
  skipped_ = _limitSkipped;
  _limitSkipped = 0;

  return {getHasMoreState(), TRI_ERROR_NO_ERROR};
}

ExecutionState LimitBlock::getHasMoreState() {
  if (_state == State::DONE) {
    return ExecutionState::DONE;
  }
  return ExecutionState::HASMORE;
}
