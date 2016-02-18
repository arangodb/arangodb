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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/AgencyComm.h"
#include "Basics/JsonHelper.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/WriteLocker.h"
#include "Basics/json.h"
#include "Basics/logging.h"
#include "Basics/random.h"
#include "Cluster/ServerState.h"
#include "Rest/Endpoint.h"
#include "Rest/GeneralRequest.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an agency endpoint
////////////////////////////////////////////////////////////////////////////////

AgencyEndpoint::AgencyEndpoint(
    arangodb::rest::Endpoint* endpoint,
    arangodb::httpclient::GeneralClientConnection* connection)
    : _endpoint(endpoint), _connection(connection), _busy(false) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an agency endpoint
////////////////////////////////////////////////////////////////////////////////

AgencyEndpoint::~AgencyEndpoint() {
  delete _connection;
  delete _endpoint;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a communication result
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult::AgencyCommResult()
    : _location(),
      _message(),
      _body(),
      _values(),
      _index(0),
      _statusCode(0),
      _connected(false) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a communication result
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult::~AgencyCommResult() {
  // free all JSON data
  std::map<std::string, AgencyCommResultEntry>::iterator it = _values.begin();

  while (it != _values.end()) {
    if ((*it).second._json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, (*it).second._json);
    }
    ++it;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the connected flag from the result
////////////////////////////////////////////////////////////////////////////////

bool AgencyCommResult::connected() const { return _connected; }

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the http code from the result
////////////////////////////////////////////////////////////////////////////////

int AgencyCommResult::httpCode() const { return _statusCode; }

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the error code from the result
////////////////////////////////////////////////////////////////////////////////

int AgencyCommResult::errorCode() const {
  int result = 0;

  std::unique_ptr<TRI_json_t> json(
      TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, _body.c_str()));

  if (!TRI_IsObjectJson(json.get())) {
    return result;
  }

  // get "errorCode" attribute
  TRI_json_t const* errorCode = TRI_LookupObjectJson(json.get(), "errorCode");

  if (TRI_IsNumberJson(errorCode)) {
    result = (int)errorCode->_value._number;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the error message from the result
/// if there is no error, an empty string will be returned
////////////////////////////////////////////////////////////////////////////////

std::string AgencyCommResult::errorMessage() const {
  std::string result;

  if (!_message.empty()) {
    // return stored message first if set
    return _message;
  }

  if (!_connected) {
    return std::string("unable to connect to agency");
  }

  std::unique_ptr<TRI_json_t> json(
      TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, _body.c_str()));

  if (json == nullptr) {
    return std::string("Out of memory");
  }

  if (!TRI_IsObjectJson(json.get())) {
    return result;
  }

  // get "message" attribute
  TRI_json_t const* message = TRI_LookupObjectJson(json.get(), "message");

  if (TRI_IsStringJson(message)) {
    result = std::string(message->_value._string.data,
                         message->_value._string.length - 1);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the error details from the result
/// if there is no error, an empty string will be returned
////////////////////////////////////////////////////////////////////////////////

std::string AgencyCommResult::errorDetails() const {
  std::string const errorMessage = this->errorMessage();

  if (errorMessage.empty()) {
    return _message;
  }

  return _message + " (" + errorMessage + ")";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the internal result buffer
////////////////////////////////////////////////////////////////////////////////

void AgencyCommResult::clear() {
  // free existing values if any
  std::map<std::string, AgencyCommResultEntry>::iterator it = _values.begin();

  while (it != _values.end()) {
    if ((*it).second._json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, (*it).second._json);
    }
    ++it;
  }

  _values.clear();

  _location = "";
  _message = "";
  _body = "";
  _index = 0;
  _statusCode = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively flatten the JSON response into a map
///
/// stripKeyPrefix is decoded, as is the _globalPrefix
////////////////////////////////////////////////////////////////////////////////

bool AgencyCommResult::parseJsonNode(TRI_json_t const* node,
                                     std::string const& stripKeyPrefix,
                                     bool withDirs) {
  if (!TRI_IsObjectJson(node)) {
    return true;
  }

  // get "key" attribute
  TRI_json_t const* key = TRI_LookupObjectJson(node, "key");

  if (!TRI_IsStringJson(key)) {
    return false;
  }

  std::string keydecoded = std::move(AgencyComm::decodeKey(
      std::string(key->_value._string.data, key->_value._string.length - 1)));

  // make sure we don't strip more bytes than the key is long
  size_t const offset =
      AgencyComm::_globalPrefix.size() + stripKeyPrefix.size();
  size_t const length = keydecoded.size();

  std::string prefix;
  if (offset >= length) {
    prefix = "";
  } else {
    prefix = keydecoded.substr(offset);
  }

  // get "dir" attribute
  TRI_json_t const* dir = TRI_LookupObjectJson(node, "dir");
  bool isDir = (TRI_IsBooleanJson(dir) && dir->_value._boolean);

  if (isDir) {
    if (withDirs) {
      AgencyCommResultEntry entry;

      entry._index = 0;
      entry._json = 0;
      entry._isDir = true;
      _values.emplace(prefix, entry);
    }

    // is a directory, so there may be a "nodes" attribute
    TRI_json_t const* nodes = TRI_LookupObjectJson(node, "nodes");

    if (!TRI_IsArrayJson(nodes)) {
      // if directory is empty...
      return true;
    }

    size_t const n = TRI_LengthVector(&nodes->_value._objects);

    for (size_t i = 0; i < n; ++i) {
      if (!parseJsonNode(
              (TRI_json_t const*)TRI_AtVector(&nodes->_value._objects, i),
              stripKeyPrefix, withDirs)) {
        return false;
      }
    }
  } else {
    // not a directory

    // get "value" attribute
    TRI_json_t const* value = TRI_LookupObjectJson(node, "value");

    if (TRI_IsStringJson(value)) {
      if (!prefix.empty()) {
        AgencyCommResultEntry entry;

        // get "modifiedIndex"
        entry._index =
            arangodb::basics::JsonHelper::stringUInt64(node, "modifiedIndex");
        entry._json = arangodb::basics::JsonHelper::fromString(
            value->_value._string.data, value->_value._string.length - 1);
        entry._isDir = false;

        _values.emplace(prefix, entry);
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// parse an agency result
/// note that stripKeyPrefix is a decoded, normal key!
////////////////////////////////////////////////////////////////////////////////

bool AgencyCommResult::parse(std::string const& stripKeyPrefix, bool withDirs) {
  TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, _body.c_str());

  if (!TRI_IsObjectJson(json)) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    return false;
  }

  // get "node" attribute
  TRI_json_t const* node = TRI_LookupObjectJson(json, "node");

  bool const result = parseJsonNode(node, stripKeyPrefix, withDirs);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return result;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief the static global URL prefix
////////////////////////////////////////////////////////////////////////////////

std::string const AgencyComm::AGENCY_URL_PREFIX = "v2/keys";

////////////////////////////////////////////////////////////////////////////////
/// @brief global (variable) prefix used for endpoint
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::_globalPrefix = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for the global endpoints
////////////////////////////////////////////////////////////////////////////////

arangodb::basics::ReadWriteLock AgencyComm::_globalLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of global endpoints
////////////////////////////////////////////////////////////////////////////////

std::list<AgencyEndpoint*> AgencyComm::_globalEndpoints;

////////////////////////////////////////////////////////////////////////////////
/// @brief global connection options
////////////////////////////////////////////////////////////////////////////////

AgencyConnectionOptions AgencyComm::_globalConnectionOptions = {
    15.0,   // connectTimeout
    120.0,  // requestTimeout
    120.0,  // lockTimeout
    10      // numRetries
};



////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an agency comm locker
////////////////////////////////////////////////////////////////////////////////

AgencyCommLocker::AgencyCommLocker(std::string const& key,
                                   std::string const& type, double ttl)
    : _key(key), _type(type), _json(nullptr), _version(0), _isLocked(false) {
  AgencyComm comm;

  _json =
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, type.c_str(), type.size());

  if (_json == nullptr) {
    return;
  }

  if (comm.lock(key, ttl, 0.0, _json)) {
    fetchVersion(comm);
    _isLocked = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an agency comm locker
////////////////////////////////////////////////////////////////////////////////

AgencyCommLocker::~AgencyCommLocker() {
  unlock();

  if (_json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _json);
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the lock
////////////////////////////////////////////////////////////////////////////////

void AgencyCommLocker::unlock() {
  if (_isLocked) {
    AgencyComm comm;

    updateVersion(comm);
    if (comm.unlock(_key, _json, 0.0)) {
      _isLocked = false;
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief fetch a lock version from the agency
////////////////////////////////////////////////////////////////////////////////

bool AgencyCommLocker::fetchVersion(AgencyComm& comm) {
  if (_type != "WRITE") {
    return true;
  }

  AgencyCommResult result = comm.getValues(_key + "/Version", false);
  if (!result.successful()) {
    if (result.httpCode() != (int)arangodb::rest::GeneralResponse::NOT_FOUND) {
      return false;
    }

    return true;
  }

  result.parse("", false);
  std::map<std::string, AgencyCommResultEntry>::const_iterator it =
      result._values.begin();

  if (it == result._values.end()) {
    return false;
  }

  _version = arangodb::basics::JsonHelper::stringUInt64((*it).second._json);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a lock version in the agency
////////////////////////////////////////////////////////////////////////////////

bool AgencyCommLocker::updateVersion(AgencyComm& comm) {
  if (_type != "WRITE") {
    return true;
  }

  if (_version == 0) {
    TRI_json_t* json =
        arangodb::basics::JsonHelper::uint64String(TRI_UNKNOWN_MEM_ZONE, 1);

    if (json == nullptr) {
      return false;
    }

    // no Version key found, now set it
    AgencyCommResult result =
        comm.casValue(_key + "/Version", json, false, 0.0, 0.0);

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

    return result.successful();
  } else {
    // Version key found, now update it
    TRI_json_t* oldJson = arangodb::basics::JsonHelper::uint64String(
        TRI_UNKNOWN_MEM_ZONE, _version);

    if (oldJson == nullptr) {
      return false;
    }

    TRI_json_t* newJson = arangodb::basics::JsonHelper::uint64String(
        TRI_UNKNOWN_MEM_ZONE, _version + 1);

    if (newJson == nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, oldJson);
      return false;
    }

    AgencyCommResult result =
        comm.casValue(_key + "/Version", oldJson, newJson, 0.0, 0.0);

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, newJson);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, oldJson);

    return result.successful();
  }
}



////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an agency communication object
////////////////////////////////////////////////////////////////////////////////

AgencyComm::AgencyComm(bool addNewEndpoints)
    : _addNewEndpoints(addNewEndpoints) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an agency communication object
////////////////////////////////////////////////////////////////////////////////

AgencyComm::~AgencyComm() {}


////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up all connections
////////////////////////////////////////////////////////////////////////////////

void AgencyComm::cleanup() {
  AgencyComm::disconnect();

  while (true) {
    {
      bool busyFound = false;
      WRITE_LOCKER(AgencyComm::_globalLock);

      std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();

      while (it != _globalEndpoints.end()) {
        AgencyEndpoint* agencyEndpoint = (*it);

        TRI_ASSERT(agencyEndpoint != nullptr);
        if (agencyEndpoint->_busy) {
          busyFound = true;
          ++it;
        } else {
          it = _globalEndpoints.erase(it);
          delete agencyEndpoint;
        }
      }
      if (!busyFound) {
        break;
      }
    }
    usleep(100000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to establish a communication channel
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::tryConnect() {
  {
    WRITE_LOCKER(AgencyComm::_globalLock);
    if (_globalEndpoints.size() == 0) {
      return false;
    }

    std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();

    while (it != _globalEndpoints.end()) {
      AgencyEndpoint* agencyEndpoint = (*it);

      TRI_ASSERT(agencyEndpoint != nullptr);
      TRI_ASSERT(agencyEndpoint->_endpoint != nullptr);
      TRI_ASSERT(agencyEndpoint->_connection != nullptr);

      if (agencyEndpoint->_endpoint->isConnected()) {
        return true;
      }

      agencyEndpoint->_endpoint->connect(
          _globalConnectionOptions._connectTimeout,
          _globalConnectionOptions._requestTimeout);

      if (agencyEndpoint->_endpoint->isConnected()) {
        return true;
      }

      ++it;
    }
  }

  // unable to connect to any endpoint
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnects all communication channels
////////////////////////////////////////////////////////////////////////////////

void AgencyComm::disconnect() {
  WRITE_LOCKER(AgencyComm::_globalLock);

  std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();

  while (it != _globalEndpoints.end()) {
    AgencyEndpoint* agencyEndpoint = (*it);

    TRI_ASSERT(agencyEndpoint != nullptr);
    TRI_ASSERT(agencyEndpoint->_connection != nullptr);
    TRI_ASSERT(agencyEndpoint->_endpoint != nullptr);

    agencyEndpoint->_connection->disconnect();
    agencyEndpoint->_endpoint->disconnect();

    ++it;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an endpoint to the endpoints list
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::addEndpoint(std::string const& endpointSpecification,
                             bool toFront) {
  LOG_TRACE("adding global agency-endpoint '%s'",
            endpointSpecification.c_str());

  {
    WRITE_LOCKER(AgencyComm::_globalLock);

    // check if we already have got this endpoint
    std::list<AgencyEndpoint*>::const_iterator it = _globalEndpoints.begin();

    while (it != _globalEndpoints.end()) {
      AgencyEndpoint const* agencyEndpoint = (*it);

      TRI_ASSERT(agencyEndpoint != nullptr);

      if (agencyEndpoint->_endpoint->getSpecification() ==
          endpointSpecification) {
        // a duplicate. just ignore
        return false;
      }

      ++it;
    }

    // didn't find the endpoint in our list of endpoints, so now create a new
    // one
    for (size_t i = 0; i < NumConnections; ++i) {
      AgencyEndpoint* agencyEndpoint =
          createAgencyEndpoint(endpointSpecification);

      if (agencyEndpoint == nullptr) {
        return false;
      }

      if (toFront) {
        AgencyComm::_globalEndpoints.push_front(agencyEndpoint);
      } else {
        AgencyComm::_globalEndpoints.push_back(agencyEndpoint);
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if an endpoint is present
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::hasEndpoint(std::string const& endpointSpecification) {
  {
    READ_LOCKER(AgencyComm::_globalLock);

    // check if we have got this endpoint
    std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();

    while (it != _globalEndpoints.end()) {
      AgencyEndpoint const* agencyEndpoint = (*it);

      if (agencyEndpoint->_endpoint->getSpecification() ==
          endpointSpecification) {
        return true;
      }

      ++it;
    }
  }

  // not found
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a stringified version of the endpoints
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> AgencyComm::getEndpoints() {
  std::vector<std::string> result;

  {
    // iterate over the list of endpoints
    READ_LOCKER(AgencyComm::_globalLock);

    std::list<AgencyEndpoint*>::const_iterator it =
        AgencyComm::_globalEndpoints.begin();

    while (it != AgencyComm::_globalEndpoints.end()) {
      AgencyEndpoint const* agencyEndpoint = (*it);

      TRI_ASSERT(agencyEndpoint != nullptr);

      result.push_back(agencyEndpoint->_endpoint->getSpecification());
      ++it;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a stringified version of the endpoints
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::getEndpointsString() {
  std::string result;

  {
    // iterate over the list of endpoints
    READ_LOCKER(AgencyComm::_globalLock);

    std::list<AgencyEndpoint*>::const_iterator it =
        AgencyComm::_globalEndpoints.begin();

    while (it != AgencyComm::_globalEndpoints.end()) {
      if (!result.empty()) {
        result += ", ";
      }

      AgencyEndpoint const* agencyEndpoint = (*it);

      TRI_ASSERT(agencyEndpoint != nullptr);

      result.append(agencyEndpoint->_endpoint->getSpecification());
      ++it;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the global prefix for all operations
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::setPrefix(std::string const& prefix) {
  // agency prefix must not be changed
  _globalPrefix = prefix;

  // make sure prefix starts with a forward slash
  if (prefix[0] != '/') {
    _globalPrefix = '/' + _globalPrefix;
  }

  // make sure prefix ends with a forward slash
  if (_globalPrefix.size() > 0) {
    if (_globalPrefix[_globalPrefix.size() - 1] != '/') {
      _globalPrefix += '/';
    }
  }

  LOG_TRACE("setting agency-prefix to '%s'", prefix.c_str());
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the global prefix for all operations
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::prefix() { return _globalPrefix; }

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a timestamp
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::generateStamp() {
  time_t tt = time(0);
  struct tm tb;
  char buffer[21];

  TRI_gmtime(tt, &tb);

  size_t len = ::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);

  return std::string(buffer, len);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new agency endpoint
////////////////////////////////////////////////////////////////////////////////

AgencyEndpoint* AgencyComm::createAgencyEndpoint(
    std::string const& endpointSpecification) {
  std::unique_ptr<arangodb::rest::Endpoint> endpoint(
      arangodb::rest::Endpoint::clientFactory(endpointSpecification));

  if (endpoint == nullptr) {
    // could not create endpoint...
    return nullptr;
  }

  std::unique_ptr<arangodb::httpclient::GeneralClientConnection> connection(
      arangodb::httpclient::GeneralClientConnection::factory(
          endpoint.get(), _globalConnectionOptions._requestTimeout,
          _globalConnectionOptions._connectTimeout,
          _globalConnectionOptions._connectRetries, 0));

  if (connection == nullptr) {
    return nullptr;
  }

  auto ep = new AgencyEndpoint(endpoint.get(), connection.get());
  endpoint.release();
  connection.release();

  return ep;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief sends the current server state to the agency
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::sendServerState(double ttl) {
  // construct JSON value { "status": "...", "time": "..." }
  std::unique_ptr<TRI_json_t> json(
      TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE, 2));

  if (json == nullptr) {
    return AgencyCommResult();
  }

  std::string const status =
      ServerState::stateToString(ServerState::instance()->getState());
  std::string const stamp = std::move(AgencyComm::generateStamp());

  TRI_Insert3ObjectJson(
      TRI_UNKNOWN_MEM_ZONE, json.get(), "status",
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, status.c_str(),
                               status.size()));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json.get(), "time",
                        TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE,
                                                 stamp.c_str(), stamp.size()));

  AgencyCommResult result(
      setValue("Sync/ServerStates/" + ServerState::instance()->getId(),
               json.get(), ttl));

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the backend version
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::getVersion() {
  AgencyCommResult result;

  sendWithFailover(arangodb::rest::GeneralRequest::HTTP_REQUEST_GET,
                   _globalConnectionOptions._requestTimeout, result, "version",
                   "", false);

  if (result.successful()) {
    return result._body;
  }

  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a version number in the agency
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::increaseVersion(std::string const& key) {
  // fetch existing version number
  AgencyCommResult result = getValues(key, false);

  if (!result.successful()) {
    if (result.httpCode() != (int)arangodb::rest::GeneralResponse::NOT_FOUND) {
      return false;
    }

    // no version key found, now set it
    std::unique_ptr<TRI_json_t> json(
        arangodb::basics::JsonHelper::uint64String(TRI_UNKNOWN_MEM_ZONE, 1));

    if (json == nullptr) {
      return false;
    }

    result.clear();
    result = casValue(key, json.get(), false, 0.0, 0.0);

    return result.successful();
  }

  // found a version
  result.parse("", false);
  auto it = result._values.begin();

  if (it == result._values.end()) {
    return false;
  }

  uint64_t version =
      arangodb::basics::JsonHelper::stringUInt64((*it).second._json);

  // version key found, now update it
  std::unique_ptr<TRI_json_t> oldJson(
      arangodb::basics::JsonHelper::uint64String(TRI_UNKNOWN_MEM_ZONE,
                                                 version));

  if (oldJson == nullptr) {
    return false;
  }

  std::unique_ptr<TRI_json_t> newJson(
      arangodb::basics::JsonHelper::uint64String(TRI_UNKNOWN_MEM_ZONE,
                                                 version + 1));

  if (newJson == nullptr) {
    return false;
  }

  result.clear();

  result = casValue(key, oldJson.get(), newJson.get(), 0.0, 0.0);

  return result.successful();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a version number in the agency, retry until it works
////////////////////////////////////////////////////////////////////////////////

void AgencyComm::increaseVersionRepeated(std::string const& key) {
  bool ok = false;
  while (!ok) {
    ok = increaseVersion(key);
    if (ok) {
      return;
    }
    uint32_t val = 300 + TRI_UInt32Random() % 400;
    LOG_INFO("Could not increase %s in agency, retrying in %dms!", key.c_str(),
             val);
    usleep(val * 1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a directory in the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::createDirectory(std::string const& key) {
  AgencyCommResult result;

  sendWithFailover(arangodb::rest::GeneralRequest::HTTP_REQUEST_PUT,
                   _globalConnectionOptions._requestTimeout, result,
                   buildUrl(key) + "?dir=true", "", false);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a value in the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::setValue(std::string const& key,
                                      TRI_json_t const* json, double ttl) {
  AgencyCommResult result;

  sendWithFailover(arangodb::rest::GeneralRequest::HTTP_REQUEST_PUT,
                   _globalConnectionOptions._requestTimeout, result,
                   buildUrl(key) + ttlParam(ttl, true),
                   "value=" + arangodb::basics::StringUtils::urlEncode(
                                  arangodb::basics::JsonHelper::toString(json)),
                   false);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a value in the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::setValue(std::string const& key,
                                      arangodb::velocypack::Slice const json,
                                      double ttl) {
  AgencyCommResult result;

  sendWithFailover(
      arangodb::rest::GeneralRequest::HTTP_REQUEST_PUT,
      _globalConnectionOptions._requestTimeout, result,
      buildUrl(key) + ttlParam(ttl, true),
      "value=" + arangodb::basics::StringUtils::urlEncode(json.toJson()),
      false);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a key exists
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::exists(std::string const& key) {
  std::string url(buildUrl(key));

  AgencyCommResult result;

  sendWithFailover(arangodb::rest::GeneralRequest::HTTP_REQUEST_GET,
                   _globalConnectionOptions._requestTimeout, result, url, "",
                   false);

  return result.successful();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one or multiple values from the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::getValues(std::string const& key, bool recursive) {
  std::string url(buildUrl(key));
  if (recursive) {
    url += "?recursive=true";
  }

  AgencyCommResult result;

  sendWithFailover(arangodb::rest::GeneralRequest::HTTP_REQUEST_GET,
                   _globalConnectionOptions._requestTimeout, result, url, "",
                   false);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes one or multiple values from the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::removeValues(std::string const& key,
                                          bool recursive) {
  std::string url;

  if (key.empty() && recursive) {
    // delete everything, recursive
    url = buildUrl();
  } else {
    url = buildUrl(key);
  }

  if (recursive) {
    url += "?recursive=true";
  }

  AgencyCommResult result;

  sendWithFailover(arangodb::rest::GeneralRequest::HTTP_REQUEST_DELETE,
                   _globalConnectionOptions._requestTimeout, result, url, "",
                   false);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a single value in the backend
/// the CAS condition is whether or not a previous value existed for the key
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::casValue(std::string const& key,
                                      TRI_json_t const* json, bool prevExist,
                                      double ttl, double timeout) {
  AgencyCommResult result;

  sendWithFailover(
      arangodb::rest::GeneralRequest::HTTP_REQUEST_PUT,
      timeout == 0.0 ? _globalConnectionOptions._requestTimeout : timeout,
      result, buildUrl(key) + "?prevExist=" + (prevExist ? "true" : "false") +
                  ttlParam(ttl, false),
      "value=" + arangodb::basics::StringUtils::urlEncode(
                     arangodb::basics::JsonHelper::toString(json)),
      false);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a single value in the backend
/// the CAS condition is whether or not a previous value existed for the key
/// velocypack variant
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::casValue(std::string const& key,
                                      arangodb::velocypack::Slice const json,
                                      bool prevExist, double ttl,
                                      double timeout) {
  AgencyCommResult result;

  sendWithFailover(
      arangodb::rest::GeneralRequest::HTTP_REQUEST_PUT,
      timeout == 0.0 ? _globalConnectionOptions._requestTimeout : timeout,
      result, buildUrl(key) + "?prevExist=" + (prevExist ? "true" : "false") +
                  ttlParam(ttl, false),
      "value=" + arangodb::basics::StringUtils::urlEncode(json.toJson()),
      false);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a single value in the backend
/// the CAS condition is whether or not the previous value for the key was
/// identical to `oldValue`
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::casValue(std::string const& key,
                                      TRI_json_t const* oldJson,
                                      TRI_json_t const* newJson, double ttl,
                                      double timeout) {
  AgencyCommResult result;

  sendWithFailover(
      arangodb::rest::GeneralRequest::HTTP_REQUEST_PUT,
      timeout == 0.0 ? _globalConnectionOptions._requestTimeout : timeout,
      result, buildUrl(key) + "?prevValue=" +
                  arangodb::basics::StringUtils::urlEncode(
                      arangodb::basics::JsonHelper::toString(oldJson)) +
                  ttlParam(ttl, false),
      "value=" + arangodb::basics::StringUtils::urlEncode(
                     arangodb::basics::JsonHelper::toString(newJson)),
      false);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief blocks on a change of a single value in the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::watchValue(std::string const& key,
                                        uint64_t waitIndex, double timeout,
                                        bool recursive) {
  std::string url(buildUrl(key) + "?wait=true");

  if (waitIndex > 0) {
    url += "&waitIndex=" + arangodb::basics::StringUtils::itoa(waitIndex);
  }

  if (recursive) {
    url += "&recursive=true";
  }

  AgencyCommResult result;

  sendWithFailover(
      arangodb::rest::GeneralRequest::HTTP_REQUEST_GET,
      timeout == 0.0 ? _globalConnectionOptions._requestTimeout : timeout,
      result, url, "", true);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquire a read lock
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::lockRead(std::string const& key, double ttl, double timeout) {
  std::unique_ptr<TRI_json_t> json(
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "READ", strlen("READ")));

  if (json == nullptr) {
    return false;
  }

  return lock(key, ttl, timeout, json.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquire a write lock
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::lockWrite(std::string const& key, double ttl, double timeout) {
  std::unique_ptr<TRI_json_t> json(
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "WRITE", strlen("WRITE")));

  if (json == nullptr) {
    return false;
  }

  return lock(key, ttl, timeout, json.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief release a read lock
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::unlockRead(std::string const& key, double timeout) {
  std::unique_ptr<TRI_json_t> json(
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "READ", strlen("READ")));

  if (json == nullptr) {
    return false;
  }

  return unlock(key, json.get(), timeout);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief release a write lock
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::unlockWrite(std::string const& key, double timeout) {
  std::unique_ptr<TRI_json_t> json(
      TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "WRITE", strlen("WRITE")));

  if (json == nullptr) {
    return false;
  }

  return unlock(key, json.get(), timeout);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get unique id
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::uniqid(std::string const& key, uint64_t count,
                                    double timeout) {
  static int const maxTries = 10;
  int tries = 0;

  AgencyCommResult result;

  while (tries++ < maxTries) {
    result.clear();
    result = getValues(key, false);

    if (result.httpCode() == (int)arangodb::rest::GeneralResponse::NOT_FOUND) {
      std::unique_ptr<TRI_json_t> json(
          TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "0", strlen("0")));

      if (json != nullptr) {
        // create the key on the fly
        setValue(key, json.get(), 0.0);
        tries--;

        continue;
      }
    }

    if (!result.successful()) {
      return result;
    }

    result.parse("", false);

    std::unique_ptr<TRI_json_t> oldJson;

    std::map<std::string, AgencyCommResultEntry>::iterator it =
        result._values.begin();

    if (it != result._values.end()) {
      // steal the json
      oldJson.reset((*it).second._json);
      (*it).second._json = nullptr;
    } else {
      oldJson.reset(
          TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "0", strlen("0")));
    }

    if (oldJson == nullptr) {
      return AgencyCommResult();
    }

    uint64_t const oldValue =
        arangodb::basics::JsonHelper::stringUInt64(oldJson.get()) + count;
    uint64_t const newValue = oldValue + count;
    std::unique_ptr<TRI_json_t> newJson(
        arangodb::basics::JsonHelper::uint64String(TRI_UNKNOWN_MEM_ZONE,
                                                   newValue));

    if (newJson == nullptr) {
      return AgencyCommResult();
    }

    result.clear();
    result = casValue(key, oldJson.get(), newJson.get(), 0.0, timeout);

    if (result.successful()) {
      result._index = oldValue + 1;
      break;
    }
  }

  return result;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief creates a ttl query parameter
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::ttlParam(double ttl, bool isFirst) {
  if ((int)ttl <= 0) {
    return "";
  }

  return (isFirst ? "?ttl=" : "&ttl=") +
         arangodb::basics::StringUtils::itoa((int)ttl);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a lock
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::lock(std::string const& key, double ttl, double timeout,
                      TRI_json_t const* json) {
  if (ttl == 0.0) {
    ttl = _globalConnectionOptions._lockTimeout;
  }

  if (timeout == 0.0) {
    timeout = _globalConnectionOptions._lockTimeout;
  }

  unsigned long sleepTime = InitialSleepTime;
  double const end = TRI_microtime() + timeout;

  std::unique_ptr<TRI_json_t> oldJson(TRI_CreateStringCopyJson(
      TRI_UNKNOWN_MEM_ZONE, "UNLOCKED", strlen("UNLOCKED")));

  if (oldJson == nullptr) {
    return false;
  }

  while (true) {
    AgencyCommResult result =
        casValue(key + "/Lock", oldJson.get(), json, ttl, timeout);

    if (!result.successful() &&
        result.httpCode() == (int)arangodb::rest::GeneralResponse::NOT_FOUND) {
      // key does not yet exist. create it now
      result = casValue(key + "/Lock", json, false, ttl, timeout);
    }

    if (result.successful()) {
      return true;
    }

    usleep(sleepTime);

    if (sleepTime < MaxSleepTime) {
      sleepTime += InitialSleepTime;
    }

    if (TRI_microtime() >= end) {
      return false;
    }
  }

  TRI_ASSERT(false);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a lock
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::unlock(std::string const& key, TRI_json_t const* json,
                        double timeout) {
  if (timeout == 0.0) {
    timeout = _globalConnectionOptions._lockTimeout;
  }

  unsigned long sleepTime = InitialSleepTime;
  double const end = TRI_microtime() + timeout;

  std::unique_ptr<TRI_json_t> newJson(TRI_CreateStringCopyJson(
      TRI_UNKNOWN_MEM_ZONE, "UNLOCKED", strlen("UNLOCKED")));

  if (newJson == nullptr) {
    return false;
  }

  while (true) {
    AgencyCommResult result =
        casValue(key + "/Lock", json, newJson.get(), 0.0, timeout);

    if (result.successful()) {
      return true;
    }

    usleep(sleepTime);

    if (sleepTime < MaxSleepTime) {
      sleepTime += InitialSleepTime;
    }

    if (TRI_microtime() >= end) {
      return false;
    }
  }

  TRI_ASSERT(false);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pop an endpoint from the queue
////////////////////////////////////////////////////////////////////////////////

AgencyEndpoint* AgencyComm::popEndpoint(std::string const& endpoint) {
  unsigned long sleepTime = InitialSleepTime;

  while (1) {
    {
      WRITE_LOCKER(AgencyComm::_globalLock);

      size_t const numEndpoints TRI_UNUSED = _globalEndpoints.size();
      std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();

      while (it != _globalEndpoints.end()) {
        AgencyEndpoint* agencyEndpoint = (*it);

        TRI_ASSERT(agencyEndpoint != nullptr);

        if (!endpoint.empty() &&
            agencyEndpoint->_endpoint->getSpecification() != endpoint) {
          // we're looking for a different endpoint
          ++it;
          continue;
        }

        if (!agencyEndpoint->_busy) {
          agencyEndpoint->_busy = true;

          if (AgencyComm::_globalEndpoints.size() > 1) {
            // remove from list
            AgencyComm::_globalEndpoints.remove(agencyEndpoint);
            // and re-insert at end
            AgencyComm::_globalEndpoints.push_back(agencyEndpoint);
          }

          TRI_ASSERT(_globalEndpoints.size() == numEndpoints);

          return agencyEndpoint;
        }

        ++it;
      }
    }

    // if we got here, we ran out of non-busy connections...

    usleep(sleepTime);

    if (sleepTime < MaxSleepTime) {
      sleepTime += InitialSleepTime;
    }
  }

  // just to shut up compilers
  TRI_ASSERT(false);
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reinsert an endpoint into the queue
////////////////////////////////////////////////////////////////////////////////

void AgencyComm::requeueEndpoint(AgencyEndpoint* agencyEndpoint,
                                 bool wasWorking) {
  WRITE_LOCKER(AgencyComm::_globalLock);
  size_t const numEndpoints TRI_UNUSED = _globalEndpoints.size();

  TRI_ASSERT(agencyEndpoint != nullptr);
  TRI_ASSERT(agencyEndpoint->_busy);

  // set to non-busy
  agencyEndpoint->_busy = false;

  if (AgencyComm::_globalEndpoints.size() > 1) {
    if (wasWorking) {
      // remove from list
      AgencyComm::_globalEndpoints.remove(agencyEndpoint);

      // and re-insert at front
      AgencyComm::_globalEndpoints.push_front(agencyEndpoint);
    }
  }

  TRI_ASSERT(_globalEndpoints.size() == numEndpoints);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a URL
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::buildUrl(std::string const& relativePart) const {
  return AgencyComm::AGENCY_URL_PREFIX +
         std::move(encodeKey(_globalPrefix + relativePart));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a URL, without a key
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::buildUrl() const {
  return AgencyComm::AGENCY_URL_PREFIX +
         std::move(
             encodeKey(_globalPrefix.substr(0, _globalPrefix.size() - 1)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends an HTTP request to the agency, handling failover
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::sendWithFailover(
    arangodb::rest::GeneralRequest::RequestType method, double const timeout,
    AgencyCommResult& result, std::string const& url, std::string const& body,
    bool isWatch) {
  size_t numEndpoints;

  {
    READ_LOCKER(AgencyComm::_globalLock);
    numEndpoints = AgencyComm::_globalEndpoints.size();

    if (numEndpoints == 0) {
      return false;
    }
  }

  TRI_ASSERT(numEndpoints > 0);

  size_t tries = 0;
  std::string realUrl = url;
  std::string forceEndpoint;

  while (tries++ < numEndpoints) {
    AgencyEndpoint* agencyEndpoint = popEndpoint(forceEndpoint);

    TRI_ASSERT(agencyEndpoint != nullptr);

    try {
      send(agencyEndpoint->_connection, method, timeout, result, realUrl, body);
    } catch (...) {
      result._connected = false;
      result._statusCode = 0;
      result._message = "could not send request to agency";

      agencyEndpoint->_connection->disconnect();

      requeueEndpoint(agencyEndpoint, true);
      return false;
    }

    if (result._statusCode ==
        (int)arangodb::rest::GeneralResponse::TEMPORARY_REDIRECT) {
      // sometimes the agency will return a 307 (temporary redirect)
      // in this case we have to pick it up and use the new location returned

      // put the current connection to the end of the list
      requeueEndpoint(agencyEndpoint, false);

      // a 307 does not count as a success
      TRI_ASSERT(!result.successful());

      std::string endpoint;

      // transform location into an endpoint
      if (result.location().substr(0, 7) == "http://") {
        endpoint = "tcp://" + result.location().substr(7);
      } else if (result.location().substr(0, 8) == "https://") {
        endpoint = "ssl://" + result.location().substr(8);
      } else {
        // invalid endpoint, return an error
        return false;
      }

      size_t const delim = endpoint.find('/', 6);

      if (delim == std::string::npos) {
        // invalid location header
        return false;
      }

      realUrl = endpoint.substr(delim);
      endpoint = endpoint.substr(0, delim);

      if (!AgencyComm::hasEndpoint(endpoint)) {
        // redirection to an unknown endpoint
        if (_addNewEndpoints) {
          AgencyComm::addEndpoint(endpoint, true);

          LOG_INFO("adding agency-endpoint '%s'", endpoint.c_str());

          // re-check the new endpoint
          if (AgencyComm::hasEndpoint(endpoint)) {
            ++numEndpoints;
            continue;
          }
        }

        LOG_ERROR(
            "found redirection to unknown endpoint '%s'. Will not follow!",
            endpoint.c_str());

        // this is an error
        return false;
      }

      forceEndpoint = endpoint;

      // if we get here, we'll just use the next endpoint from the list
      continue;
    }

    forceEndpoint = "";

    // we can stop iterating over endpoints if the operation succeeded,
    // if a watch timed out or
    // if the reason for failure was a client-side error
    bool const canAbort =
        result.successful() || (isWatch && result._statusCode == 0) ||
        (result._statusCode >= 400 && result._statusCode <= 499);

    requeueEndpoint(agencyEndpoint, canAbort);

    if (canAbort) {
      // we're done
      return true;
    }

    // otherwise, try next
  }

  // if we get here, we could not send data to any endpoint successfully
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends data to the URL
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::send(arangodb::httpclient::GeneralClientConnection* connection,
                      arangodb::rest::GeneralRequest::RequestType method,
                      double timeout, AgencyCommResult& result,
                      std::string const& url, std::string const& body) {
  TRI_ASSERT(connection != nullptr);

  if (method == arangodb::rest::GeneralRequest::HTTP_REQUEST_GET ||
      method == arangodb::rest::GeneralRequest::HTTP_REQUEST_HEAD ||
      method == arangodb::rest::GeneralRequest::HTTP_REQUEST_DELETE) {
    TRI_ASSERT(body.empty());
  }

  TRI_ASSERT(!url.empty());

  result._connected = false;
  result._statusCode = 0;

  LOG_TRACE("sending %s request to agency at endpoint '%s', url '%s': %s",
            arangodb::rest::GeneralRequest::translateMethod(method).c_str(),
            connection->getEndpoint()->getSpecification().c_str(), url.c_str(),
            body.c_str());

  arangodb::httpclient::SimpleHttpClient client(connection, timeout, false);

  client.keepConnectionOnDestruction(true);

  // set up headers
  std::map<std::string, std::string> headers;
  if (method == arangodb::rest::GeneralRequest::HTTP_REQUEST_PUT ||
      method == arangodb::rest::GeneralRequest::HTTP_REQUEST_POST) {
    // the agency needs this content-type for the body
    headers["content-type"] = "application/x-www-form-urlencoded";
  }

  // send the actual request
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(method, url, body.c_str(), body.size(), headers));

  if (response == nullptr) {
    connection->disconnect();
    result._message = "could not send request to agency";
    LOG_TRACE("sending request to agency failed");

    return false;
  }

  if (!response->isComplete()) {
    connection->disconnect();
    result._message = "sending request to agency failed";
    LOG_TRACE("sending request to agency failed");

    return false;
  }

  result._connected = true;

  if (response->getHttpReturnCode() ==
      (int)arangodb::rest::GeneralResponse::TEMPORARY_REDIRECT) {
    // temporary redirect. now save location header

    bool found = false;
    result._location = response->getHeaderField("location", found);

    LOG_TRACE("redirecting to location: '%s'", result._location.c_str());

    if (!found) {
      // a 307 without a location header does not make any sense
      connection->disconnect();
      result._message = "invalid agency response (header missing)";

      return false;
    }
  }

  result._message = response->getHttpReturnMessage();
  basics::StringBuffer& sb = response->getBody();
  result._body = std::string(sb.c_str(), sb.length());
  result._index = 0;
  result._statusCode = response->getHttpReturnCode();

  bool found = false;
  std::string lastIndex = response->getHeaderField("x-etcd-index", found);
  if (found) {
    result._index = arangodb::basics::StringUtils::uint64(lastIndex);
  }

  LOG_TRACE(
      "request to agency returned status code %d, message: '%s', body: '%s'",
      result._statusCode, result._message.c_str(), result._body.c_str());

  if (result.successful()) {
    return true;
  }

  connection->disconnect();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief encode a key for etcd
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::encodeKey(std::string const& s) {
  std::string::const_iterator i;
  std::string res;
  for (i = s.begin(); i != s.end(); ++i) {
    if (*i == '_') {
      res.push_back('@');
      res.push_back('U');
    } else if (*i == '@') {
      res.push_back('@');
      res.push_back('@');
    } else {
      res.push_back(*i);
    }
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decode a key for etcd
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::decodeKey(std::string const& s) {
  std::string::const_iterator i;
  std::string res;
  for (i = s.begin(); i != s.end(); ++i) {
    if (*i == '@') {
      ++i;
      if (*i == 'U') {
        res.push_back('_');
      } else {
        res.push_back('@');
      }
    } else {
      res.push_back(*i);
    }
  }
  return res;
}


