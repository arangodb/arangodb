////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/StringUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::basics;

static std::vector<std::string> const EmptyMapping;

////////////////////////////////////////////////////////////////////////////////
/// @brief create an endpoint list
////////////////////////////////////////////////////////////////////////////////

EndpointList::EndpointList() : _endpoints() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an endpoint list
////////////////////////////////////////////////////////////////////////////////

EndpointList::~EndpointList() {
  for (auto& it : _endpoints) {
    delete it.second;
  }

  _endpoints.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a new endpoint
////////////////////////////////////////////////////////////////////////////////

bool EndpointList::add(std::string const& specification, int backLogSize, bool reuseAddress) {
  std::string const key = Endpoint::unifiedForm(specification);

  if (key.empty()) {
    return false;
  }

  auto it = _endpoints.find(key);

  if (it != _endpoints.end()) {
    return true;
  }

  Endpoint* ep = Endpoint::serverFactory(key, backLogSize, reuseAddress);

  if (ep == nullptr) {
    return false;
  }

  _endpoints[key] = ep;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a specific endpoint
////////////////////////////////////////////////////////////////////////////////

bool EndpointList::remove(std::string const& specification, Endpoint** dst) {
  std::string const key = Endpoint::unifiedForm(specification);

  if (key.empty()) {
    return false;
  }

  if (_endpoints.size() <= 1) {
    // must not remove last endpoint
    return false;
  }

  auto it = _endpoints.find(key);

  if (it == _endpoints.end()) {
    // not in list
    return false;
  }

  *dst = (*it).second;
  _endpoints.erase(key);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all endpoints
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> EndpointList::all() const {
  std::vector<std::string> result;

  for (auto& it : _endpoints) {
    result.emplace_back(it.first);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all typed endpoints
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> EndpointList::all(Endpoint::TransportType transport) const {
  std::vector<std::string> result;
  std::string prefix;

  switch (transport) {
    case Endpoint::TransportType::HTTP:
      prefix = "http+";
      break;

    case Endpoint::TransportType::VST:
      prefix = "vst+";
      break;
  }

  for (auto& it : _endpoints) {
    std::string const& key = it.first;

    if (StringUtils::isPrefix(key, prefix)) {
      result.emplace_back(it.first);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all endpoints with a certain encryption type
////////////////////////////////////////////////////////////////////////////////

// std::map<std::string, Endpoint*> EndpointList::matching(
//    Endpoint::TransportType transport,
//    Endpoint::EncryptionType encryption) const {
//  std::string prefix;
//
//  switch (transport) {
//    case Endpoint::TransportType::HTTP:
//      prefix = "http+";
//      break;
//
//    case Endpoint::TransportType::VST:
//      prefix = "vpp+";
//      break;
//  }
//
//  std::map<std::string, Endpoint*> result;
//
//  for (auto& it : _endpoints) {
//    std::string const& key = it.first;
//
//    if (encryption == Endpoint::EncryptionType::SSL) {
//      if (StringUtils::isPrefix(key, prefix + "ssl://")) {
//        result[key] = it.second;
//      }
//    } else {
//      if (StringUtils::isPrefix(key, prefix + "tcp://") ||
//          StringUtils::isPrefix(key, prefix + "unix://")) {
//        result[key] = it.second;
//      }
//    }
//  }
//
//  return result;
//}

////////////////////////////////////////////////////////////////////////////////
/// @brief return if there is an endpoint with SSL
////////////////////////////////////////////////////////////////////////////////

bool EndpointList::hasSsl() const {
  for (auto& it : _endpoints) {
    std::string const& key = it.first;

    if (StringUtils::isPrefix(key, "http+ssl://")) {
      return true;
    }

    if (StringUtils::isPrefix(key, "vst+ssl://")) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump all endpoints used
////////////////////////////////////////////////////////////////////////////////

void EndpointList::dump() const {
  for (auto& it : _endpoints) {
    Endpoint const* ep = it.second;

    LOG_TOPIC("6ea38", INFO, arangodb::Logger::FIXME)
        << "using endpoint '" << it.first << "' for "
        << encryptionName(ep->encryption()) << " requests";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an encryption name
////////////////////////////////////////////////////////////////////////////////

std::string EndpointList::encryptionName(Endpoint::EncryptionType encryption) {
  switch (encryption) {
    case Endpoint::EncryptionType::SSL:
      return "ssl-encrypted";
    case Endpoint::EncryptionType::NONE:
    default:
      return "non-encrypted";
  }
}
