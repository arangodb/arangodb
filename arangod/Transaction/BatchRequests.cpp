#include "BatchRequests.h"
#include <unordered_map>
#include "Transaction/Methods.h"

namespace arangodb {
namespace batch {

namespace {

// Operation Helper

struct OperationHash {
  size_t operator()(Operation value) const noexcept {
    typedef std::underlying_type<decltype(value)>::type UnderlyingType;
    return std::hash<UnderlyingType>()(UnderlyingType(value));
  }
};

static
std::unordered_map<Operation, std::string, OperationHash> const&
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

static
std::unordered_map<std::string, Operation>
ensureStringToBatchMapIsInitialized() {
 std::unordered_map<std::string, Operation> stringToBatchMap;
  for (auto const& it : getBatchToStingMap()) {
    stringToBatchMap.insert({it.second, it.first});
  }
  return stringToBatchMap;
}

}

std::string batchToString(Operation op) {
  auto const& map = getBatchToStingMap();
  return map.at(op);
}

boost::optional<Operation> stringToBatch(std::string const& op) {
  static const auto stringToBatchMap = ensureStringToBatchMapIsInitialized();
  auto it = stringToBatchMap.find(op);
  if (it == stringToBatchMap.end()) {
    return boost::none;
  }
  return it->second;
}

using AttributeSet = arangodb::basics::VelocyPackHelper::AttributeSet;


//ResultT<OperationOptions>
//optionsFromVelocypack(VPackSlice const optionsSlice
//                     ,AttributeSet const& required
//                     ,AttributeSet const& optional
//                     ,AttributeSet const& deprecated
//                     ){
//  Result res = expectedAttributes(optionsSlice, required, optional, deprecated);
//  if (res.fail()) { return prefixResultMessage(res, "Error occured while pasing options for batchDocumentOperation: "); }
//  OperationOptions options = createOperationOptions(optionsSlice);
//  return ResultT<OperationOptions>::success(std::move(options));
//};
//
//
//static ResultT<BatchRequest> fromVelocypack(VPackSlice const slice, batch::Operation batchOp) {
//
//    AttributeSet required = {"data"};
//    AttributeSet optional = {"options"};
//    AttributeSet deprecated;
//
//    auto const maybeAttributes = expectedAttributes(slice, required, optional, deprecated);
//    if(maybeAttributes.fail()){ return {maybeAttributes}; }
//
//    ////////////////////////////////////////////////////////////////////////////////////////
//    // data
//    VPackSlice const dataSlice = slice.get("data"); // data is requuired
//
//    required.clear();
//    optional.clear();
//    deprecated.clear();
//
//    required.insert("pattern");
//
//    switch (batchOp) {
//      case batch::Operation::READ:
//      case batch::Operation::REMOVE:
//        break;
//      case batch::Operation::INSERT:
//        required.clear();
//        required.insert("insertDocument");
//        break;
//      case batch::Operation::REPLACE:
//        required.insert("replaceDocument");
//        break;
//      case batch::Operation::UPDATE:
//        required.insert("updateDocument");
//        break;
//      case batch::Operation::UPSERT:
//        required.insert("insertDocument");
//        required.insert("updateDocument");
//        break;
//      case batch::Operation::REPSERT:
//        required.insert("replaceDocument");
//        required.insert("updateDocument");
//        break;
//    }
//
//    auto maybeData = CheckAttributesInVelocypackArray(dataSlice, required, optional, deprecated);
//    if (maybeData.fail()) {
//      return prefixResultMessage(maybeData, "When parsing attribute 'data'");
//    }
//
//    ////////////////////////////////////////////////////////////////////////////////////////
//    // options
//    OperationOptions options;
//
//    if(maybeAttributes.get().find("options") != maybeAttributes.get().end()) {
//      required = {};
//      optional = {"oneTransactionPerDOcument", "checkGraphs", "graphName"};
//      // mergeObjects "ignoreRevs"  "isRestore" keepNull?!
//      deprecated = {};
//      switch (batchOp) {
//        case batch::Operation::READ:
//          optional.insert("graphName");
//          break;
//        case batch::Operation::UPDATE:
//          optional.insert("keepNull"); // please fall through
//        case batch::Operation::INSERT:
//        case batch::Operation::UPSERT:
//        case batch::Operation::REPSERT:
//        case batch::Operation::REPLACE:
//          optional.insert("returnNew"); // please fall through
//        case batch::Operation::REMOVE:
//          optional.insert("waitForSync");
//          optional.insert("returnOld");
//          optional.insert("silent");
//          break;
//      }
//      VPackSlice const optionsSlice = slice.get("options");
//
//      auto const maybeOptions = optionsFromVelocypack(optionsSlice, required, optional, deprecated);
//      if (maybeOptions.fail()) {
//        return prefixResultMessage(maybeOptions, "When parsing attribute 'options'");
//      }
//      options = maybeOptions.get();
//    }
//
//    // create
//    return BatchRequest(slice, std::move(options), batchOp);
//  }
//}


// request parsing and verification
auto batchSlice<RemoveDoc>::fromVPack(VPackSlice slice) -> OperationData<RemoveDoc> {
  return {TRI_ERROR_ARANGO_VALIDATION_FAILED};
}
auto batchSlice<UpdateDoc>::fromVPack(VPackSlice slice) -> OperationData<UpdateDoc> {
  return {TRI_ERROR_ARANGO_VALIDATION_FAILED};
}

auto batchSlice<ReplaceDoc>::fromVPack(VPackSlice slice) -> OperationData<ReplaceDoc> {
  return {TRI_ERROR_ARANGO_VALIDATION_FAILED};
}

// execute
OperationResult batchSlice<RemoveDoc>::execute(transaction::Methods* trx, std::string const& collection, Request<RemoveDoc> const& request){
    //return trx->remove(collection, request);
    return {};
}

OperationResult batchSlice<UpdateDoc>::execute(transaction::Methods* trx, std::string const& collection, Request<UpdateDoc> const& request){
    //return trx->update(collection, request);
    return {};
}

OperationResult batchSlice<ReplaceDoc>::execute(transaction::Methods* trx, std::string const& collection, Request<ReplaceDoc> const& request){
    //return trx->replace(collection, request);
    return {};
}

}}
