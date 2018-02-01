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
#include "Basics/VelocyPackHelper.h"

#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Aql/QueryString.h"

#include "RestServer/QueryRegistryFeature.h"

#include <v8.h>
#include <velocypack/Builder.h>
//#include <velocypack/Iterator.h>
//#include <velocypack/velocypack-aliases.h>

using namespace arangodb;


static bool isValidFunctionName(std::string const& testName) {
  std::locale loc;
  std::string str="C++";

  for (std::string::iterator it=str.begin(); it!=str.end(); ++it)
  {
    if (! std::isalpha(*it,loc) && (*it != ':') )
      return false;
  }
  return true;
}

static std::shared_ptr<VPackBuilder> QueryAllFunctions(
                                                       TRI_vocbase_t* vocbase,
                                                       std::string const& filterBy,
                                                       bool SubString
) {
  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
  std::string filter;
  if (! filterBy.empty()) {
    if (!isValidFunctionName(filterBy)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_FUNCTION_INVALID_NAME,
                                     "Error fetching AQL User Function: '" +
                                     filterBy +
                                     "' contains invalid characters");
    }
    std::string lc(filterBy);
    basics::StringUtils::tolowerInPlace(&lc);

    if (SubString){
      filter = std::string("LOWER(LEFT(function.name, ");
      filter.append(basics::StringUtils::itoa(filterBy.length()));
      filter.append(") "") == '");
      filter.append(lc);
      filter.append("'");
    } else {
      filter = std::string("LOWER(function.name) == '");
      filter.append(lc);
      filter.append("'");
    }
  }
  
  std::string aql("FOR function IN _aqlfunctions ");
  aql.append(filter);
  aql.append(" RETURN function");

  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(aql),
                             nullptr, nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(queryRegistry);
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
                                   queryResult.code, "Error executing user query: " + queryResult.details);
  }

  VPackSlice usersFunctionsSlice = queryResult.result->slice();

  if (usersFunctionsSlice.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  } else if (!usersFunctionsSlice.isArray()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "cannot read user functions from _aqlfunctions collection";
    return std::shared_ptr<VPackBuilder>();
  }

  return queryResult.result;
}

// will do some AQLFOO: and return true if it deleted one 
Result unregisterUserFunction(
                              TRI_vocbase_t* vocbase,
                              std::string const& functionName
                              )
{
  auto existingFunction = QueryAllFunctions(vocbase, functionName, true);
}
  
// will do some AQLFOO, and return the number of deleted
Result unregisterUserFunctionsGroup(
                                    TRI_vocbase_t* vocbase,
                                    std::string const& functionFilterPrefix,
                                    int& deleteCount
                                    )
{
  auto existingFunction = QueryAllFunctions(vocbase, functionFilterPrefix, false);
}


// documentation: will pull v8 context, or allocate if non already used
  
// needs the V8 context to test the function to throw evenual errors:
Result registerUserFunction(
                            TRI_vocbase_t* vocbase,
                            //// todo : drinnen: isolate get current / oder neues v8::Isolate*,
                            velocypack::Slice userFunction,

                            bool& replacedExisting
                            )
{// Note: Adding user functions to the _aql namespace is disallowed and will fail.
}


// todo: document which result builder state is expected
// will do some AQLFOO:
Result toArrayUserFunctions(
                            TRI_vocbase_t* vocbase,
                            std::string const& functionFilterPrefix,
                            velocypack::Builder& result)
{
}

