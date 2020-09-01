////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_VPACKSTRING_H
#define ARANGODB_BASICS_VPACKSTRING_H

#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"

#include <velocypack/Sink.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace basics {

class VPackStringBufferAdapter final : public VPackSink {
 public:
  explicit VPackStringBufferAdapter(TRI_string_buffer_t* buffer)
      : _buffer(buffer) {}

  void push_back(char c) override final {
    int res = TRI_AppendCharStringBuffer(_buffer, c);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  void append(std::string const& p) override final {
    int res = TRI_AppendString2StringBuffer(_buffer, p.c_str(), p.size());
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  void append(char const* p) override final {
    int res = TRI_AppendString2StringBuffer(_buffer, p, strlen(p));
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }
  void append(char const* p, uint64_t len) override final {
    int res = TRI_AppendString2StringBuffer(_buffer, p, static_cast<size_t>(len));
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  void reserve(uint64_t len) override final {
    int res = TRI_ReserveStringBuffer(_buffer, static_cast<size_t>(len));
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

 private:
  TRI_string_buffer_t* _buffer;
};
}  // namespace basics
}  // namespace arangodb

#endif
