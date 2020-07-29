////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include <deque>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Agency/PathComponent.h"
#include "Basics/Mutex.h"
#include "Basics/Result.h"
#include "GeneralServer/GeneralDefinitions.h"
#include "Rest/CommonDefines.h"
#include "RestServer/Metrics.h"
#include "SimpleHttpClient/GeneralClientConnection.h"

namespace arangodb {
class Endpoint;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

// -----------------------------------------------------------------------------
// --SECTION--                                           AgencyConnectionOptions
// -----------------------------------------------------------------------------

struct AgencyConnectionOptions {
  AgencyConnectionOptions(double ct, double rt, double lt, size_t cr) noexcept
      : _connectTimeout(ct), _requestTimeout(rt), _lockTimeout(lt), _connectRetries(cr) {}
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

enum class AgencyValueOperationType {
  ERASE,
  SET,
  OBSERVE,
  UNOBSERVE,
  PUSH,
  PREPEND,
  REPLACE
};

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
          case AgencyValueOperationType::REPLACE:
            return "replace";
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
// --SECTION--                                                 AgencyCommHelper
// -----------------------------------------------------------------------------

class AgencyCommHelper {
 public:
  static AgencyConnectionOptions CONNECTION_OPTIONS;
  static std::string PREFIX;

 public:
  static void initialize(std::string const& prefix);
  static void shutdown();

  static std::string path();
  static std::string path(std::string const&);
  static std::string path(std::string const&, std::string const&);
  static std::vector<std::string> slicePath(std::string const&);

  static std::string generateStamp();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                AgencyPrecondition
// -----------------------------------------------------------------------------

class AgencyPrecondition {
 public:
  enum class Type { NONE, EMPTY, VALUE, TIN, NOTIN, INTERSECTION_EMPTY};

 public:
  AgencyPrecondition();
  AgencyPrecondition(std::string const& key, Type, bool e);
  AgencyPrecondition(std::string const& key, Type, velocypack::Slice const&);
  template <typename T>
  AgencyPrecondition(std::string const& key, Type t, T const& v)
      : key(AgencyCommHelper::path(key)),
        type(t),
        empty(false),
        builder(std::make_shared<VPackBuilder>()) {
    builder->add(VPackValue(v));
    value = builder->slice();
  }

  AgencyPrecondition(std::shared_ptr<cluster::paths::Path const> const& path, Type, bool e);
  AgencyPrecondition(std::shared_ptr<cluster::paths::Path const> const& path,
                     Type, velocypack::Slice const&);
  template <typename T>
  AgencyPrecondition(std::shared_ptr<cluster::paths::Path const> const& path,
                     Type t, T const& v)
      : key(path->str()), type(t), empty(false), builder(std::make_shared<VPackBuilder>()) {
    builder->add(VPackValue(v));
    value = builder->slice();
  }

 public:
  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  void toGeneralBuilder(arangodb::velocypack::Builder& builder) const;

 public:
  std::string key;
  Type type;
  bool empty;
  velocypack::Slice value;
  std::shared_ptr<VPackBuilder> builder;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   AgencyOperation
// -----------------------------------------------------------------------------

class AgencyOperation {
 public:
  explicit AgencyOperation(std::string const& key);

  AgencyOperation(std::string const& key, AgencySimpleOperationType opType);

  AgencyOperation(std::string const& key, AgencyValueOperationType opType,
                  velocypack::Slice const value);

  template <typename T>
  AgencyOperation(std::string const& key, AgencyValueOperationType opType, T const& value)
      : _key(AgencyCommHelper::path(key)),
        _opType(),
        _holder(std::make_shared<VPackBuilder>()) {
    _holder->add(VPackValue(value));
    _value = _holder->slice();
    _opType.type = AgencyOperationType::Type::VALUE;
    _opType.value = opType;
  }

  AgencyOperation(std::string const& key, AgencyValueOperationType opType,
                  velocypack::Slice const newValue, velocypack::Slice const oldValue);

 public:
  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  void toGeneralBuilder(arangodb::velocypack::Builder& builder) const;
  AgencyOperationType type() const;

 public:
  uint64_t _ttl = 0;

 private:
  std::string const _key;
  AgencyOperationType _opType;
  velocypack::Slice _value;
  velocypack::Slice _value2;
  std::shared_ptr<VPackBuilder> _holder;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  AgencyCommResult
// -----------------------------------------------------------------------------

class AgencyCommResult {
 public:
  AgencyCommResult() = default;
  AgencyCommResult(int code, std::string message);

  ~AgencyCommResult() = default;

  AgencyCommResult(AgencyCommResult const& other) = delete;
  AgencyCommResult& operator=(AgencyCommResult const& other) = delete;

  AgencyCommResult(AgencyCommResult&& other) noexcept;
  AgencyCommResult& operator=(AgencyCommResult&& other) noexcept;

 public:
  void set(int code, std::string message);

  [[nodiscard]] bool successful() const { return (_statusCode >= 200 && _statusCode <= 299); }

