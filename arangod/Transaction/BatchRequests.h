#ifndef ARANGOD_TRANSACTION_BATCHREQUESTS_H
#define ARANGOD_TRANSACTION_BATCHREQUESTS_H 1

#include "boost/optional.hpp"

#include "Basics/VelocyPackHelper.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"

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

struct ReadDoc : public PatternWithKeyAndDoc{
  template<typename ...Args> ReadDoc(Args&& ...args)
    : PatternWithKeyAndDoc{std::forward<Args>(args)...}{}
};

struct UpdateDoc : public PatternWithKeyAndDoc{
  template<typename ...Args> UpdateDoc(Args&& ...args)
    : PatternWithKeyAndDoc{std::forward<Args>(args)...}{}
};

struct ReplaceDoc : public PatternWithKeyAndDoc{
  template<typename ...Args> ReplaceDoc(Args&& ...args)
    : PatternWithKeyAndDoc{std::forward<Args>(args)...}{}
};

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

template <typename T> class Request;

template<typename DocType>
ResultT<Request<DocType>> createRequestFromSlice(VPackSlice slice){
  auto maybeData = batchSlice<DocType>::fromVPack(slice); // replace with constexpr if() and normal functions
  if(maybeData.fail()){
    return { maybeData };
  }

  auto data = maybeData.get();
  return ResultT<Request<DocType>>::success(
      Request<DocType>(std::move(data.first), std::move(data.second))
  );
};

//// replace with normal functions when `constexpr if()` is availalbe
template <typename DocType>
class Request {
  Request(std::vector<DocType>&& data, OperationOptions&& opts) : _data(std::move(data)), _options(std::move(opts)) {
  };
  std::vector<DocType> _data;
  OperationOptions _options;
public:
  template <typename T> friend ResultT<Request<T>> createRequestFromSlice(VPackSlice slice);
  std::size_t size()   const { return _data.size(); };
  std::vector<DocType> const& data() const { return _data; }
  OperationOptions const& options() const { return _options; }
  OperationResult execute(transaction::Methods *trx, std::string const& collection) {
    return batchSlice<DocType>::execute(trx, collection, *this);
  }
};

template<typename T> using OperationData = ResultT<std::pair<std::vector<T>,OperationOptions>>;

template<>
struct batchSlice<RemoveDoc> {
  static auto fromVPack(VPackSlice slice) -> OperationData<RemoveDoc> ;
  static OperationResult execute(transaction::Methods* trx, std::string const& collection, Request<RemoveDoc> const& request);
};

template<>
  struct batchSlice<UpdateDoc> {
  static auto fromVPack(VPackSlice slice) -> OperationData<UpdateDoc> ;
  static OperationResult execute(transaction::Methods* trx, std::string const& collection, Request<UpdateDoc> const& request);
};

template<>
struct batchSlice<ReplaceDoc> {
  static auto fromVPack(VPackSlice slice) -> OperationData<ReplaceDoc>;
  static OperationResult execute(transaction::Methods* trx, std::string const& collection, Request<ReplaceDoc> const& request);
};

} // batch
} // arangodb
#endif
