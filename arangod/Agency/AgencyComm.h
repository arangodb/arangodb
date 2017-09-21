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

#include <list>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <type_traits>

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

enum class AgencyValueOperationType { ERASE, SET, OBSERVE, UNOBSERVE, PUSH, PREPEND };

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
          case AgencyValueOperationType::ERASE:
            return "erase";
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
  AgencyCommResult(int code, std::string const& message,
                   std::string const& transactionId = std::string());

  ~AgencyCommResult() = default;

 public:
  void set(int code, std::string const& message,
           std::string const& clientId);

  bool successful() const { return (_statusCode >= 200 && _statusCode <= 299); }

  bool connected() const;

  int httpCode() const;

  int errorCode() const;

  std::string clientId() const;

  std::string errorMessage() const;

  std::string errorDetails() const;

  std::string const location() const { return _location; }

  std::string const body() const { return _body; }
  std::string const& bodyRef() const { return _body; }

  bool sent() const;

  void clear();

  VPackSlice slice() const;
  void setVPack(std::shared_ptr<velocypack::Builder> vpack) { _vpack = vpack; }

 public:
  std::string _location;
  std::string _message;
  std::string _body;
  std::string _realBody;

  std::unordered_map<std::string, AgencyCommResultEntry> _values;
  int _statusCode;
  bool _connected;
  bool _sent;

 private:
  std::shared_ptr<velocypack::Builder> _vpack;

public:
  std::string _clientId;
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
  virtual std::string getClientId() const = 0;
  
  virtual bool validate(AgencyCommResult const& result) const = 0;
  
};

// -----------------------------------------------------------------------------
// --SECTION--                                          AgencyGeneralTransaction
// -----------------------------------------------------------------------------

struct AgencyGeneralTransaction : public AgencyTransaction {

  typedef std::pair<std::vector<AgencyOperation>,std::vector<AgencyPrecondition>> TransactionType;

  explicit AgencyGeneralTransaction(
    std::pair<AgencyOperation,AgencyPrecondition> const& trx) :
    clientId(to_string(boost::uuids::random_generator()())) {
    transactions.emplace_back(
      TransactionType(std::vector<AgencyOperation>(1,trx.first),
                      std::vector<AgencyPrecondition>(1,trx.second)));
  }
  
  explicit AgencyGeneralTransaction(
    std::vector<std::pair<AgencyOperation,AgencyPrecondition>> const& trxs) :
    clientId(to_string(boost::uuids::random_generator()())) {
    for (const auto& trx : trxs) {
      transactions.emplace_back(
        TransactionType(std::vector<AgencyOperation>(1,trx.first),
                        std::vector<AgencyPrecondition>(1,trx.second)));
      
    }
  }
  
  AgencyGeneralTransaction() = default;
  
  std::vector<TransactionType> transactions;

  void toVelocyPack(
    arangodb::velocypack::Builder& builder) const override final;

  std::string toJson() const override final;

  void push_back(std::pair<AgencyOperation,AgencyPrecondition> const&);

  inline virtual std::string const& path() const override final {
    return AgencyTransaction::TypeUrl[2];
  }

  inline virtual std::string getClientId() const override final {
    return clientId;
  }
  
  virtual bool validate(AgencyCommResult const& result) const override final;
  std::string clientId;

};

// -----------------------------------------------------------------------------
// --SECTION--                                            AgencyWriteTransaction
// -----------------------------------------------------------------------------

struct AgencyWriteTransaction : public AgencyTransaction {

public:
  
  explicit AgencyWriteTransaction(AgencyOperation const& operation) :
    clientId(to_string(boost::uuids::random_generator()())) {
    operations.push_back(operation);
  }
  
  explicit AgencyWriteTransaction (std::vector<AgencyOperation> const& _opers) :
    operations(_opers),
    clientId(to_string(boost::uuids::random_generator()())) {}
  
  AgencyWriteTransaction(AgencyOperation const& operation,
                         AgencyPrecondition const& precondition) :
    clientId(to_string(boost::uuids::random_generator()())) {
    operations.push_back(operation);
    preconditions.push_back(precondition);
  }
  
  AgencyWriteTransaction(std::vector<AgencyOperation> const& _operations,
                         AgencyPrecondition const& precondition) :
    clientId(to_string(boost::uuids::random_generator()())) {
    for (auto const& op : _operations) {
      operations.push_back(op);
    }
    preconditions.push_back(precondition);
  }

  AgencyWriteTransaction(std::vector<AgencyOperation> const& opers,
                         std::vector<AgencyPrecondition> const& precs) :
    clientId(to_string(boost::uuids::random_generator()())) {
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

  inline virtual std::string const& path() const override final {
    return AgencyTransaction::TypeUrl[1];
  }

  inline virtual std::string getClientId() const override final {
    return clientId;
  }
  
  virtual bool validate(AgencyCommResult const& result) const override final;

  std::vector<AgencyPrecondition> preconditions;
  std::vector<AgencyOperation> operations;
  std::string clientId;
};

// -----------------------------------------------------------------------------
// --SECTION--                                            AgencyTransientTransaction
// -----------------------------------------------------------------------------

struct AgencyTransientTransaction : public AgencyTransaction {

public:

  explicit AgencyTransientTransaction(AgencyOperation const& operation) {
    operations.push_back(operation);
  }

  explicit AgencyTransientTransaction(
      std::vector<AgencyOperation> const& _operations)
      : operations(_operations) {}

