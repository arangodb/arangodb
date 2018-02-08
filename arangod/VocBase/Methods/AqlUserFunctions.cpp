////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#include "AqlUserFunctions.h"



#include "Basics/StringUtils.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"

#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Aql/QueryString.h"

#include "RestServer/QueryRegistryFeature.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8Server/V8DealerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/ManagedDocumentResult.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"


#include <v8.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <regex>

using namespace arangodb;

namespace {
std::string collectionName("_aqlfunctions");
  // Must not start with `_`, may contain alphanumerical characters, should have at least one set of double colons followed by more alphanumerical characters.
std::regex const funcRegEx("[a-zA-Z0-9][a-zA-Z0-9_]*(::[a-zA-Z0-9_]+)+", std::regex::ECMAScript);
  // we may filter less restrictive:
std::regex const funcFilterRegEx("[a-zA-Z0-9_]+(::[a-zA-Z0-9_]*)+", std::regex::ECMAScript);

bool isValidFunctionName(std::string const& testName) {
  return std::regex_match(testName, funcRegEx);
}

bool isValidFunctionNameFilter(std::string const& testName) {
  return std::regex_match(testName, funcFilterRegEx);
}


void reloadAqlUserFunctions() {
  std::string const def("reloadAql");

  V8DealerFeature::DEALER->addGlobalContextMethod(def);
}

}

// will do some AQLFOO: and return true if it deleted one
Result arangodb::unregisterUserFunction(TRI_vocbase_t* vocbase,
                                        std::string const& functionName) {
  if (functionName.empty() || !isValidFunctionNameFilter(functionName)) {
    return Result(TRI_ERROR_QUERY_FUNCTION_INVALID_NAME,
                  std::string("Error deleting AQL User Function: '") +
                  functionName +
                  "' contains invalid characters");
  }

  std::string aql("RETURN LENGTH( "
                  " FOR fn IN @@col"
                  "  FILTER fn._key == @fnName"
                  "  REMOVE { _key: fn._key } in @@col RETURN 1)");

  std::shared_ptr<VPackBuilder> binds;
  binds.reset(new VPackBuilder);
  binds->openObject();
  std::string UCFN = basics::StringUtils::toupper(functionName);

  binds->add("fnName", VPackValue(UCFN));
  binds->add("@col", VPackValue(collectionName));
  binds->close();  // obj

  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(aql),
                             binds, nullptr, arangodb::aql::PART_MAIN);

  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
  auto queryResult = query.execute(queryRegistry);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    /// TODO reload?
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
      return Result(TRI_ERROR_REQUEST_CANCELED);
    }
    return Result(queryResult.code,
                  std::string("Error group-deleting user defined AQL"));
  }

  VPackSlice countSlice = queryResult.result->slice();
  if (!countSlice.isArray() || (countSlice.length() != 1)) {
    return Result(TRI_ERROR_INTERNAL, "bad query result for deleting AQL User Functions");
  }

  if (countSlice[0].getInt() != 1) {
    return Result(TRI_ERROR_QUERY_FUNCTION_NOT_FOUND,
                  std::string("no AQL User Function by name '") + functionName + "' found");
  }

  reloadAqlUserFunctions();
  return Result();
}

// will do some AQLFOO, and return the number of deleted
Result arangodb::unregisterUserFunctionsGroup(TRI_vocbase_t* vocbase,
                                    std::string const& functionFilterPrefix,
                                    int& deleteCount) {
  deleteCount = 0;
  if (functionFilterPrefix.empty()) {
    return Result(TRI_ERROR_BAD_PARAMETER);
  }
  if (!isValidFunctionNameFilter(functionFilterPrefix)) {
    return Result(TRI_ERROR_QUERY_FUNCTION_INVALID_NAME,
                  std::string("Error deleting AQL User Function: '") +
                  functionFilterPrefix +
                  "' contains invalid characters");
  }

  std::string filter;
  std::shared_ptr<VPackBuilder> binds;
  binds.reset(new VPackBuilder);
  binds->openObject();

  std::string uc(functionFilterPrefix);
  basics::StringUtils::toupperInPlace(&uc);

  if ((uc.length() < 2) ||
      (uc[uc.length() - 1 ] != ':') ||
      (uc[uc.length() - 2 ] != ':')) {
    uc += "::";
  }
  binds->add("fnLength", VPackValue(uc.length()));
  binds->add("ucName", VPackValue(uc));
  binds->add("@col", VPackValue(collectionName));

  binds->close();  // obj

  std::string aql("RETURN LENGTH("
                  " FOR fn IN @@col"
                  "  FILTER UPPER(LEFT(fn.name, @fnLength)) == @ucName"
                  "  REMOVE { _key: fn._key} in @@col RETURN 1)");

  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(aql),
                             binds, nullptr, arangodb::aql::PART_MAIN);

  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
  auto queryResult = query.execute(queryRegistry);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
      return Result(TRI_ERROR_REQUEST_CANCELED);
    }
    return Result(queryResult.code,
                  std::string("Error group-deleting user defined AQL"));
  }

  VPackSlice countSlice = queryResult.result->slice();
  if (!countSlice.isArray() || (countSlice.length() != 1)) {
    return Result(TRI_ERROR_INTERNAL, "bad query result for deleting AQL User Functions");
  }

  deleteCount = countSlice[0].getInt();
  reloadAqlUserFunctions();
  return Result();
}


