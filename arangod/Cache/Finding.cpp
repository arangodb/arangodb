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

#include "Cache/Finding.h"
#include "Cache/CachedValue.h"

using namespace arangodb::cache;

Finding::Finding(CachedValue* v) : _value(v) {
  if (_value != nullptr) {
    _value->lease();
  }
}

Finding::Finding(Finding const& other) : _value(other._value) {
  if (_value != nullptr) {
    _value->lease();
  }
}

Finding::Finding(Finding&& other) : _value(other._value) {
  other._value = nullptr;
}

Finding& Finding::operator=(Finding const& other) {
  if (&other == this) {
    return *this;
  }

  if (_value != nullptr) {
    _value->release();
  }

  _value = other._value;
  if (_value != nullptr) {
    _value->lease();
  }

  return *this;
}

Finding& Finding::operator=(Finding&& other) {
  if (&other == this) {
    return *this;
  }

  if (_value != nullptr) {
    _value->release();
  }

  _value = other._value;
  other._value = nullptr;

  return *this;
}

Finding::~Finding() {
  if (_value != nullptr) {
    _value->release();
  }
}

void Finding::release() {
  if (_value != nullptr) {
    _value->release();
  }
}

void Finding::reset(CachedValue* v) {
  if (_value != nullptr) {
    _value->release();
  }

  _value = v;
  if (_value != nullptr) {
    _value->lease();
  }
}

bool Finding::found() const { return (_value != nullptr); }

CachedValue const* Finding::value() const { return _value; }

CachedValue* Finding::copy() const {
  return ((_value == nullptr) ? nullptr : _value->copy());
}
