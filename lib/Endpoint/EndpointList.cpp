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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <utility>

#include "EndpointList.h"

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::basics;

EndpointList::EndpointList() {}

EndpointList::~EndpointList() = default;

////////////////////////////////////////////////////////////////////////////////
/// @brief add a new endpoint
////////////////////////////////////////////////////////////////////////////////

bool EndpointList::add(std::string const& specification, int backLogSize,
                       bool reuseAddress) {
  std::string key = Endpoint::unifiedForm(specification);

  if (key.empty()) {
    return false;
  }

  auto it = _endpoints.find(key);

  if (it != _endpoints.end()) {
    return true;
  }

  std::unique_ptr<Endpoint> ep(
      Endpoint::serverFactory(key, backLogSize, reuseAddress));

  if (ep == nullptr) {
    return false;
  }

  _endpoints.emplace(std::move(key), std::move(ep));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all endpoints
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> EndpointList::all() const {
  std::vector<std::string> result;
  result.reserve(_endpoints.size());

  for (auto& it : _endpoints) {
    result.emplace_back(it.first);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all typed endpoints
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> EndpointList::all(
    Endpoint::TransportType transport) const {
  std::string_view prefix;

  switch (transport) {
    case Endpoint::TransportType::HTTP:
      prefix = "http+";
      break;

    case Endpoint::TransportType::VST:
      prefix = "vst+";
      break;
  }

  std::vector<std::string> result;
  for (auto& it : _endpoints) {
    if (it.first.starts_with(prefix)) {
      result.emplace_back(it.first);
    }
  }

  return result;
}

void EndpointList::apply(
    std::function<void(std::string const&, Endpoint&)> const& cb) {
  for (auto& it : _endpoints) {
    TRI_ASSERT(it.second != nullptr);
    cb(it.first, *it.second);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return if there is an endpoint with SSL
////////////////////////////////////////////////////////////////////////////////

bool EndpointList::hasSsl() const {
  for (auto& it : _endpoints) {
    if (it.first.starts_with("http+ssl://") ||
        it.first.starts_with("vst+ssl://")) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump all endpoints used
////////////////////////////////////////////////////////////////////////////////

void EndpointList::dump() const {
  for (auto const& it : _endpoints) {
    TRI_ASSERT(it.second != nullptr);
    LOG_TOPIC("6ea38", INFO, arangodb::Logger::FIXME)
        << "using endpoint '" << it.first << "' for "
        << encryptionName(it.second->encryption()) << " requests";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an encryption name
////////////////////////////////////////////////////////////////////////////////

std::string_view EndpointList::encryptionName(
    Endpoint::EncryptionType encryption) {
  switch (encryption) {
    case Endpoint::EncryptionType::SSL:
      return "ssl-encrypted";
    case Endpoint::EncryptionType::NONE:
    default:
      return "non-encrypted";
  }
}
