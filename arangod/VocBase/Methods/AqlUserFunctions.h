#ifndef ARANGOD_VOCBASE_METHODS_AQL_USER_FUNCTIONS_HANDLER_H
#define ARANGOD_VOCBASE_METHODS_AQL_USER_FUNCTIONS_HANDLER_H 1

#include "Basics/Result.h"

struct TRI_vocbase_t;

namespace arangodb {
  namespace velocypack {
    class Builder;
    class Slice;
    
  }
  // will do some AQLFOO: and return true if it deleted one 
  Result unregisterUserFunction(
                                TRI_vocbase_t* vocbase,
                                std::string const& functionName
                                );
  
  // will do some AQLFOO, and return the number of deleted
  Result unregisterUserFunctionsGroup(
                                      TRI_vocbase_t* vocbase,
                                      std::string const& functionFilterPrefix,
                                      int& deleteCount
                                      );


  // documentation: will pull v8 context, or allocate if non already used
  
  // needs the V8 context to test the function to throw evenual errors:
  Result registerUserFunction(
                              TRI_vocbase_t* vocbase,
                              //// todo : drinnen: isolate get current / oder neues v8::Isolate*,
                              velocypack::Slice userFunction,

                              bool& replacedExisting
                              );


  // todo: document which result builder state is expected
  // will do some AQLFOO:
  Result toArrayUserFunctions(
                              TRI_vocbase_t* vocbase,
                              std::string const& functionFilterPrefix,
                              velocypack::Builder& result
                              );
}
#endif
