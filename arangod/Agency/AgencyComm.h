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

#include "Basics/ReadWriteLock.h"
#include "Rest/HttpRequest.h"

namespace arangodb {
class Endpoint;

namespace httpclient {
class GeneralClientConnection;
}

namespace velocypack {
class Builder;
class Slice;
}

class AgencyComm;

enum class AgencyValueOperationType { SET, OBSERVE, UNOBSERVE, PUSH, PREPEND };

enum class AgencySimpleOperationType {
  INCREMENT_OP,
  DECREMENT_OP,
  DELETE_OP,
  POP_OP,
  SHIFT_OP
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    AgencyEndpoint
// -----------------------------------------------------------------------------

struct AgencyEndpoint {
  AgencyEndpoint(Endpoint*, arangodb::httpclient::GeneralClientConnection*);
  ~AgencyEndpoint();

  Endpoint* _endpoint;
  arangodb::httpclient::GeneralClientConnection* _connection;
  bool _busy;
};

// -----------------------------------------------------------------------------
// --SECTION--                                           AgencyConnectionOptions
// -----------------------------------------------------------------------------

struct AgencyConnectionOptions {
  double _connectTimeout;
  double _requestTimeout;
  double _lockTimeout;
  size_t _connectRetries;
};

// -----------------------------------------------------------------------------
// --SECTION--                                             AgencyCommResultEntry
// -----------------------------------------------------------------------------

struct AgencyCommResultEntry {
  uint64_t _index;
  std::shared_ptr<arangodb::velocypack::Builder> _vpack;
  bool _isDir;
};

// -----------------------------------------------------------------------------
// --SECTION--                                               AgencyOperationType
// -----------------------------------------------------------------------------

struct AgencyOperationType {
  enum { VALUE, SIMPLE } type;

  union {
    AgencyValueOperationType value;
    AgencySimpleOperationType simple;
  };

