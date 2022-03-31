////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace arangodb::basics::Nonce {

static constexpr size_t numNonces = 2 * 1024 * 1024;

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the nonce storage
////////////////////////////////////////////////////////////////////////////////

void destroy();

////////////////////////////////////////////////////////////////////////////////
/// @brief create a nonce
////////////////////////////////////////////////////////////////////////////////

std::string createNonce();

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a nonce is valid and invalidates it
////////////////////////////////////////////////////////////////////////////////

bool checkAndMark(std::string const& nonce);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a nonce is valid and invalidates it
////////////////////////////////////////////////////////////////////////////////

bool checkAndMark(uint32_t timestamp, uint64_t random);

}  // namespace arangodb::basics::Nonce
