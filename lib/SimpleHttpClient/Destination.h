////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SIMPLE_HTTP_CLIENT_DESTINATION_H
#define ARANGODB_SIMPLE_HTTP_CLIENT_DESTINATION_H 1

#include "Basics/Common.h"

namespace arangodb {
class Endpoint;

namespace communicator {
class Destination {
 public:
  explicit Destination(std::string const& url) : _url(url) {}
  Destination(Destination const&) = default;
  Destination(Destination&&) = default;

 public:
  std::string const& url() const { return _url; }

 private:
  std::string _url;
};
}
}

#endif