  std::string toString() const {
    switch (type) {
      case VALUE:
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
      case SIMPLE:
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
      default:
        return "unknown_operation_type";
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                AgencyPrecondition
// -----------------------------------------------------------------------------

struct AgencyPrecondition {
  typedef enum { NONE, EMPTY, VALUE } Type;

  AgencyPrecondition(std::string const& key, Type t, bool e);
  AgencyPrecondition(std::string const& key, Type t, VPackSlice const s);

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;

  std::string key;
  Type type;
  bool empty;
  VPackSlice const value;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   AgencyOperation
// -----------------------------------------------------------------------------

struct AgencyOperation {
  AgencyOperation(std::string const& key, AgencySimpleOperationType opType);

  AgencyOperation(std::string const& key, AgencyValueOperationType opType,
                  VPackSlice const value);

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;

  uint32_t _ttl = 0;
  VPackSlice _oldValue;

 private:
  std::string const _key;
  AgencyOperationType _opType;
  VPackSlice _value;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 AgencyTransaction
// -----------------------------------------------------------------------------

struct AgencyTransaction {
  virtual std::string toJson() const = 0;
  virtual void toVelocyPack(arangodb::velocypack::Builder& builder) const = 0;
  virtual ~AgencyTransaction() {}
  virtual bool isWriteTransaction() const = 0;
};

// -----------------------------------------------------------------------------
// --SECTION--                                            AgencyWriteTransaction
// -----------------------------------------------------------------------------

struct AgencyWriteTransaction : public AgencyTransaction {
  std::vector<AgencyPrecondition> preconditions;
  std::vector<AgencyOperation> operations;

  void toVelocyPack(
      arangodb::velocypack::Builder& builder) const override final;

  std::string toJson() const override final;

  explicit AgencyWriteTransaction(AgencyOperation const& operation) {
    operations.push_back(operation);
  }

  explicit AgencyWriteTransaction(
      std::vector<AgencyOperation> const& _operations)
      : operations(_operations) {}

  explicit AgencyWriteTransaction(AgencyOperation const& operation,
                                  AgencyPrecondition const& precondition) {
    operations.push_back(operation);
    preconditions.push_back(precondition);
  }

  explicit AgencyWriteTransaction(
      std::vector<AgencyOperation> const& _operations,
      AgencyPrecondition const& precondition) {
    for (auto const& op : _operations) {
      operations.push_back(op);
    }
    preconditions.push_back(precondition);
  }

  explicit AgencyWriteTransaction(
      std::vector<AgencyOperation> const& opers,
      std::vector<AgencyPrecondition> const& precs) {
    for (auto const& op : opers) {
      operations.push_back(op);
    }
    for (auto const& pre : precs) {
      preconditions.push_back(pre);
    }
  }

  AgencyWriteTransaction() = default;

  bool isWriteTransaction() const override final { return true; }
};

// -----------------------------------------------------------------------------
// --SECTION--                                             AgencyReadTransaction
// -----------------------------------------------------------------------------

struct AgencyReadTransaction : public AgencyTransaction {
  std::vector<std::string> keys;

  void toVelocyPack(
      arangodb::velocypack::Builder& builder) const override final;

  std::string toJson() const override final;

  explicit AgencyReadTransaction(std::string const& key) {
    keys.push_back(key);
  }

  explicit AgencyReadTransaction(std::vector<std::string>&& k) : keys(k) {}

  AgencyReadTransaction() = default;

  bool isWriteTransaction() const override final { return false; }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  AgencyCommResult
// -----------------------------------------------------------------------------

struct AgencyCommResult {
  AgencyCommResult();

  ~AgencyCommResult();

  bool successful() const { return (_statusCode >= 200 && _statusCode <= 299); }

  bool connected() const;

  int httpCode() const;

  int errorCode() const;

  std::string errorMessage() const;

  std::string errorDetails() const;

  std::string const location() const { return _location; }

  std::string const body() const { return _body; }

  void clear();

  VPackSlice slice();
  void setVPack(std::shared_ptr<velocypack::Builder> vpack) { _vpack = vpack; }

 private:
  std::shared_ptr<velocypack::Builder> _vpack;

 public:
  std::string _location;
  std::string _message;
  std::string _body;
  std::string _realBody;

  std::map<std::string, AgencyCommResultEntry> _values;
  int _statusCode;
  bool _connected;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        AgencyComm
// -----------------------------------------------------------------------------

class AgencyComm {
  friend struct AgencyCommResult;

 public:
  static void cleanup();
  static bool initialize();
  static void disconnect();

  static bool addEndpoint(std::string const&, bool = false);
  static bool hasEndpoint(std::string const&);
  static std::vector<std::string> getEndpoints();
  static std::string getEndpointsString();
  static std::string getUniqueEndpointsString();

  static bool setPrefix(std::string const&);
  static std::string prefixPath();
  static std::string prefix();

  static std::string generateStamp();

  static AgencyEndpoint* createAgencyEndpoint(std::string const&);

  AgencyCommResult sendServerState(double ttl);
  std::string getVersion();

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

  AgencyCommResult increment(std::string const&);

  AgencyCommResult removeValues(std::string const&, bool);

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

 private:
  static bool tryConnect();

 private:
  bool lock(std::string const&, double, double,
            arangodb::velocypack::Slice const&);

  bool unlock(std::string const&, arangodb::velocypack::Slice const&, double);

  AgencyEndpoint* popEndpoint(std::string const&);

  void requeueEndpoint(AgencyEndpoint*, bool);

  std::string buildUrl() const;

  AgencyCommResult sendWithFailover(arangodb::rest::RequestType, double,
                                    std::string const&, std::string const&,
                                    bool);

  AgencyCommResult send(arangodb::httpclient::GeneralClientConnection*,
                        arangodb::rest::RequestType, double, std::string const&,
                        std::string const&);

  bool ensureStructureInitialized();

  bool tryInitializeStructure(std::string const& jwtSecret);

  bool shouldInitializeStructure();

 private:
  static std::string const AGENCY_URL_PREFIX;

  static std::string _globalPrefix;
  static std::string _globalPrefixStripped;

  static arangodb::basics::ReadWriteLock _globalLock;

  static std::list<AgencyEndpoint*> _globalEndpoints;

  static AgencyConnectionOptions _globalConnectionOptions;

  static size_t const NumConnections = 3;

  static unsigned long const InitialSleepTime = 5000;

  static unsigned long const MaxSleepTime = 50000;
};
}

#endif
