////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "Cache/Finding.h"

#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Cache/CachedValue.h"

namespace arangodb::cache {

Finding::Finding() noexcept : _value(nullptr), _result(TRI_ERROR_NO_ERROR) {}

Finding::Finding(CachedValue* v) noexcept : Finding(v, TRI_ERROR_NO_ERROR) {}

Finding::Finding(CachedValue* v, ::ErrorCode r) noexcept
    : _value(v), _result(r) {
  if (_value != nullptr) {
    _value->lease();
  }
}

Finding::Finding(Finding&& other) noexcept
    : _value(other._value), _result(other._result) {
  other._value = nullptr;
  other._result = TRI_ERROR_NO_ERROR;
}

Finding& Finding::operator=(Finding&& other) noexcept {
  if (&other == this) {
    return *this;
  }

  if (_value != nullptr) {
    _value->release();
  }

  _value = other._value;
  other._value = nullptr;

  _result = other._result;
  other._result = TRI_ERROR_NO_ERROR;

  return *this;
}

Finding::~Finding() {
  if (_value != nullptr) {
    _value->release();
  }
}

void Finding::release() noexcept {
  if (_value != nullptr) {
    _value->release();
    // reset value so we do not unintentionally release multiple times
    _value = nullptr;
  }
}

void Finding::set(CachedValue* v) noexcept {
  TRI_ASSERT(_value == nullptr);
  _value = v;
  if (v != nullptr) {
    _value->lease();
  }
}

void Finding::reset(CachedValue* v) noexcept {
  if (_value != nullptr) {
    _value->release();
  }

  _value = v;
  if (_value != nullptr) {
    _value->lease();
  }
}

void Finding::reportError(::ErrorCode r) noexcept { _result = r; }

bool Finding::found() const noexcept { return (_value != nullptr); }

CachedValue const* Finding::value() const noexcept { return _value; }

CachedValue* Finding::copy() const {
  return ((_value == nullptr) ? nullptr : _value->copy());
}

::ErrorCode Finding::result() const noexcept { return _result; }

}  // namespace arangodb::cache