  [[nodiscard]] bool connected() const;

  [[nodiscard]] int httpCode() const;

  [[nodiscard]] int errorCode() const;

  [[nodiscard]] std::string errorMessage() const;

  [[nodiscard]] std::string errorDetails() const;

  [[nodiscard]] std::string const& location() const { return _location; }

  [[nodiscard]] std::string const& body() const { return _body; }

  [[nodiscard]] bool sent() const;

  void clear();

  [[nodiscard]] velocypack::Slice slice() const;

  void setVPack(std::shared_ptr<velocypack::Builder> const& vpack) {
    _vpack = vpack;
  }

  [[nodiscard]] Result asResult() const;

  void toVelocyPack(VPackBuilder& builder) const;

  [[nodiscard]] VPackBuilder toVelocyPack() const;

  [[nodiscard]] std::pair<std::optional<int>, std::optional<std::string_view>> parseBodyError() const;

 public:
  std::string _location = "";
  std::string _message = "";
  std::string _body = "";

  std::unordered_map<std::string, AgencyCommResultEntry> _values = {};
  int _statusCode = 0;
  bool _connected = false;
  bool _sent = false;

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

  std::string toJson() const;
  virtual void toVelocyPack(arangodb::velocypack::Builder&) const = 0;
  virtual std::string const& path() const = 0;
  virtual std::string getClientId() const = 0;

  virtual bool validate(AgencyCommResult const& result) const = 0;
  virtual char const* typeName() const = 0;
};

// -----------------------------------------------------------------------------
// --SECTION--                                          AgencyGeneralTransaction
// -----------------------------------------------------------------------------

/*struct AgencyGeneralTransaction : public AgencyTransaction {

  typedef std::pair<std::vector<AgencyOperation>,std::vector<AgencyPrecondition>> TransactionType;

  explicit AgencyGeneralTransaction(AgencyOperation const& op,
                                    AgencyPrecondition const& pre) :
    clientId(to_string(boost::uuids::random_generator()())) {
    transactions.emplace_back(
    TransactionType(std::vector<AgencyOperation>(1, op),
                    std::vector<AgencyPrecondition>(1, pre)));
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

  void push_back(AgencyOperation const& op);
  void push_back(std::pair<AgencyOperation, AgencyPrecondition> const&);

  inline virtual std::string const& path() const override final {
    return AgencyTransaction::TypeUrl[2];
  }

  inline virtual std::string getClientId() const override final {
    return clientId;
  }

  virtual bool validate(AgencyCommResult const& result) const override final;
  char const* typeName() const override { return "AgencyGeneralTransaction"; }
  std::string clientId;

};*/

// -----------------------------------------------------------------------------
// --SECTION--                                            AgencyWriteTransaction
// -----------------------------------------------------------------------------

struct AgencyWriteTransaction : public AgencyTransaction {
 public:
  static std::string randomClientId();

  explicit AgencyWriteTransaction(AgencyOperation const& operation)
      : clientId(randomClientId()) {
    operations.push_back(operation);
  }

  explicit AgencyWriteTransaction(std::vector<AgencyOperation> const& _opers)
      : operations(_opers), clientId(randomClientId()) {}

  AgencyWriteTransaction(AgencyOperation const& operation, AgencyPrecondition const& precondition)
      : clientId(randomClientId()) {
    operations.push_back(operation);
    preconditions.push_back(precondition);
  }

  AgencyWriteTransaction(std::vector<AgencyOperation> const& opers,
                         AgencyPrecondition const& precondition)
      : clientId(randomClientId()) {
    std::copy(opers.begin(), opers.end(), std::back_inserter(operations));
    preconditions.push_back(precondition);
  }

  AgencyWriteTransaction(AgencyOperation const& operation,
                         std::vector<AgencyPrecondition> const& precs)
      : clientId(randomClientId()) {
    operations.push_back(operation);
    std::copy(precs.begin(), precs.end(), std::back_inserter(preconditions));
  }

  AgencyWriteTransaction(std::vector<AgencyOperation> const& opers,
                         std::vector<AgencyPrecondition> const& precs)
      : clientId(randomClientId()) {
    std::copy(opers.begin(), opers.end(), std::back_inserter(operations));
    std::copy(precs.begin(), precs.end(), std::back_inserter(preconditions));
  }

  AgencyWriteTransaction() : clientId(randomClientId()) {}

  void toVelocyPack(arangodb::velocypack::Builder& builder) const override final;

  inline std::string const& path() const override final {
    return AgencyTransaction::TypeUrl[1];
  }

  inline std::string getClientId() const override final {
    return clientId;
  }

  bool validate(AgencyCommResult const& result) const override final;
  char const* typeName() const override { return "AgencyWriteTransaction"; }

  std::vector<AgencyPrecondition> preconditions;
  std::vector<AgencyOperation> operations;
  std::string clientId;
};

// -----------------------------------------------------------------------------
// --SECTION-- AgencyTransientTransaction
// -----------------------------------------------------------------------------

struct AgencyTransientTransaction : public AgencyTransaction {
 public:
  explicit AgencyTransientTransaction(AgencyOperation const& operation) {
    operations.push_back(operation);
  }

