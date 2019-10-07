#ifndef ARANGOD_VOCBASE_METHODS_AQL_USER_FUNCTIONS_HANDLER_H
#define ARANGOD_VOCBASE_METHODS_AQL_USER_FUNCTIONS_HANDLER_H 1

#include "Basics/Result.h"

struct TRI_vocbase_t;

namespace arangodb {

namespace velocypack {

class Builder;
class Slice;

}  // namespace velocypack

// @brief locates functionName in the current databases _aqlFunctions and
// deletes it.
//   Reloads the global AQL function context on success.
// @param vocbase current database to work with
// @param functionName the case insensitive name of the function to delete
// @return success only on exact match
Result unregisterUserFunction(TRI_vocbase_t& vocbase, std::string const& functionName);

// @brief locates a group of user functions matching functionFilterPrefix and
//        removes them from _aqlFunctions
// @param vocbase current database to work with
// @param functionFilterPrefix the case insensitive name of the function to
// delete
// @param deleteCount return the number of deleted functions, 0 is ok.
// @return succeeds if >= 0 functions were successfully deleted.
Result unregisterUserFunctionsGroup(TRI_vocbase_t& vocbase, std::string const& functionFilterPrefix,
                                    int& deleteCount);

// @brief registers an aql function with the current database
// will pull v8 context from TLS, or allocate one from the context dealer.
// needs the V8 context to test the function to eventually throw errors.
// @param vocbase current database to work with
// @param userFunction an Object with the following attributes:
//    name: the case insensitive name of the user function.
//    code: the javascript code of the function body
//    isDeterministic: whether the function will return the same result on same
//    params
// @param replaceExisting set to true if the function replaced a previously
// existing one
// @return result object
Result registerUserFunction(TRI_vocbase_t& vocbase, velocypack::Slice userFunction,
                            bool& replacedExisting);

// @brief fetches [all functions | functions matching the functionFilterPrefix]
// @param vocbase current database to work with
// @param functionFilterPrefix if non-empty, only return functions matching this
// prefix
// @param result result payload:
//   creates a list containing objects with these attributes:
//    name: the case insensitive name of the user function.
//    code: the javascript code of the function body
//    isDeterministic: whether the function will return the same result on same
//    params
// @return result object
Result toArrayUserFunctions(TRI_vocbase_t& vocbase, std::string const& functionFilterPrefix,
                            velocypack::Builder& result);

}  // namespace arangodb

#endif
