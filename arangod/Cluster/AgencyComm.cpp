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

#include "AgencyComm.h"

#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Sink.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ReadLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/json.h"
#include "Cluster/ServerState.h"
#include "Endpoint/Endpoint.h"
#include "Logger/Logger.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"


using namespace arangodb;

static void addEmptyVPackObject(std::string const& name, VPackBuilder& builder) {
  builder.add(VPackValue(name));
  VPackObjectBuilder c(&builder);
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief constructs an operation
//////////////////////////////////////////////////////////////////////////////

AgencyOperation::AgencyOperation(std::string const& key, AgencySimpleOperationType opType)
    : _key(AgencyComm::prefix() + key), _opType() {
  _opType.type = AgencyOperationType::SIMPLE;
  _opType.simple = opType;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief constructs an operation
//////////////////////////////////////////////////////////////////////////////

AgencyOperation::AgencyOperation(std::string const& key, AgencyValueOperationType opType,
    VPackSlice value)
    : _key(AgencyComm::prefix() + key), _opType(), _value(value) {
  _opType.type = AgencyOperationType::VALUE;
  _opType.value = opType;
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief adds the operation formatted as an attribute in a vpack object
//////////////////////////////////////////////////////////////////////////////

void AgencyOperation::toVelocyPack(VPackBuilder& builder) const {
  builder.add(VPackValue(_key));
  {
    VPackObjectBuilder valueOperation(&builder);
    builder.add("op", VPackValue(_opType.toString()));
    if (_opType.type == AgencyOperationType::VALUE) {
      if (_opType.value == AgencyValueOperationType::OBSERVE
          || _opType.value == AgencyValueOperationType::UNOBSERVE) {
        builder.add("url", _value);
      } else {
        builder.add("new", _value);
      }
      if (_ttl > 0) {
        builder.add("ttl", VPackValue(_ttl));
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief constructs a precondition
//////////////////////////////////////////////////////////////////////////////

AgencyPrecondition::AgencyPrecondition(std::string const& key, Type t, bool e)
    : key(AgencyComm::prefix() + key), type(t), empty(e) {
}

//////////////////////////////////////////////////////////////////////////////
/// @brief constructs a precondition
//////////////////////////////////////////////////////////////////////////////

AgencyPrecondition::AgencyPrecondition(std::string const& key, Type t,
                                       VPackSlice s)
    : key(AgencyComm::prefix() + key), type(t), empty(false), value(s) {
}

//////////////////////////////////////////////////////////////////////////////
/// @brief adds the precondition formatted as an attribute in a vpack obj
//////////////////////////////////////////////////////////////////////////////

void AgencyPrecondition::toVelocyPack(VPackBuilder& builder) const {
  if (type != AgencyPrecondition::NONE) {
    builder.add(VPackValue(key));
    {
      VPackObjectBuilder preconditionDefinition(&builder);
      switch(type) {
        case AgencyPrecondition::EMPTY:
          builder.add("oldEmpty", VPackValue(empty));
          break;
        case AgencyPrecondition::VALUE:
          builder.add("old", value);
          break;
        // mop: useless compiler warning :S
        case AgencyPrecondition::NONE:
          break;
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief converts the transaction to json
//////////////////////////////////////////////////////////////////////////////

std::string AgencyWriteTransaction::toJson() const {
  VPackBuilder builder;
  {
    VPackArrayBuilder guard(&builder);
    toVelocyPack(builder);
  }
  return builder.toJson();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief converts the transaction to velocypack
//////////////////////////////////////////////////////////////////////////////

void AgencyWriteTransaction::toVelocyPack(VPackBuilder& builder) const {
  VPackArrayBuilder guard(&builder);
  {
    VPackObjectBuilder guard2(&builder);
    for (AgencyOperation const& operation: operations) {
      operation.toVelocyPack(builder);
    }
  }
  if (preconditions.size() > 0) {
    VPackObjectBuilder guard3(&builder);
    for (AgencyPrecondition const& precondition: preconditions) {
      precondition.toVelocyPack(builder);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief converts the read transaction to json
//////////////////////////////////////////////////////////////////////////////

std::string AgencyReadTransaction::toJson() const {
  VPackBuilder builder;
  {
    VPackArrayBuilder guard(&builder);
    toVelocyPack(builder);
  }
  return builder.toJson();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief converts the read transaction to velocypack
//////////////////////////////////////////////////////////////////////////////

void AgencyReadTransaction::toVelocyPack(VPackBuilder& builder) const {
  VPackArrayBuilder guard(&builder);
  {
    VPackArrayBuilder guard2(&builder);
    for (std::string const& key: keys) {
      builder.add(VPackValue(key));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an agency endpoint
////////////////////////////////////////////////////////////////////////////////

AgencyEndpoint::AgencyEndpoint(
    Endpoint* endpoint,
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
  // All elements free themselves
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
  try {
    std::shared_ptr<VPackBuilder> bodyBuilder =
        VPackParser::fromJson(_body.c_str());
    VPackSlice body = bodyBuilder->slice();
    if (!body.isObject()) {
      return 0;
    }
    // get "errorCode" attribute (0 if not exist)
    return arangodb::basics::VelocyPackHelper::getNumericValue<int>(
        body, "errorCode", 0);
  } catch (VPackException const&) {
    return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the error message from the result
/// if there is no error, an empty string will be returned
////////////////////////////////////////////////////////////////////////////////

std::string AgencyCommResult::errorMessage() const {
  if (!_message.empty()) {
    // return stored message first if set
    return _message;
  }

  if (!_connected) {
    return std::string("unable to connect to agency");
  }

  try {
    std::shared_ptr<VPackBuilder> bodyBuilder =
        VPackParser::fromJson(_body.c_str());

    VPackSlice body = bodyBuilder->slice();
    if (!body.isObject()) {
      return "";
    }
    // get "message" attribute ("" if not exist)
    return arangodb::basics::VelocyPackHelper::getStringValue(body, "message",
                                                              "");
  } catch (VPackException const& e) {
    std::string message("VPackException parsing body ("+ _body + "): "
                        + e.what());
    return std::string(message);
  }
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
  // clear existing values. They free themselves
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

bool AgencyCommResult::parseVelocyPackNode(VPackSlice const& node,
                                           std::string const& stripKeyPrefix,
                                           bool withDirs) {
  if (!node.isObject()) {
    return true;
  }

  // get "key" attribute
  VPackSlice const key = node.get("key");

  if (!key.isString()) {
    return false;
  }

  std::string keydecoded = key.copyString();

  // make sure we don't strip more bytes than the key is long
  size_t const offset =
      AgencyComm::_globalPrefix.size() + stripKeyPrefix.size();
  size_t const length = keydecoded.size();

  std::string prefix;
  if (offset < length) {
    prefix = keydecoded.substr(offset);
  }

  // get "dir" attribute
  bool isDir =
      arangodb::basics::VelocyPackHelper::getBooleanValue(node, "dir", false);

  if (isDir) {
    if (withDirs) {
      AgencyCommResultEntry entry;

      entry._index = 0;
      entry._vpack = std::make_shared<VPackBuilder>();
      entry._isDir = true;
      _values.emplace(prefix, entry);
    }

    // is a directory, so there may be a "nodes" attribute
    if (!node.hasKey("nodes")) {
      // if directory is empty...
      return true;
    }
    VPackSlice const nodes = node.get("nodes");

    if (!nodes.isArray()) {
      // if directory is empty...
      return true;
    }

    for (auto const& subNode : VPackArrayIterator(nodes)) {
      if (!parseVelocyPackNode(subNode, stripKeyPrefix, withDirs)) {
        return false;
      }
    }
  } else {
    // not a directory

    // get "value" attribute
    VPackSlice const value = node.get("value");

    if (!prefix.empty()) {
      AgencyCommResultEntry entry;
      
      // get "modifiedIndex"
      entry._index = arangodb::basics::VelocyPackHelper::stringUInt64(
        node.get("modifiedIndex"));
      entry._vpack = std::make_shared<VPackBuilder>();
      entry._isDir = false;
      
      entry._vpack->add(value);
      
      _values.emplace(prefix, entry);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// parse an agency result
/// note that stripKeyPrefix is a decoded, normal key!
////////////////////////////////////////////////////////////////////////////////

bool AgencyCommResult::parse(std::string const& stripKeyPrefix, bool withDirs) {
  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = VPackParser::fromJson(_body.c_str());
  } catch (...) {
    return false;
  }
  
  VPackSlice slice = parsedBody->slice();
  
  if (!slice.isObject()) {
    return false;
  }

  // get "node" attribute
  VPackSlice const node = slice.get("node");

  bool const result = parseVelocyPackNode(node, stripKeyPrefix, withDirs);

  return result;
}

VPackSlice AgencyCommResult::parse(std::string const& path) {
  std::vector<std::string> pv (1,AgencyComm::prefixStripped());
  std::vector<std::string> tmp = basics::StringUtils::split(path,'/');
  pv.insert(pv.end(), tmp.begin(), tmp.end());
  return _vpack->slice()[0].get(pv);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief the static global URL prefix
////////////////////////////////////////////////////////////////////////////////

std::string const AgencyComm::AGENCY_URL_PREFIX = "_api/agency";

////////////////////////////////////////////////////////////////////////////////
/// @brief global (variable) prefix used for endpoint
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::_globalPrefix = "";
std::string AgencyComm::_globalPrefixStripped = "";

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
                                   std::string const& type,
                                   double ttl, double timeout)
    : _key(key), _type(type), _isLocked(false) {
  AgencyComm comm;

  _vpack = std::make_shared<VPackBuilder>();
  try {
    _vpack->add(VPackValue(type));
  } catch (...) {
    return;
  }
  
  if (comm.lock(key, ttl, timeout, _vpack->slice())) {
    _isLocked = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an agency comm locker
////////////////////////////////////////////////////////////////////////////////

AgencyCommLocker::~AgencyCommLocker() {
  unlock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the lock
////////////////////////////////////////////////////////////////////////////////

void AgencyCommLocker::unlock() {
  if (_isLocked) {
    AgencyComm comm;

    updateVersion(comm);
    if (comm.unlock(_key, _vpack->slice(), 0.0)) {
      _isLocked = false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a lock version in the agency
////////////////////////////////////////////////////////////////////////////////

bool AgencyCommLocker::updateVersion(AgencyComm& comm) {
  if (_type != "WRITE") {
    return true;
  }
  AgencyCommResult result = comm.increment(_key  + "/Version");

  return result.successful();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up all connections
////////////////////////////////////////////////////////////////////////////////

void AgencyComm::cleanup() {
  AgencyComm::disconnect();

  while (true) {
    {
      bool busyFound = false;
      WRITE_LOCKER(writeLocker, AgencyComm::_globalLock);

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
    std::string endpointsStr{getUniqueEndpointsString()};

    WRITE_LOCKER(writeLocker, AgencyComm::_globalLock);
    if (_globalEndpoints.empty()) {
      return false;
    }

    // mop: not sure if a timeout makes sense here
    while (true) {
      LOG(INFO) << "Trying to find an active agency. Checking " << endpointsStr;
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
      sleep(1);
    }
  }

  // unable to connect to any endpoint
  return false;
}
//////////////////////////////////////////////////////////////////////////////
/// @brief will try to initialize a new agency
//////////////////////////////////////////////////////////////////////////////

bool AgencyComm::initialize() {
  if (!AgencyComm::tryConnect()) {
    return false;
  }
  AgencyComm comm;
  return comm.ensureStructureInitialized();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief will try to initialize a new agency
//////////////////////////////////////////////////////////////////////////////

bool AgencyComm::tryInitializeStructure() { 
  VPackBuilder builder;
  try {
    VPackObjectBuilder b(&builder);
    builder.add(VPackValue("Sync"));
    {
      VPackObjectBuilder c(&builder);
      builder.add("LatestID", VPackValue("1"));
      addEmptyVPackObject("Problems", builder);
      builder.add("UserVersion", VPackValue(1));
      addEmptyVPackObject("ServerStates", builder);
      builder.add("HeartbeatIntervalMs", VPackValue(1000));
      addEmptyVPackObject("Commands", builder);
    }
    builder.add(VPackValue("Current"));
    {
      VPackObjectBuilder c(&builder);
      builder.add(VPackValue("Collections"));
      {
        VPackObjectBuilder d(&builder);
        addEmptyVPackObject("_system", builder);
      }
      builder.add("Version", VPackValue(1));
      addEmptyVPackObject("ShardsCopied", builder);
      addEmptyVPackObject("NewServers", builder);
      addEmptyVPackObject("Coordinators", builder);
      builder.add("Lock", VPackValue("UNLOCKED"));
      addEmptyVPackObject("DBServers", builder);
      builder.add(VPackValue("ServersRegistered"));
      {
        VPackObjectBuilder c(&builder);
        builder.add("Version", VPackValue("1"));
      }
      addEmptyVPackObject("Databases", builder);
    }
    builder.add(VPackValue("Plan"));
    {
      VPackObjectBuilder c(&builder);
      addEmptyVPackObject("Coordinators", builder);
      builder.add(VPackValue("Databases"));
      {
        VPackObjectBuilder d(&builder);
        builder.add(VPackValue("_system"));
        {
          VPackObjectBuilder d2(&builder);
          builder.add("name", VPackValue("_system"));
          builder.add("id", VPackValue("1"));
        }
//KV        builder.add("_system",
        //                VPackValue("{name:_system, id:1}"));
      }
      builder.add("Lock", VPackValue("UNLOCKED"));
      addEmptyVPackObject("DBServers", builder);
      builder.add("Version", VPackValue(1));
      builder.add(VPackValue("Collections"));
      {
        VPackObjectBuilder d(&builder);
        addEmptyVPackObject("_system", builder);
      }
    }
    builder.add(VPackValue("Target"));
    {
      VPackObjectBuilder c(&builder);
      addEmptyVPackObject("Coordinators", builder);
      builder.add(VPackValue("Collections"));
      {
        VPackObjectBuilder d(&builder);
        addEmptyVPackObject("_system", builder);
      }
      builder.add("Version", VPackValue(1));
      addEmptyVPackObject("MapLocalToID", builder);
      builder.add(VPackValue("Databases"));
      {
        VPackObjectBuilder d(&builder);
        builder.add(VPackValue("_system"));
        {
          VPackObjectBuilder d2(&builder);
          builder.add("name", VPackValue("_system"));
          builder.add("id", VPackValue("1"));
        }
//KV        builder.add("_system",
        //                VPackValue("{name:_system, id:1}"));
      }
      addEmptyVPackObject("DBServers", builder);
      builder.add("Lock", VPackValue("UNLOCKED"));
    }
    builder.add("InitDone", VPackValue(true));
  } catch (...) {
    LOG_TOPIC(ERR, Logger::STARTUP) << "Couldn't create initializing structure";
    return false;
  }

  try {
    LOG_TOPIC(TRACE, Logger::STARTUP) 
        << "Initializing agency with " << builder.toJson();

    AgencyOperation initOperation("", AgencyValueOperationType::SET, builder.slice());

    AgencyWriteTransaction initTransaction;
    initTransaction.operations.push_back(initOperation);
  
    auto result = sendTransactionWithFailover(initTransaction);
    
    return result.successful();
  } catch (std::exception const& e) {
    LOG(FATAL) << "Fatal error initializing agency " << e.what();
    FATAL_ERROR_EXIT();
  } catch (...) {
    LOG(FATAL) << "Fatal error initializing agency";
    FATAL_ERROR_EXIT();
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief checks if we are responsible for initializing the agency
//////////////////////////////////////////////////////////////////////////////

bool AgencyComm::shouldInitializeStructure() {
  VPackBuilder builder;
  builder.add(VPackValue(false));

  double timeout = _globalConnectionOptions._requestTimeout;
  // "InitDone" key should not previously exist
  auto result = casValue("InitDone", builder.slice(), false, 60.0, timeout);
    
  if (!result.successful() &&
      result.httpCode() ==
          (int)arangodb::GeneralResponse::ResponseCode::PRECONDITION_FAILED) {
    // somebody else has or is initializing the agency
    LOG_TOPIC(TRACE, Logger::STARTUP) 
        << "someone else is initializing the agency";
    return false;
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief will initialize agency if it is freshly started
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::ensureStructureInitialized() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "Checking if agency is initialized";

  while (true) {
    while (shouldInitializeStructure()) {
      LOG_TOPIC(TRACE, Logger::STARTUP) 
          << "Agency is fresh. Needs initial structure.";
      // mop: we initialized it .. great success
      if (tryInitializeStructure()) {
        LOG_TOPIC(TRACE, Logger::STARTUP) << "Successfully initialized agency";
        break;
      } 

      LOG_TOPIC(WARN, Logger::STARTUP) 
          << "Initializing agency failed. We'll try again soon";
      // We should really have exclusive access, here, this is strange!
      sleep(1);
    }
    
    AgencyCommResult result = getValues("InitDone", false);
 
    if (result.successful()) {
      result.parse("", false);

      std::map<std::string, AgencyCommResultEntry>::iterator it =
          result._values.begin();
      if (it != result._values.end()) {
        auto value = (*it).second._vpack;

        if (value->slice().isBoolean() && value->slice().getBoolean()) {
          // expecting a value of "true"
          LOG_TOPIC(TRACE, Logger::STARTUP) << "Found an initialized agency";
          return true;
        }
        // fallthrough to sleeping
      }
    }
          
    LOG_TOPIC(TRACE, Logger::STARTUP) 
        << "Waiting for agency to get initialized";

    sleep(1);
  } // next attempt
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnects all communication channels
////////////////////////////////////////////////////////////////////////////////

void AgencyComm::disconnect() {
  WRITE_LOCKER(writeLocker, AgencyComm::_globalLock);

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
  LOG(TRACE) << "adding global agency-endpoint '" << endpointSpecification
             << "'";

  {
    WRITE_LOCKER(writeLocker, AgencyComm::_globalLock);

    // check if we already have got this endpoint
    std::list<AgencyEndpoint*>::const_iterator it = _globalEndpoints.begin();

    while (it != _globalEndpoints.end()) {
      AgencyEndpoint const* agencyEndpoint = (*it);

      TRI_ASSERT(agencyEndpoint != nullptr);

      if (agencyEndpoint->_endpoint->specification() == endpointSpecification) {
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
    READ_LOCKER(readLocker, AgencyComm::_globalLock);

    // check if we have got this endpoint
    std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();

    while (it != _globalEndpoints.end()) {
      AgencyEndpoint const* agencyEndpoint = (*it);
      
      if (agencyEndpoint->_endpoint->specification() == endpointSpecification) {
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
    READ_LOCKER(readLocker, AgencyComm::_globalLock);

    std::list<AgencyEndpoint*>::const_iterator it =
        AgencyComm::_globalEndpoints.begin();

    while (it != AgencyComm::_globalEndpoints.end()) {
      AgencyEndpoint const* agencyEndpoint = (*it);

      TRI_ASSERT(agencyEndpoint != nullptr);

      result.push_back(agencyEndpoint->_endpoint->specification());
      ++it;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a stringified version of all endpoints (unique)
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::getUniqueEndpointsString() {
  std::string result;

  {
    // iterate over the list of endpoints
    READ_LOCKER(readLocker, AgencyComm::_globalLock);

    std::list<AgencyEndpoint*> uniqueEndpoints{
        AgencyComm::_globalEndpoints.begin(),
        AgencyComm::_globalEndpoints.end()};

    uniqueEndpoints.unique([](AgencyEndpoint* a, AgencyEndpoint* b) {
      return a->_endpoint->specification() == b->_endpoint->specification();
    });

    std::list<AgencyEndpoint*>::const_iterator it = uniqueEndpoints.begin();

    while (it != uniqueEndpoints.end()) {
      if (!result.empty()) {
        result += ", ";
      }

      AgencyEndpoint const* agencyEndpoint = (*it);

      TRI_ASSERT(agencyEndpoint != nullptr);

      result.append(agencyEndpoint->_endpoint->specification());
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
    READ_LOCKER(readLocker, AgencyComm::_globalLock);

    std::list<AgencyEndpoint*>::const_iterator it =
        AgencyComm::_globalEndpoints.begin();

    while (it != AgencyComm::_globalEndpoints.end()) {
      if (!result.empty()) {
        result += ", ";
      }

      AgencyEndpoint const* agencyEndpoint = (*it);

      TRI_ASSERT(agencyEndpoint != nullptr);

      result.append(agencyEndpoint->_endpoint->specification());
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
  _globalPrefixStripped = prefix;
  
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

  LOG(TRACE) << "setting agency-prefix to '" << prefix << "'";
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the global prefix for all operations
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::prefix() { return _globalPrefix; }
std::string AgencyComm::prefixStripped() { return _globalPrefixStripped; }

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
  std::unique_ptr<Endpoint> endpoint(
      Endpoint::clientFactory(endpointSpecification));

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
  VPackBuilder builder;
  try {
    builder.openObject();
    std::string const status =
        ServerState::stateToString(ServerState::instance()->getState());
    builder.add("status", VPackValue(status));
    std::string const stamp = AgencyComm::generateStamp();
    builder.add("time", VPackValue(stamp));
    builder.close();
  } catch (...) {
    return AgencyCommResult();
  }

  AgencyCommResult result(
      setValue("Sync/ServerStates/" + ServerState::instance()->getId(),
               builder.slice(), ttl));

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the backend version
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::getVersion() {
  AgencyCommResult result
      =sendWithFailover(arangodb::GeneralRequest::RequestType::GET,
			_globalConnectionOptions._requestTimeout, "version",
			"", false);

  if (result.successful()) {
    return result._body;
  }

  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a directory in the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::createDirectory(std::string const& key) {
  VPackBuilder builder;
  {
    VPackObjectBuilder dir(&builder);
  }
  
  AgencyOperation operation(key, AgencyValueOperationType::SET, builder.slice());
  AgencyWriteTransaction transaction(operation);

  return sendTransactionWithFailover(transaction);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a value in the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::setValue(std::string const& key,
                                      std::string const& value,
                                      double ttl) {
  VPackBuilder builder;
  builder.add(VPackValue(value));
  
  AgencyOperation operation(key, AgencyValueOperationType::SET, builder.slice());
  operation._ttl = static_cast<uint32_t>(ttl);
  AgencyWriteTransaction transaction(operation);

  return sendTransactionWithFailover(transaction);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a value in the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::setValue(std::string const& key,
                                      arangodb::velocypack::Slice const& slice,
                                      double ttl) {

  AgencyOperation operation(key, AgencyValueOperationType::SET, slice);
  operation._ttl = static_cast<uint32_t>(ttl);
  AgencyWriteTransaction transaction(operation);

  return sendTransactionWithFailover(transaction);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a key exists
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::exists(std::string const& key) {
  AgencyCommResult result = getValues2(key);

  return result.successful();  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increment a key
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::increment(std::string const& key) {

  AgencyWriteTransaction transaction(
      AgencyOperation(key, AgencySimpleOperationType::INCREMENT_OP)
  );

  return sendTransactionWithFailover(transaction);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one or multiple values from the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::getValues(std::string const& key, bool recursive) {
  std::string url(buildUrl());
  
  url += "/read";
  VPackBuilder builder;
  {
    VPackArrayBuilder root(&builder);
    {
      VPackArrayBuilder keys(&builder);
      builder.add(VPackValue(AgencyComm::prefix() + key));
    }
  }

  AgencyCommResult result
      = sendWithFailover(arangodb::GeneralRequest::RequestType::POST,
			 _globalConnectionOptions._requestTimeout, url,
			 builder.toJson(), false);

  if (!result.successful()) {
    return result;
  }
  
  try {
    std::shared_ptr<VPackBuilder> parsedBody;
    std::string const body = result.body();
   
    parsedBody = VPackParser::fromJson(body.c_str());
    VPackSlice agencyResult = parsedBody->slice();
    VPackSlice resultNode;
    if (!agencyResult.isArray()) {
      result._statusCode = 500;
      return result;
    }

    if (agencyResult.length() != 1) {
      result._statusCode = 500;
      return result;
    }
    resultNode = agencyResult.at(0);
    
    std::function<void (std::string const&, VPackSlice, VPackBuilder&, int)>
      fakeEtcdNode = [&] (std::string const& key, VPackSlice node,
                          VPackBuilder& builder, int level) {
      VPackObjectBuilder nodeValue(&builder);
      builder.add("key", VPackValue(key));
      builder.add("modifiedIndex", VPackValue(1));
      builder.add("createdIndex", VPackValue(1));
      if (node.isObject()) {
        builder.add("dir", VPackValue(true));

        if (node.length() > 0 && (recursive || level < 1)) {
          builder.add(VPackValue("nodes"));
          VPackArrayBuilder objectStructure(&builder);
          for (auto const& it : VPackObjectIterator(node)) {
            std::string subKey;
            subKey = key + "/" + it.key.copyString();
            fakeEtcdNode(subKey, it.value, builder, level + 1);
          }
        }
      } else if (node.isArray()) {
        LOG(ERR) << node.toJson();
        LOG(ERR) << "Oops...TODO array unexpected";
      } else {
        builder.add("value", node);
      }
    };
    
    VPackBuilder builder;
    {
      VPackObjectBuilder nodeRoot(&builder);
      builder.add("action", VPackValue("get"));
      builder.add(VPackValue("node"));

      const std::string fullKey = AgencyComm::prefix() + key;
      // mop: need to remove all parents... key requested: /arango/hans/mann/wurst.
      // instead of just the result of wurst we will get the full tree
      // but only if there is something inside this object

      TRI_ASSERT(fullKey.size() > 0);
      //TRI_ASSERT(fullkey[0] == '/');
      size_t currentKeyStart = fullKey.size() > 1 ? 1 : std::string::npos;
      while (currentKeyStart != std::string::npos) {
        // at least one further step to go down
        size_t found = fullKey.find_first_of('/', currentKeyStart);
        std::string currentKey 
            = (found == std::string::npos) ?
              fullKey.substr(currentKeyStart) :
              fullKey.substr(currentKeyStart, found - currentKeyStart);
        if (!resultNode.isObject() || !resultNode.hasKey(currentKey)) {
          result.clear();
          result._statusCode = 404;
          return result;
        }
        resultNode = resultNode.get(currentKey);

        currentKeyStart 
            = (found == std::string::npos) ? found : found + 1;
      }
      
      fakeEtcdNode(AgencyComm::prefix() + key, resultNode, builder, 0);
    }
    
    result._body.clear();
    VPackSlice s = builder.slice();
    
    VPackOptions dumperOptions;
    VPackStringSink sink(&result._body);
    VPackDumper::dump(s, &sink, &dumperOptions);

    LOG(TRACE) << "Created fake etcd node" << result._body;
    result._statusCode = 200;
  } catch(std::exception &e) {
    LOG(ERR) << "Error transforming result. " << e.what();
    result.clear();
  } catch(...) {
    LOG(ERR) << "Error transforming result. Out of memory";
    result.clear();
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one or multiple values from the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::getValues2(std::string const& key) {
  std::string url(buildUrl());
  
  url += "/read";
  VPackBuilder builder;
  {
    VPackArrayBuilder root(&builder);
    {
      VPackArrayBuilder keys(&builder);
      builder.add(VPackValue(AgencyComm::prefix() + key));
    }
  }

  AgencyCommResult result
      = sendWithFailover(arangodb::GeneralRequest::RequestType::POST,
                         _globalConnectionOptions._requestTimeout, url,
                         builder.toJson(), false);

  if (!result.successful()) {
    return result;
  }
  
  try {

    result._vpack = VPackParser::fromJson(result.body().c_str());

    if (!result._vpack->slice().isArray()) {
      result._statusCode = 500;
      return result;
    }

    if (result._vpack->slice().length() != 1) {
      result._statusCode = 500;
      return result;
    }
    
    result._body.clear();
    result._statusCode = 200;
    
  } catch(std::exception &e) {
    LOG(ERR) << "Error transforming result. " << e.what();
    result.clear();
  } catch(...) {
    LOG(ERR) << "Error transforming result. Out of memory";
    result.clear();
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes one or multiple values from the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::removeValues(std::string const& key,
                                          bool recursive) {
  AgencyWriteTransaction transaction(
      AgencyOperation(key, AgencySimpleOperationType::DELETE_OP),
      AgencyPrecondition(key, AgencyPrecondition::EMPTY, false)
  );

  return sendTransactionWithFailover(transaction);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a single value in the backend
/// the CAS condition is whether or not a previous value existed for the key
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::casValue(std::string const& key,
                                      arangodb::velocypack::Slice const& json,
                                      bool prevExist, double ttl,
                                      double timeout) {

  VPackBuilder newBuilder;
  newBuilder.add(json);

  AgencyOperation operation(key, AgencyValueOperationType::SET, newBuilder.slice());
  AgencyPrecondition precondition(key, AgencyPrecondition::EMPTY, !prevExist);
  if (ttl >= 0.0) {
    operation._ttl = static_cast<uint32_t>(ttl);
  }
  
  AgencyWriteTransaction transaction(operation, precondition);
  return sendTransactionWithFailover(transaction, timeout);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a single value in the backend
/// the CAS condition is whether or not the previous value for the key was
/// identical to `oldValue`
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::casValue(std::string const& key,
                                      VPackSlice const& oldJson,
                                      VPackSlice const& newJson, double ttl,
                                      double timeout) {
  VPackBuilder newBuilder;
  newBuilder.add(newJson);
  
  VPackBuilder oldBuilder;
  oldBuilder.add(oldJson);
  
  AgencyOperation operation(key, AgencyValueOperationType::SET, newBuilder.slice());
  AgencyPrecondition precondition(key, AgencyPrecondition::VALUE,
                                  oldBuilder.slice());
  if (ttl >= 0.0) {
    operation._ttl = static_cast<uint32_t>(ttl);
  }
  
  AgencyWriteTransaction transaction(operation, precondition);
  return sendTransactionWithFailover(transaction, timeout);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a callback on a key
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::registerCallback(std::string const& key, std::string const& endpoint) {
  VPackBuilder builder;
  builder.add(VPackValue(endpoint));
  
  AgencyOperation operation(key, AgencyValueOperationType::OBSERVE, builder.slice());
  AgencyWriteTransaction transaction(operation);
  
  auto result = sendTransactionWithFailover(transaction);
  return result.successful();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a callback on a key
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::unregisterCallback(std::string const& key,
                                    std::string const& endpoint) {
  VPackBuilder builder;
  builder.add(VPackValue(endpoint));
  
  AgencyOperation operation(key, AgencyValueOperationType::UNOBSERVE, builder.slice());
  AgencyWriteTransaction transaction(operation);
  
  auto result = sendTransactionWithFailover(transaction);
  return result.successful();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquire a read lock
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::lockRead(std::string const& key, double ttl, double timeout) {
  VPackBuilder builder;
  try {
    builder.add(VPackValue("READ"));
  } catch (...) {
    return false;
  }
  return lock(key, ttl, timeout, builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquire a write lock
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::lockWrite(std::string const& key, double ttl, double timeout) {
  VPackBuilder builder;
  try {
    builder.add(VPackValue("WRITE"));
  } catch (...) {
    return false;
  }
  return lock(key, ttl, timeout, builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief release a read lock
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::unlockRead(std::string const& key, double timeout) {
  VPackBuilder builder;
  try {
    builder.add(VPackValue("READ"));
  } catch (...) {
    return false;
  }
  return unlock(key, builder.slice(), timeout);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief release a write lock
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::unlockWrite(std::string const& key, double timeout) {
  VPackBuilder builder;
  try {
    builder.add(VPackValue("WRITE"));
  } catch (...) {
    return false;
  }
  return unlock(key, builder.slice(), timeout);
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

    if (result.httpCode() ==
        (int)arangodb::GeneralResponse::ResponseCode::NOT_FOUND) {
      try {
        VPackBuilder builder;
        builder.add(VPackValue(0));

        // create the key on the fly
        setValue(key, builder.slice(), 0.0);
        tries--;

        continue;
      } catch (...) {
        // Could not build local key. Try again
      }
    }

    if (!result.successful()) {
      return result;
    }

    result.parse("", false);

    std::shared_ptr<VPackBuilder> oldBuilder;

    std::map<std::string, AgencyCommResultEntry>::iterator it =
        result._values.begin();

    try {
      if (it != result._values.end()) {
        // steal the velocypack
        oldBuilder.swap((*it).second._vpack);
      } else {
        oldBuilder->add(VPackValue(0));
      }
    } catch (...) {
      return AgencyCommResult();
    }

    VPackSlice oldSlice = oldBuilder->slice();
    uint64_t const oldValue = arangodb::basics::VelocyPackHelper::stringUInt64(oldSlice) + count;
    uint64_t const newValue = oldValue + count;

    VPackBuilder newBuilder;
    try {
      newBuilder.add(VPackValue(newValue));
    } catch (...) {
      return AgencyCommResult();
    }

    result.clear();
    result = casValue(key, oldSlice, newBuilder.slice(), 0.0, timeout);

    if (result.successful()) {
      result._index = oldValue + 1;
      break;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a lock
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::lock(std::string const& key, double ttl, double timeout,
                      VPackSlice const& slice) {
  if (ttl == 0.0) {
    ttl = _globalConnectionOptions._lockTimeout;
  }

  if (timeout == 0.0) {
    timeout = _globalConnectionOptions._lockTimeout;
  }
  unsigned long sleepTime = InitialSleepTime;
  double const end = TRI_microtime() + timeout;

  VPackBuilder builder;
  try {
    builder.add(VPackValue("UNLOCKED"));
  } catch (...) {
    return false;
  }
  VPackSlice oldSlice = builder.slice();

  while (true) {
    AgencyCommResult result =
        casValue(key + "/Lock", oldSlice, slice, ttl, timeout);

    if (!result.successful() &&
        result.httpCode() ==
            (int)arangodb::GeneralResponse::ResponseCode::PRECONDITION_FAILED) {
      // key does not yet exist. create it now
      result = casValue(key + "/Lock", slice, false, ttl, timeout);
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

bool AgencyComm::unlock(std::string const& key, VPackSlice const& slice,
                        double timeout) {
  if (timeout == 0.0) {
    timeout = _globalConnectionOptions._lockTimeout;
  }

  unsigned long sleepTime = InitialSleepTime;
  double const end = TRI_microtime() + timeout;

  VPackBuilder builder;
  try {
    builder.add(VPackValue("UNLOCKED"));
  } catch (...) {
    // Out of Memory
    return false;
  }
  VPackSlice newSlice = builder.slice();

  while (true) {
    AgencyCommResult result =
        casValue(key + "/Lock", slice, newSlice, 0.0, timeout);

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
      WRITE_LOCKER(writeLocker, AgencyComm::_globalLock);

      size_t const numEndpoints TRI_UNUSED = _globalEndpoints.size();
      std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();

      while (it != _globalEndpoints.end()) {
        AgencyEndpoint* agencyEndpoint = (*it);

        TRI_ASSERT(agencyEndpoint != nullptr);

        if (!endpoint.empty() &&
            agencyEndpoint->_endpoint->specification() != endpoint) {
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
  WRITE_LOCKER(writeLocker, AgencyComm::_globalLock);
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
/// @brief construct a URL, without a key
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::buildUrl() const {
  return AgencyComm::AGENCY_URL_PREFIX;
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief sends a write HTTP request to the agency, handling failover
//////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::sendTransactionWithFailover(
    AgencyTransaction const& transaction, double timeout) {

  std::string url(buildUrl());

  url += "/write";

  return sendWithFailover(arangodb::GeneralRequest::RequestType::POST,
      timeout == 0.0 ? _globalConnectionOptions._requestTimeout : timeout, url,
      transaction.toJson(), false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends an HTTP request to the agency, handling failover
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::sendWithFailover(
      arangodb::GeneralRequest::RequestType method,
      double const timeout,
      std::string const& url,
      std::string const& body, bool isWatch) {

  size_t numEndpoints;

  {
    READ_LOCKER(readLocker, AgencyComm::_globalLock);
    numEndpoints = AgencyComm::_globalEndpoints.size();

    if (numEndpoints == 0) {
      AgencyCommResult result;
      result._statusCode = 400;
      result._message = "No endpoints for agency found.";
      return result;
    }
  }

  TRI_ASSERT(numEndpoints > 0);

  size_t tries = 0;
  std::string realUrl = url;
  std::string forceEndpoint;

  AgencyCommResult result;

  while (tries++ < numEndpoints) {
    AgencyEndpoint* agencyEndpoint = popEndpoint(forceEndpoint);

    TRI_ASSERT(agencyEndpoint != nullptr);

    try {
      result = send(agencyEndpoint->_connection, method, timeout, realUrl, body);
    } catch (...) {
      result._connected = false;
      result._statusCode = 0;
      result._message = "could not send request to agency";

      agencyEndpoint->_connection->disconnect();

      requeueEndpoint(agencyEndpoint, true);
      break;
    }

    if (result._statusCode ==
        (int)arangodb::GeneralResponse::ResponseCode::TEMPORARY_REDIRECT) {
      // sometimes the agency will return a 307 (temporary redirect)
      // in this case we have to pick it up and use the new location returned

      // put the current connection to the end of the list
      requeueEndpoint(agencyEndpoint, false);

      // a 307 does not count as a success
      TRI_ASSERT(!result.successful());

      std::string endpoint;

      // transform location into an endpoint
      int offset;
      if (result.location().substr(0, 7) == "http://") {
        endpoint = "http+tcp://" + result.location().substr(7);
        offset = 11;
      } else if (result.location().substr(0, 8) == "https://") {
        endpoint = "ssl://" + result.location().substr(8);
        offset = 6;
      } else {
        // invalid endpoint, return an error
        break;
      }

      size_t const delim = endpoint.find('/', offset);

      if (delim == std::string::npos) {
        // invalid location header
        break;
      }

      realUrl = endpoint.substr(delim);
      endpoint = endpoint.substr(0, delim);

      if (!AgencyComm::hasEndpoint(endpoint)) {
        AgencyComm::addEndpoint(endpoint, true);

        LOG(INFO) << "adding agency-endpoint '" << endpoint << "'";

        // re-check the new endpoint
        if (AgencyComm::hasEndpoint(endpoint)) {
          ++numEndpoints;
          continue;
        }

        LOG(ERR) << "found redirection to unknown endpoint '" << endpoint
                 << "'. Will not follow!";

        // this is an error
        break;
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
      break;
    }

    // otherwise, try next
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends data to the URL
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::send(
      arangodb::httpclient::GeneralClientConnection* connection,
      arangodb::GeneralRequest::RequestType method,
      double timeout, std::string const& url, std::string const& body) {
  TRI_ASSERT(connection != nullptr);

  if (method == arangodb::GeneralRequest::RequestType::GET ||
      method == arangodb::GeneralRequest::RequestType::HEAD ||
      method == arangodb::GeneralRequest::RequestType::DELETE_REQ) {
    TRI_ASSERT(body.empty());
  }

  TRI_ASSERT(!url.empty());

  AgencyCommResult result;
  result._connected = false;
  result._statusCode = 0;

  LOG(TRACE) << "sending " << arangodb::HttpRequest::translateMethod(method)
             << " request to agency at endpoint '"
             << connection->getEndpoint()->specification() << "', url '" << url
             << "': " << body;

  arangodb::httpclient::SimpleHttpClient client(connection, timeout, false);

  client.keepConnectionOnDestruction(true);

  // set up headers
  std::unordered_map<std::string, std::string> headers;
  if (method == arangodb::GeneralRequest::RequestType::POST) {
    // the agency needs this content-type for the body
    headers["content-type"] = "application/json";
  }

  // send the actual request
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response(
      client.request(method, url, body.c_str(), body.size(), headers));

  if (response == nullptr) {
    connection->disconnect();
    result._message = "could not send request to agency";
    LOG(TRACE) << "sending request to agency failed";

    return result;
  }

  if (!response->isComplete()) {
    connection->disconnect();
    result._message = "sending request to agency failed";
    LOG(TRACE) << "sending request to agency failed";

    return result;
  }

  result._connected = true;
  
  if (response->getHttpReturnCode() ==
      (int)arangodb::GeneralResponse::ResponseCode::TEMPORARY_REDIRECT) {
    // temporary redirect. now save location header

    bool found = false;
    result._location = response->getHeaderField("location", found);

    LOG(TRACE) << "redirecting to location: '" << result._location << "'";

    if (!found) {
      // a 307 without a location header does not make any sense
      connection->disconnect();
      result._message = "invalid agency response (header missing)";

      return result;
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

  LOG(TRACE) << "request to agency returned status code " << result._statusCode
             << ", message: '" << result._message << "', body: '"
             << result._body << "'";

  if (result.successful()) {
    return result;
  }

  connection->disconnect();
  return result;
}