  explicit AgencyTransientTransaction(std::vector<AgencyOperation> const& _operations)
      : operations(_operations) {}

  AgencyTransientTransaction(AgencyOperation const& operation,
                             AgencyPrecondition const& precondition) {
    operations.push_back(operation);
    preconditions.push_back(precondition);
  }

  AgencyTransientTransaction(std::vector<AgencyOperation> const& opers,
                             AgencyPrecondition const& precondition) {
    std::copy(opers.begin(), opers.end(), std::back_inserter(operations));
    preconditions.push_back(precondition);
  }

  AgencyTransientTransaction(std::vector<AgencyOperation> const& opers,
                             std::vector<AgencyPrecondition> const& precs) {
    std::copy(opers.begin(), opers.end(), std::back_inserter(operations));
    std::copy(precs.begin(), precs.end(), std::back_inserter(preconditions));
  }

  AgencyTransientTransaction() = default;

  void toVelocyPack(arangodb::velocypack::Builder& builder) const override final;

  inline std::string const& path() const override final {
    return AgencyTransaction::TypeUrl[3];
  }

  inline std::string getClientId() const override final {
    return std::string();
  }

  bool validate(AgencyCommResult const& result) const override final;
  char const* typeName() const override { return "AgencyTransientTransaction"; }

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

  void toVelocyPack(arangodb::velocypack::Builder& builder) const override final;

  inline std::string const& path() const override final {
    return AgencyTransaction::TypeUrl[0];
  }

  inline std::string getClientId() const override final {
    return std::string();
  }

  bool validate(AgencyCommResult const& result) const override final;
  char const* typeName() const override { return "AgencyReadTransaction"; }

  std::vector<std::string> keys;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        AgencyComm
// -----------------------------------------------------------------------------

class AgencyComm {
 private:
  static std::string const AGENCY_URL_PREFIX;
  static uint64_t const INITIAL_SLEEP_TIME = 5000; // microseconds
  static uint64_t const MAX_SLEEP_TIME = 50000; // microseconds

#ifdef DEBUG_SYNC_REPLICATION
 public:
  static bool syncReplDebug;
#endif

 public:
  explicit AgencyComm(application_features::ApplicationServer&);

  AgencyCommResult sendServerState(double timeout);

  std::string version();

  AgencyCommResult dump();

  bool increaseVersion(std::string const& key) {
    AgencyCommResult result = increment(key);
    return result.successful();
  }

  AgencyCommResult createDirectory(std::string const&);

  AgencyCommResult setValue(std::string const&, std::string const&, double);

  AgencyCommResult setValue(std::string const&, arangodb::velocypack::Slice const&, double);

  AgencyCommResult setTransient(std::string const& key,
                                arangodb::velocypack::Slice const& slice, 
                                uint64_t ttl, double timeout);

  bool exists(std::string const&);

  AgencyCommResult getValues(std::string const&);

  AgencyCommResult removeValues(std::string const&, bool);

  AgencyCommResult increment(std::string const&);

  /// compares and swaps a single value in the backend the CAS condition is
  /// whether or not a previous value existed for the key
  AgencyCommResult casValue(std::string const&, arangodb::velocypack::Slice const&,
                            bool, double, double);

  /// compares and swaps a single value in the back end the CAS condition is
  /// whether or not the previous value for the key was identical to `oldValue`
  AgencyCommResult casValue(std::string const&, arangodb::velocypack::Slice const&,
                            arangodb::velocypack::Slice const&, double, double);

  uint64_t uniqid(uint64_t, double);

  AgencyCommResult registerCallback(std::string const& key, std::string const& endpoint);

  AgencyCommResult unregisterCallback(std::string const& key, std::string const& endpoint);

  bool lockRead(std::string const&, double, double);

  bool lockWrite(std::string const&, double, double);

  bool unlockRead(std::string const&, double);

  bool unlockWrite(std::string const&, double);

  AgencyCommResult sendTransactionWithFailover(AgencyTransaction const&,
                                               double timeout = 0.0);

  application_features::ApplicationServer& server();

  bool ensureStructureInitialized();

  AgencyCommResult sendWithFailover(arangodb::rest::RequestType, double,
                                    std::string const&, velocypack::Slice);

  static void buildInitialAnalyzersSlice(VPackBuilder& builder);

 private:
  bool lock(std::string const&, double, double, arangodb::velocypack::Slice const&);

  bool unlock(std::string const&, arangodb::velocypack::Slice const&, double);

  bool tryInitializeStructure();

  bool shouldInitializeStructure();

 private:
  application_features::ApplicationServer& _server;
  Histogram<log_scale_t<uint64_t>>& _agency_comm_request_time_ms;
};
}  // namespace arangodb

namespace std {
ostream& operator<<(ostream& o, arangodb::AgencyCommResult const& a);
}

#endif
