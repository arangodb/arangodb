#include "BatchRequests.h"
#include <unordered_map>
#include "Transaction/Methods.h"
#include "Basics/StaticStrings.h"

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
using AttributeMap = arangodb::basics::VelocyPackHelper::AttributeMap;

//    DATUM
//    required.insert("pattern");
//
//    switch (batchOp) {
//      case batch::Operation::READ:
//        break;
//      case batch::Operation::INSERT:
//        required.clear();
//        required.insert("insertDocument");
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
//    OPTIONS
//    OperationOptions options;
//      required = {};
//      optional = {"oneTransactionPerDOcument", "checkGraphs", "graphName"};
//      // mergeObjects "ignoreRevs"  "isRestore" keepNull?!
//      deprecated = {};
//      switch (batchOp) {
//        case batch::Operation::READ:
//          optional.insert("graphName");
//          break;
//        case batch::Operation::INSERT:
//        case batch::Operation::UPSERT:
//        case batch::Operation::REPSERT:
//          optional.insert("returnNew"); // please fall through
//          optional.insert("returnOld");
//          optional.insert("waitForSync");
//          optional.insert("silent");
//          break;
//      }

using VT = velocypack::ValueType;
using VH = basics::VelocyPackHelper;

auto batchSlice<RemoveDoc>::fromVPack(VPackSlice slice) -> OperationData<RemoveDoc> {
  AttributeSet required = { {"data", VT::Array} };
  AttributeSet optional = { {"options", VT::Object} } ;
  AttributeSet deprecated;

  auto const maybeAttributes = VH::expectedAttributes(slice, required, optional, deprecated, true);
  if(maybeAttributes.fail()){ return {maybeAttributes}; }

  ////////////////////////////////////////////////////////////////////////////////////////
  // data
  VPackSlice const& dataSlice = maybeAttributes.get().find("data")->second; //will not fail

  required.clear();
  optional.clear();
  deprecated.clear();


  std::vector<RemoveDoc> rv_vec;
  for (auto const& datum : VPackArrayIterator(dataSlice) ){
    if(!datum.hasKey("pattern")){
      return { TRI_ERROR_ARANGO_VALIDATION_FAILED };
    } else {
      VPackSlice pattern = datum.get("pattern");
      if (pattern.hasKey(::arangodb::StaticStrings::KeyString)) {
        VPackSlice key = pattern.get(::arangodb::StaticStrings::KeyString);
        rv_vec.emplace_back(RemoveDoc{key.copyString(), pattern});
      } else {
        return { TRI_ERROR_ARANGO_VALIDATION_FAILED };
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////
  // options
  OperationOptions options;
  if(maybeAttributes.get().find("options") != maybeAttributes.get().end()) {
    required.clear();
    optional = { {"oneTransactionPerDOcument", VT::Bool}
               , {"checkGraphs", VT::Bool}
               , {"graphName", VT::String}
               , {"waitForSync", VT::Bool}
               , {"returnOld", VT::Bool}
               , {"silent", VT::Bool}
               };

    VPackSlice const optionsSlice = slice.get("options");

    auto const maybeOptions = VH::expectedAttributes(optionsSlice, required, optional, deprecated, true);
    if (maybeOptions.fail()) {
      return prefixResultMessage(maybeOptions, "When parsing attribute 'options'");
    } else {
      options = createOperationOptions(optionsSlice);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////
  // result
  return OperationData<RemoveDoc>::success({rv_vec, options});
}

auto batchSlice<UpdateDoc>::fromVPack(VPackSlice slice) -> OperationData<UpdateDoc> {
  AttributeSet required = { {"data", VT::Array} };
  AttributeSet optional = { {"options", VT::Object} } ;
  AttributeSet deprecated;

  auto const maybeAttributes = VH::expectedAttributes(slice, required, optional, deprecated, true);
  if(maybeAttributes.fail()){ return {maybeAttributes}; }

  ////////////////////////////////////////////////////////////////////////////////////////
  // data
  VPackSlice const& dataSlice = maybeAttributes.get().find("data")->second; //will not fail

  required.clear();
  optional.clear();
  deprecated.clear();

  required.insert({"updateDocument", VT::Object});
  required.insert({"pattern", VT::Object});

  std::vector<UpdateDoc> rv_vec;
  for (auto const& datum : VPackArrayIterator(dataSlice) ){
    auto const& maybeDatum = VH::expectedAttributes(datum, required, optional, deprecated, true);
    if(maybeDatum.fail()){
      return { TRI_ERROR_ARANGO_VALIDATION_FAILED };
    } else {
      VPackSlice const& pattern = maybeDatum.get().find("pattern")->second;
      VPackSlice const& updateDocument = maybeDatum.get().find("updateDocument")->second;
      if (pattern.hasKey(::arangodb::StaticStrings::KeyString)) {
        VPackSlice key = pattern.get(::arangodb::StaticStrings::KeyString);
        rv_vec.emplace_back( PatternWithKeyAndDoc{key.copyString(), pattern, updateDocument});
      } else {
        return { TRI_ERROR_ARANGO_VALIDATION_FAILED };
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////
  // options
  OperationOptions options;
  if(maybeAttributes.get().find("options") != maybeAttributes.get().end()) {
    required.clear();
    optional = { {"oneTransactionPerDOcument", VT::Bool}
               , {"checkGraphs", VT::Bool}
               , {"graphName", VT::String}
               , {"keepNull", VT::Bool}
               , {"waitForSync", VT::Bool}
               , {"returnNew", VT::Bool}
               , {"returnOld", VT::Bool}
               , {"silent", VT::Bool}
               };

    VPackSlice const optionsSlice = slice.get("options");

    auto const maybeOptions = VH::expectedAttributes(optionsSlice, required, optional, deprecated, true);
    if (maybeOptions.fail()) {
      return prefixResultMessage(maybeOptions, "When parsing attribute 'options'");
    } else {
      options = createOperationOptions(optionsSlice);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////
  // result
  return OperationData<UpdateDoc>::success({rv_vec, options});
}

auto batchSlice<ReplaceDoc>::fromVPack(VPackSlice slice) -> OperationData<ReplaceDoc> {
  AttributeSet required = { {"data", VT::Array} };
  AttributeSet optional = { {"options", VT::Object} } ;
  AttributeSet deprecated;

  auto const maybeAttributes = VH::expectedAttributes(slice, required, optional, deprecated, true);
  if(maybeAttributes.fail()){ return {maybeAttributes}; }

  ////////////////////////////////////////////////////////////////////////////////////////
  // data
  VPackSlice const& dataSlice = maybeAttributes.get().find("data")->second; //will not fail

  required.clear();
  optional.clear();
  deprecated.clear();

  required.insert({"replaceDocument", VT::Object});
  required.insert({"pattern", VT::Object});

  std::vector<ReplaceDoc> rv_vec;
  for (auto const& datum : VPackArrayIterator(dataSlice) ){
    auto const& maybeDatum = VH::expectedAttributes(datum, required, optional, deprecated, true);
    if(maybeDatum.fail()){
      return { TRI_ERROR_ARANGO_VALIDATION_FAILED };
    } else {
      VPackSlice const& pattern = maybeDatum.get().find("pattern")->second;
      VPackSlice const& updateDocument = maybeDatum.get().find("replaceDocument")->second;
      if (pattern.hasKey(::arangodb::StaticStrings::KeyString)) {
        VPackSlice key = pattern.get(::arangodb::StaticStrings::KeyString);
        rv_vec.emplace_back(PatternWithKeyAndDoc{key.copyString(), pattern, updateDocument});
      } else {
        return { TRI_ERROR_ARANGO_VALIDATION_FAILED };
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////
  // options
  OperationOptions options;
  if(maybeAttributes.get().find("options") != maybeAttributes.get().end()) {
    required.clear();
    optional = { {"oneTransactionPerDOcument", VT::Bool}
               , {"checkGraphs", VT::Bool}
               , {"graphName", VT::String}
               , {"waitForSync", VT::Bool}
               , {"returnNull", VT::Bool}
               , {"returnOld", VT::Bool}
               , {"silent", VT::Bool}
               };

    VPackSlice const optionsSlice = slice.get("options");

    auto const maybeOptions = VH::expectedAttributes(optionsSlice, required, optional, deprecated, true);
    if (maybeOptions.fail()) {
      return prefixResultMessage(maybeOptions, "When parsing attribute 'options'");
    } else {
      options = createOperationOptions(optionsSlice);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////
  // result
  return OperationData<ReplaceDoc>::success({rv_vec, options});
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
