////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Function.h"

#include "ApplicationServerHelper.h"

namespace arangodb {
namespace iresearch {

bool addFunction(arangodb::aql::AqlFunctionFeature& functions,
                 arangodb::aql::Function const& function) {
  // check that a function by the given name is not registred to avoid
  // triggering an assert inside AqlFunctionFeature::add(...)
  if (functions.exists(function.name)) {
    return false;
  }

  functions.add(function);

  return true;
}

arangodb::aql::Function const* getFunction(arangodb::aql::AqlFunctionFeature& functions,
                                           std::string const& name) {
  // if a function cannot be found then return nullptr instead of throwing
  // exception
  try {
    return functions.byName(name);
  } catch (arangodb::basics::Exception& e) {
    if (TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN != e.code()) {
      throw;  // not a missing function exception
    }
  }

  return nullptr;
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
