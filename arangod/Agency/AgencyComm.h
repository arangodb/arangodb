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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_AGENCY_COMM_H
#define ARANGOD_CLUSTER_AGENCY_COMM_H 1

#include "Basics/Common.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <type_traits>

#include <list>

#include "Basics/Mutex.h"
#include "Rest/HttpRequest.h"
#include "SimpleHttpClient/GeneralClientConnection.h"

namespace arangodb {
class Endpoint;

namespace velocypack {
class Builder;
class Slice;
}

// -----------------------------------------------------------------------------
// --SECTION--                                           AgencyConnectionOptions
// -----------------------------------------------------------------------------

struct AgencyConnectionOptions {
  AgencyConnectionOptions(double ct, double rt, double lt, size_t cr) :
    _connectTimeout(ct), _requestTimeout(rt), _lockTimeout(lt),
    _connectRetries(cr) {}
  double _connectTimeout;
  double _requestTimeout;
  double _lockTimeout;
  size_t _connectRetries;
};

// -----------------------------------------------------------------------------
// --SECTION--                                             AgencyCommResultEntry
// -----------------------------------------------------------------------------

class AgencyCommResultEntry {
 public:
  uint64_t _index;
  std::shared_ptr<arangodb::velocypack::Builder> _vpack;
  bool _isDir;
};

// -----------------------------------------------------------------------------
// --SECTION--                                          AgencyValueOperationType
// -----------------------------------------------------------------------------

enum class AgencyReadOperationType { READ };

// -----------------------------------------------------------------------------
// --SECTION--                                          AgencyValueOperationType
// -----------------------------------------------------------------------------

enum class AgencyValueOperationType { SET, OBSERVE, UNOBSERVE, PUSH, PREPEND };

// -----------------------------------------------------------------------------
// --SECTION--                                         AgencySimpleOperationType
// -----------------------------------------------------------------------------

enum class AgencySimpleOperationType {
  INCREMENT_OP,
  DECREMENT_OP,
  DELETE_OP,
  POP_OP,
  SHIFT_OP
};

// -----------------------------------------------------------------------------
// --SECTION--                                               AgencyOperationType
// -----------------------------------------------------------------------------

class AgencyOperationType {
 public:
  enum class Type { VALUE, SIMPLE, READ };

 public:
  Type type;

  union {
    AgencyValueOperationType value;
    AgencySimpleOperationType simple;
    AgencyReadOperationType read;
  };

