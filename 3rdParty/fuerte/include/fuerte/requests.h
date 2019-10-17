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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef ARANGO_CXX_DRIVER_REQUESTS
#define ARANGO_CXX_DRIVER_REQUESTS

#include "message.h"

// This file will be a bit messy in the near future
//
// Maybe we will use some kind of mpl library that allows us to use named and
// defaulted arguments (not only at the end of signature) for constructors.
// The way it is now we need too supply too many different make functions.
// At the is probably the best to create request manually and fill in the
// required fields
//

//
// Fuerte defaults to VelocyPack for sending and receiving
//

//
// should it default to VelocyPack for sending, maybe the contentType
// should be required when payload is added.
//

namespace arangodb { namespace fuerte { inline namespace v1 {
// Helper and Implementation

std::unique_ptr<Request> createRequest(RestVerb verb,
                                       ContentType contentType);

// For User
std::unique_ptr<Request> createRequest(RestVerb verb, std::string const& path,
                                       StringMap const& parameter,
                                       velocypack::Buffer<uint8_t> payload);

std::unique_ptr<Request> createRequest(RestVerb verb, std::string const& path,
                                       StringMap const& parameter,
                                       velocypack::Slice const payload);

std::unique_ptr<Request> createRequest(
    RestVerb verb, std::string const& path,
    StringMap const& parameter = StringMap());
}}}  // namespace arangodb::fuerte::v1
#endif