// documentation: will pull v8 context, or allocate if non already used

// needs the V8 context to test the function to throw evenual errors:
Result arangodb::registerUserFunction(TRI_vocbase_t* vocbase,
                                      //// todo : drinnen: isolate get current / oder neues v8::Isolate*,
                                      velocypack::Slice userFunction,
                                      bool& replacedExisting
                                      ) {
  Result res;
  auto vname = userFunction.get("name");
  if (!vname.isString()) {
    return Result(); //TODO
  }

  std::string name = vname.copyString();

  if (!isValidFunctionName(name)) {
    return Result(TRI_ERROR_QUERY_FUNCTION_INVALID_NAME,
                  std::string("Error creating AQL User Function: '") +
                  name +
                  "' is not a valid name.");
  }
  auto cvString = userFunction.get("code");
  if (!cvString.isString()) {
        return Result(TRI_ERROR_QUERY_FUNCTION_INVALID_CODE,
                      "expecting function or string");
  }
  std::string tmp = cvString.copyString();

  std::string code = std::string("(") + tmp + "\n)";
  bool isDeterministic = userFunction.get("isDeterministic").getBool();
  {
    ISOLATE;
    bool throwV8Exception = (isolate != nullptr);
    V8ContextDealerGuard dealerGuard(res, &isolate, vocbase, true /*allowModification*/);
    if(res.fail()){
      return res;
    }

    std::string testCode("(function() { var callback = ");
    testCode += tmp + "; return callback; })()";
    v8::HandleScope scope(isolate);


    v8::Handle<v8::Value> result;
    {
      v8::TryCatch tryCatch;

      result = TRI_ExecuteJavaScriptString(isolate,
                                           isolate->GetCurrentContext(),
                                           TRI_V8_STD_STRING(isolate, testCode),
                                           TRI_V8_ASCII_STRING(isolate, "userFunction"),
                                           false);


      if (result.IsEmpty() || tryCatch.HasCaught() || !result->IsFunction()) {
        res.reset(TRI_ERROR_QUERY_FUNCTION_INVALID_CODE,
            TRI_StringifyV8Exception(isolate, &tryCatch));
        if (!tryCatch.CanContinue()) {
          if (throwV8Exception) {
            tryCatch.ReThrow();
          }
          TRI_GET_GLOBALS();
          v8g->_canceled = true;
        }
            // return Result(TRI_ERROR_QUERY_FUNCTION_INVALID_CODE, "failed to test the function you wanted to save");
      }
    }
  }
  if (!res.ok()) {
    return res;
  }
  std::string _key(name);
  basics::StringUtils::toupperInPlace(&_key);

  //      name: name,
  // code: params.code,
  // isDeterministic: params.isDeterministic || false

  VPackBuilder oneFunctionDocument;
  // TODO: replacedExisting
  oneFunctionDocument.openObject();
  oneFunctionDocument.add("_key", VPackValue(_key));
  oneFunctionDocument.add("name", VPackValue(name));
  oneFunctionDocument.add("code", VPackValue(code));
  oneFunctionDocument.add("isDeterministic", VPackValue(isDeterministic));
  oneFunctionDocument.close();

  arangodb::OperationOptions opOptions;
  opOptions.isRestore = false;
  opOptions.waitForSync = true;
  opOptions.returnNew = false;
  opOptions.silent = false;
  opOptions.isSynchronousReplicationFrom = true; /// TODO

  // find and load collection given by name or identifier
  auto ctx = transaction::StandaloneContext::Create(vocbase);
  SingleCollectionTransaction trx(ctx, collectionName, AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  res = trx.begin();

  if (!res.ok()) {
    // TODO: this comes from the RestVocbaseBaseHandler generateTransactionError(collectionName, res, "");
    return res;
  }

  std::unique_ptr<ManagedDocumentResult> mmdr;
  transaction::BuilderLeaser searchBuilder(&trx);
  searchBuilder->add(VPackValue(_key));
  arangodb::velocypack::Builder existDocument;
  try {
    res = trx.documentFastPath(collectionName, mmdr.get(), searchBuilder->slice(), existDocument, false);
  } catch (arangodb::basics::Exception const& ex) {
    res.reset(ex.code());
  }

  arangodb::OperationResult result;
  replacedExisting = res.ok();
  if (replacedExisting){
    result = trx.replace(collectionName, oneFunctionDocument.slice(), opOptions);
  } else {
    result = trx.insert(collectionName, oneFunctionDocument.slice(), opOptions);
  }
  // Will commit if no error occured.
  // or abort if an error occured.
  // result stays valid!
  res = trx.finish(result.result);

  if (result.fail()) {
    printf("resultfail\n");
    // TODO: this comes from the RestVocbaseBaseHandler     generateTransactionError(result);
    return res;
  }

  if (res.fail()) {
    printf("result not ok\n");
    // TODO: this comes from the RestVocbaseBaseHandler     generateTransactionError(collectionName, res, "");
    return res;
  }

  reloadAqlUserFunctions();
  // Note: Adding user functions to the _aql namespace is disallowed and will fail.
  return res;
}

// todo: document which result builder state is expected
// will do some AQLFOO:
Result arangodb::toArrayUserFunctions(TRI_vocbase_t* vocbase,
                                      std::string const& functionFilterPrefix,
                                      velocypack::Builder& result) {
  /*
  if (!functionFilterPrefix.empty() && !isValidFunctionName(functionFilterPrefix)) {
    return Result(TRI_ERROR_QUERY_FUNCTION_INVALID_NAME,
                  std::string("Error filtering AQL User Function: '") +
                  functionFilterPrefix +
                  "' contains invalid characters");
  }
  */
  std::string filter;

  std::shared_ptr<VPackBuilder> binds;
  binds.reset(new VPackBuilder);
  binds->openObject();

  if (! functionFilterPrefix.empty()) {
    std::string uc(functionFilterPrefix);
    basics::StringUtils::toupperInPlace(&uc);

    if ((uc.length() < 2) ||
        (uc[uc.length() - 1 ] != ':') ||
        (uc[uc.length() - 2 ] != ':')) {
      uc += "::";
    }
    filter = std::string(" FILTER LEFT(function._key, @fnLength) == @ucName ");
    binds->add("fnLength", VPackValue(uc.length()));
    binds->add("ucName", VPackValue(uc));
  }
  binds->add("@col", VPackValue(collectionName));
  binds->close();  // obj

  std::string aql("FOR function IN @@col");
  aql.append(filter + "RETURN function");

  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(aql),
                             binds, nullptr, arangodb::aql::PART_MAIN);

  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
  auto queryResult = query.execute(queryRegistry);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
      return Result(TRI_ERROR_REQUEST_CANCELED);
    }
    return Result(queryResult.code,
                  std::string("Error fetching user defined AQL functions with this query: ") +
                  queryResult.details);
  }

  auto usersFunctionsSlice = queryResult.result->slice();

  if (!usersFunctionsSlice.isArray()) {
    return Result(TRI_ERROR_INTERNAL, "bad query result for AQL User Functions");
  }
  result.openArray();
  std::string tmp;
  for (auto const& it : VPackArrayIterator(usersFunctionsSlice)) {
    VPackSlice itt;
    if (it.isExternal()) {
        itt = it.resolveExternal();
    }
    else {
      itt = it;
    }

    if (!itt.isObject()) {
      /// std::cout << "it is not an object? "<< itt.toJson() << " but rather: " << itt.typeName() << std::endl;
    }
    auto name = itt.get("name");
    auto fn = itt.get("code");
    auto ref=StringRef(fn);
    if (name.isString() && fn.isString() && (ref.length() > 2)) {
      ref = ref.substr(1, ref.length() - 2);
      tmp = basics::StringUtils::trim(ref.toString());

      VPackBuilder oneFunction;
      oneFunction.openObject();
      oneFunction.add("name", name);
      oneFunction.add("code", VPackValue(tmp));
      oneFunction.close();
      result.add(oneFunction.slice());
    }
  }
  result.close();
  return Result();
}