 public:
  std::string toString() const {
    switch (type) {
      case Type::VALUE:
        switch (value) {
          case AgencyValueOperationType::SET:
            return "set";
          case AgencyValueOperationType::OBSERVE:
            return "observe";
          case AgencyValueOperationType::UNOBSERVE:
            return "unobserve";
          case AgencyValueOperationType::PUSH:
            return "push";
          case AgencyValueOperationType::PREPEND:
            return "prepend";
          default:
            return "unknown_operation_type";
        }
        break;
      case Type::SIMPLE:
        switch (simple) {
          case AgencySimpleOperationType::INCREMENT_OP:
            return "increment";
          case AgencySimpleOperationType::DECREMENT_OP:
            return "decrement";
          case AgencySimpleOperationType::DELETE_OP:
            return "delete";
          case AgencySimpleOperationType::POP_OP:
            return "pop";
          case AgencySimpleOperationType::SHIFT_OP:
            return "shift";
          default:
            return "unknown_operation_type";
        }
        break;
      case Type::READ:
        return "read";
        break;
      default:
        return "unknown_operation_type";
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                AgencyPrecondition
// -----------------------------------------------------------------------------

class AgencyPrecondition {
 public:
  enum class Type { NONE, EMPTY, VALUE };

 public:
  AgencyPrecondition();
  AgencyPrecondition(std::string const& key, Type, bool e);
  AgencyPrecondition(std::string const& key, Type, VPackSlice const);

 public:
  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  void toGeneralBuilder(arangodb::velocypack::Builder& builder) const;

 public:
  std::string key;
  Type type;
  bool empty;
  VPackSlice const value;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   AgencyOperation
// -----------------------------------------------------------------------------

class AgencyOperation {
 public:
  explicit AgencyOperation(std::string const& key);
  
  AgencyOperation(std::string const& key, AgencySimpleOperationType opType);

  AgencyOperation(std::string const& key, AgencyValueOperationType opType,
                  VPackSlice const value);

 public:
  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  void toGeneralBuilder(arangodb::velocypack::Builder& builder) const;
  AgencyOperationType type() const;

 public:
  uint32_t _ttl = 0;
  VPackSlice _oldValue;

 private:
  std::string const _key;
  AgencyOperationType _opType;
  VPackSlice _value;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  AgencyCommResult
// -----------------------------------------------------------------------------

class AgencyCommResult {
 public:
  AgencyCommResult();
  AgencyCommResult(int code, std::string const& message);
  ~AgencyCommResult() = default;

 public:
  bool successful() const { return (_statusCode >= 200 && _statusCode <= 299); }

  bool connected() const;

  int httpCode() const;

  int errorCode() const;

  std::string errorMessage() const;

  std::string errorDetails() const;

  std::string const location() const { return _location; }

  std::string const body() const { return _body; }

  void clear();

  VPackSlice slice() const;
  void setVPack(std::shared_ptr<velocypack::Builder> vpack) { _vpack = vpack; }

 public:
  std::string _location;
  std::string _message;
  std::string _body;
  std::string _realBody;

  std::map<std::string, AgencyCommResultEntry> _values;
  int _statusCode;
  bool _connected;

 private:
  std::shared_ptr<velocypack::Builder> _vpack;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 AgencyTransaction
// -----------------------------------------------------------------------------

class AgencyTransaction {

public:
  virtual ~AgencyTransaction() = default;

  static const std::vector<std::string> TypeUrl;

  virtual std::string toJson() const = 0;
  virtual void toVelocyPack(arangodb::velocypack::Builder&) const = 0;
  virtual std::string const& path() const = 0;

  virtual bool validate(AgencyCommResult const& result) const = 0;
  
};

// -----------------------------------------------------------------------------
// --SECTION--                                          AgencyGeneralTransaction
// -----------------------------------------------------------------------------

struct AgencyGeneralTransaction : public AgencyTransaction {

  explicit AgencyGeneralTransaction(
    std::pair<AgencyOperation,AgencyPrecondition> const& operation) {
    operations.push_back(operation);
  }
  
  explicit AgencyGeneralTransaction(
    std::vector<std::pair<AgencyOperation,AgencyPrecondition>> const& _operations)
    : operations(_operations) {}
  
  AgencyGeneralTransaction() = default;
  
  std::vector<std::pair<AgencyOperation,AgencyPrecondition>> operations;

  void toVelocyPack(
    arangodb::velocypack::Builder& builder) const override final;

  std::string toJson() const override final;

  void push_back(std::pair<AgencyOperation,AgencyPrecondition> const&);

  inline std::string const& path() const override final {
    return AgencyTransaction::TypeUrl[2];
  }

  virtual bool validate(AgencyCommResult const& result) const override final;
  

};

// -----------------------------------------------------------------------------
// --SECTION--                                            AgencyWriteTransaction
// -----------------------------------------------------------------------------

struct AgencyWriteTransaction : public AgencyTransaction {

public:

  explicit AgencyWriteTransaction(AgencyOperation const& operation) {
    operations.push_back(operation);
  }

  explicit AgencyWriteTransaction(
      std::vector<AgencyOperation> const& _operations)
      : operations(_operations) {}

  AgencyWriteTransaction(AgencyOperation const& operation,
                         AgencyPrecondition const& precondition) {
    operations.push_back(operation);
    preconditions.push_back(precondition);
  }

  AgencyWriteTransaction(std::vector<AgencyOperation> const& _operations,
                         AgencyPrecondition const& precondition) {
    for (auto const& op : _operations) {
      operations.push_back(op);
    }
    preconditions.push_back(precondition);
  }

  AgencyWriteTransaction(std::vector<AgencyOperation> const& opers,
                         std::vector<AgencyPrecondition> const& precs) {
    for (auto const& op : opers) {
      operations.push_back(op);
    }
    for (auto const& pre : precs) {
      preconditions.push_back(pre);
    }
  }

  AgencyWriteTransaction() = default;

  void toVelocyPack(
      arangodb::velocypack::Builder& builder) const override final;

  std::string toJson() const override final;

  inline std::string const& path() const override final {
    return AgencyTransaction::TypeUrl[1];
  }

  virtual bool validate(AgencyCommResult const& result) const override final;

  std::vector<AgencyPrecondition> preconditions;
  std::vector<AgencyOperation> operations;
};

// -----------------------------------------------------------------------------
// --SECTION--                                             AgencyReadTransaction
// -----------------------------------------------------------------------------

struct AgencyReadTransaction : public AgencyTransaction {

public:

  explicit AgencyReadTransaction(std::string const& key) {
    keys.push_back(key);
  }

  explicit AgencyReadTransaction(std::vector<std::string>&& k) : keys(k) {}

  AgencyReadTransaction() = default;

  void toVelocyPack(
      arangodb::velocypack::Builder& builder) const override final;

  std::string toJson() const override final;

  inline std::string const& path() const override final {
    return AgencyTransaction::TypeUrl[0];
  }

  virtual bool validate(AgencyCommResult const& result) const override final;

  std::vector<std::string> keys;

};

// -----------------------------------------------------------------------------
// --SECTION--                                                 AgencyCommManager
// -----------------------------------------------------------------------------

class AgencyCommManager {
 public:
  static std::unique_ptr<AgencyCommManager> MANAGER;
  static AgencyConnectionOptions CONNECTION_OPTIONS;

 public:
  static void initialize(std::string const& prefix);
  static void shutdown();

  static bool isEnabled() { return MANAGER != nullptr; }

  static std::string path();
  static std::string path(std::string const&);
  static std::string path(std::string const&, std::string const&);

  static std::string generateStamp();

 public:
  explicit AgencyCommManager(std::string const& prefix) : _prefix(prefix) {}

 public:
  bool start();
  void stop();

  std::unique_ptr<httpclient::GeneralClientConnection> acquire(
      std::string& endpoint);

  void release(std::unique_ptr<httpclient::GeneralClientConnection>,
               std::string const& endpoint);

  void failed(std::unique_ptr<httpclient::GeneralClientConnection>,
              std::string const& endpoint);

  void redirect(std::unique_ptr<httpclient::GeneralClientConnection>,
                std::string const& endpoint, std::string const& location,
                std::string& url);

  void addEndpoint(std::string const&);
  std::string endpointsString() const;
  std::vector<std::string> endpoints() const;

 private:
  // caller must hold _lock
  std::unique_ptr<httpclient::GeneralClientConnection> createNewConnection();

  // caller must hold _lock
  void switchCurrentEndpoint();

 private:
  std::string const _prefix;

  // protects all the members
  mutable Mutex _lock;

  std::deque<std::string> _endpoints;

  std::vector<std::unique_ptr<httpclient::GeneralClientConnection>>
      _unusedConnections;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        AgencyComm
// -----------------------------------------------------------------------------

class AgencyComm {
 private:
  static std::string const AGENCY_URL_PREFIX;
  static uint64_t const INITIAL_SLEEP_TIME = 5000;
  static uint64_t const MAX_SLEEP_TIME = 50000;
  static uint64_t const MAX_TRIES = 3;

 public:
  AgencyCommResult sendServerState(double ttl);

  std::string version();

  bool increaseVersion(std::string const& key) {
    AgencyCommResult result = increment(key);
    return result.successful();
  }

  AgencyCommResult createDirectory(std::string const&);

  AgencyCommResult setValue(std::string const&, std::string const&, double);

  AgencyCommResult setValue(std::string const&,
                            arangodb::velocypack::Slice const&, double);

  bool exists(std::string const&);

  AgencyCommResult getValues(std::string const&);

  AgencyCommResult removeValues(std::string const&, bool);

  AgencyCommResult increment(std::string const&);

  // compares and swaps a single value in the backend the CAS condition is
  // whether or not a previous value existed for the key

  AgencyCommResult casValue(std::string const&,
                            arangodb::velocypack::Slice const&, bool, double,
                            double);

  // compares and swaps a single value in the back end the CAS condition is
  // whether or not the previous value for the key was identical to `oldValue`

  AgencyCommResult casValue(std::string const&,
                            arangodb::velocypack::Slice const&,
                            arangodb::velocypack::Slice const&, double, double);

  uint64_t uniqid(uint64_t, double);

  bool registerCallback(std::string const& key, std::string const& endpoint);

  bool unregisterCallback(std::string const& key, std::string const& endpoint);

  bool lockRead(std::string const&, double, double);

  bool lockWrite(std::string const&, double, double);

  bool unlockRead(std::string const&, double);

  bool unlockWrite(std::string const&, double);

  AgencyCommResult sendTransactionWithFailover(AgencyTransaction const&,
                                               double timeout = 0.0);

  bool ensureStructureInitialized();

 private:
  bool lock(std::string const&, double, double,
            arangodb::velocypack::Slice const&);

  bool unlock(std::string const&, arangodb::velocypack::Slice const&, double);

  AgencyCommResult sendWithFailover(arangodb::rest::RequestType, double,
                                    std::string const&, std::string const&,
                                    bool);

  AgencyCommResult send(httpclient::GeneralClientConnection*, rest::RequestType,
                        double, std::string const&, std::string const&);

  bool tryInitializeStructure(std::string const& jwtSecret);

  bool shouldInitializeStructure();
};
}

#endif
