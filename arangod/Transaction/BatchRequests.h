#ifndef ARANGOD_TRANSACTION_BATCHREQUESTS_H
#define ARANGOD_TRANSACTION_BATCHREQUESTS_H 1

#include "boost/optional.hpp"

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

std::string batchToString(Operation op);
boost::optional<Operation> stringToBatch(std::string const& op);

using DocumentSlice = VPackSlice;

struct InsertDoc{
  VPackSlice document;
};

struct RemoveDoc {
  std::string key;
  VPackSlice pattern;
};

struct PatternWithKeyAndDoc {
  std::string key;
  VPackSlice pattern;
  VPackSlice document;
};

struct ReadDoc : public PatternWithKeyAndDoc{};
struct UpdateDoc : public PatternWithKeyAndDoc{};
struct ReplaceDoc : public PatternWithKeyAndDoc{};

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

template<>
struct batchSlice<UpdateDoc> {
  static std::pair<OperationOptions,std::vector<UpdateDoc>> fromVPack(VPackSlice slice);
};

template<>
struct batchSlice<ReplaceDoc> {
  static std::pair<OperationOptions,std::vector<ReplaceDoc>> fromVPack(VPackSlice slice);
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
