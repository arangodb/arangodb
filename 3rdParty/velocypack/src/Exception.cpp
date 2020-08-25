////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <ostream>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"

using namespace arangodb::velocypack;

Exception::Exception(ExceptionType type, char const* msg) noexcept
    : _type(type), _msg(msg) {}

std::ostream& operator<<(std::ostream& stream, Exception const* ex) {
  stream << "[Exception " << ex->what() << "]";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, Exception const& ex) {
  return operator<<(stream, &ex);
}