  AgencyTransientTransaction(AgencyOperation const& operation,
                         AgencyPrecondition const& precondition) {
    operations.push_back(operation);
    preconditions.push_back(precondition);
  }

  AgencyTransientTransaction(std::vector<AgencyOperation> const& _operations,
                         AgencyPrecondition const& precondition) {
    for (auto const& op : _operations) {
      operations.push_back(op);
    }
    preconditions.push_back(precondition);
  }

  AgencyTransientTransaction(std::vector<AgencyOperation> const& opers,
                         std::vector<AgencyPrecondition> const& precs) {
    for (auto const& op : opers) {
      operations.push_back(op);
    }
    for (auto const& pre : precs) {
      preconditions.push_back(pre);
    }
  }

  AgencyTransientTransaction() = default;

  void toVelocyPack(
      arangodb::velocypack::Builder& builder) const override final;

  std::string toJson() const override final;

  inline std::string const& path() const override final {
    return AgencyTransaction::TypeUrl[3];
  }

  inline virtual std::string getClientId() const override final {
    return std::string();
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

  inline virtual std::string const& path() const override final {
    return AgencyTransaction::TypeUrl[0];
  }

  inline virtual std::string getClientId() const override final {
    return std::string();
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

  // Get a connection to the current endpoint, which will be filled in to
  // `endpoint`, if that is empty. Otherwise, a connection to the non-empty
  // endpoint `endpoint` is returned, regardless of what is the current
  // endpoint.
  std::unique_ptr<httpclient::GeneralClientConnection> acquire(
      std::string& endpoint);

  // Returns a connection to the manager. `endpoint` must be the string
  // description under which it was `acquire`d. Call this if you are done
  // using the connection and no error occurred.
  void release(std::unique_ptr<httpclient::GeneralClientConnection>,
               std::string const& endpoint);

  // Returns a connection to the manager. `endpoint` must be the string
  // description under which it was `acquire`d. Call this if you are done
  // using the connection and an error occurred. The connection object will
  // be destroyed and the current endpoint will be rotated.
  void failed(std::unique_ptr<httpclient::GeneralClientConnection>,
              std::string const& endpoint);

  // If a request receives a redirect HTTP 307, one should call the following
  // method to make the new location the current one. The method returns the
  // new endpoint specification. If anything goes wrong (for example, some
  // other thread has in the meantime changed the active endpoint), an empty
  // string is returned, which means that one has to acquire a new endpoint.
  std::string redirect(std::unique_ptr<httpclient::GeneralClientConnection>,
                       std::string const& endpoint, std::string const& location,
                       std::string& url);

  void addEndpoint(std::string const&);
  void removeEndpoint(std::string const&);
  std::string endpointsString() const;
  std::vector<std::string> endpoints() const;
  std::shared_ptr<VPackBuilder> summery() const;

 private:

  // caller must hold _lock
  void failedNonLocking(std::unique_ptr<httpclient::GeneralClientConnection>,
              std::string const& endpoint);

  // caller must hold lock
  void releaseNonLocking(std::unique_ptr<httpclient::GeneralClientConnection>,
                         std::string const& endpoint);

  // caller must hold _lock
  std::unique_ptr<httpclient::GeneralClientConnection> createNewConnection();

  // caller must hold _lock
  void switchCurrentEndpoint();

 private:
  std::string const _prefix;

  // protects all the members
  mutable Mutex _lock;

  // The following structure contains a list of string descriptions of the
  // known agency endpoints. The front one is the one currently used for
  // communication. If there is a redirect, we add the redirected one to
  // the list (if not already there) and move it to the front. If we fail
  // with the communication with the front one, we move it to the back and
  // try the next.
  std::deque<std::string> _endpoints;

  // In the following map we cache GeneralClientConnections to the above
  // endpoints. One can acquire one of them for use, in which case it is
  // removed from the corresponding vector. If one calls `release` on it, 
  // it is sent back to the ununsed vector. If an error occurs one
  // should call `failed` such that the manager can switch to a new
  // current endpoint. In case a redirect is received, one has to inform
  // the manager by calling `redirect`.
  std::unordered_map<std::string,
           std::vector<std::unique_ptr<httpclient::GeneralClientConnection>>>
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

#ifdef DEBUG_SYNC_REPLICATION
 public:
  static bool syncReplDebug;
#endif

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

  AgencyCommResult setTransient(std::string const&,
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

  AgencyCommResult registerCallback(
    std::string const& key, std::string const& endpoint);

  AgencyCommResult unregisterCallback(
    std::string const& key, std::string const& endpoint);

  void updateEndpoints(arangodb::velocypack::Slice const&);

  bool lockRead(std::string const&, double, double);

  bool lockWrite(std::string const&, double, double);

  bool unlockRead(std::string const&, double);

  bool unlockWrite(std::string const&, double);

  AgencyCommResult sendTransactionWithFailover(AgencyTransaction const&,
                                               double timeout = 0.0);

  bool ensureStructureInitialized();

  AgencyCommResult sendWithFailover(arangodb::rest::RequestType, double,
                                    std::string const&, VPackSlice,
                                    std::string const& clientId = std::string());

 private:
  bool lock(std::string const&, double, double,
            arangodb::velocypack::Slice const&);

  bool unlock(std::string const&, arangodb::velocypack::Slice const&, double);

  AgencyCommResult send(httpclient::GeneralClientConnection*, rest::RequestType,
                        double, std::string const&, std::string const&,
                        std::string const& clientId = std::string());

  bool tryInitializeStructure(std::string const& jwtSecret);

  bool shouldInitializeStructure();
};
}

#endif
