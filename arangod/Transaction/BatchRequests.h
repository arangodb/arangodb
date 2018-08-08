#ifndef ARANGOD_TRANSACTION_BATCHREQUESTS_H
#define ARANGOD_TRANSACTION_BATCHREQUESTS_H 1

#include "Basics/VelocyPackHelper.h"
#include "Utils/OperationOptions.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace batch {

// Operations /////////////////////////////////////////////////////////////////
enum class Operation {
  READ,
  INSERT,
  REMOVE,
  REPLACE,
  UPDATE,
  UPSERT,
  REPSERT,
};

struct OperationHash {
  size_t operator()(Operation value) const noexcept {
    typedef std::underlying_type<decltype(value)>::type UnderlyingType;
    return std::hash<UnderlyingType>()(UnderlyingType(value));
  }
};

inline std::unordered_map<Operation, std::string, OperationHash> const&
getBatchToStingMap(){
  static const std::unordered_map<Operation, std::string, OperationHash>
  // NOLINTNEXTLINE(cert-err58-cpp)
  batchToStringMap{
      {Operation::READ, "read"},
      {Operation::INSERT, "insert"},
      {Operation::REMOVE, "remove"},
      {Operation::REPLACE, "replace"},
      {Operation::UPDATE, "update"},
      {Operation::UPSERT, "upsert"},
      {Operation::REPSERT, "repsert"},
  };
  return batchToStringMap;
};

inline std::string batchToString(Operation op) {
  auto const& map = getBatchToStingMap();
  return map.at(op);
}

inline std::unordered_map<std::string, Operation> ensureStringToBatchMapIsInitialized() {
 std::unordered_map<std::string, Operation> stringToBatchMap;
  for (auto const& it : getBatchToStingMap()) {
    stringToBatchMap.insert({it.second, it.first});
  }
  return stringToBatchMap;
}

inline Operation stringToBatch(std::string const& op) {
  static const auto stringToBatchMap = ensureStringToBatchMapIsInitialized();
  auto it = stringToBatchMap.find(op);
  if (it == stringToBatchMap.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "could not convert string to batch operation");
  }
  return it->second;
}

using DocumentSlice = VPackSlice;

struct InsertDoc{
  VPackSlice document;
};

struct RemoveDoc {
  std::string key;
  VPackSlice pattern;
};

struct DefaultDoc {
  std::string key;
  VPackSlice pattern;
  VPackSlice document;
};

struct ReadDoc : public DefaultDoc{};
struct UpdateDoc : public DefaultDoc{};
struct ReplaceDoc : public DefaultDoc{};

struct UpsertDoc {
  std::string key;
  VPackSlice pattern;
  VPackSlice insert;
  VPackSlice update;
};

struct RepsetDoc {
  std::string key;
  VPackSlice pattern;
  VPackSlice insert;
  VPackSlice replace;
};

template <typename T>
struct batchSlice;

template<>
struct batchSlice<RemoveDoc> {
  static std::pair<OperationOptions,std::vector<RemoveDoc>> fromVPack(VPackSlice slice);
};

template <typename DocType>
class Request {
  Request(VPackSlice slice) : _data() {
    std::tie(_options,_data) = batchSlice<DocType>::fromVPack(slice);
  };
  std::vector<DocType> _data;
  OperationOptions _options;
public:
  std::size_t size()   const { return _data.size(); };
  std::vector<DocType> const& data() const { return _data; }
  OperationOptions const& options() const { return _options; }
};



}
} //arangodb
#endif
