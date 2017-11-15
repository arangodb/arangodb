////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Graphs.h"

#include "Aql/Graphs.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterMethods.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Transaction/Context.h"
#include <sstream>
using namespace arangodb;

#ifndef USE_ENTERPRISE
std::string const GRAPHS = "_graphs";

////////////////////////////////////////////////////////////////////////////////
/// @brief Load a graph from the _graphs collection; local and coordinator way
////////////////////////////////////////////////////////////////////////////////

arangodb::aql::Graph* arangodb::lookupGraphByName(std::shared_ptr<transaction::Context> transactionContext,
                                                  std::string const& name) {
  SingleCollectionTransaction trx(transactionContext, GRAPHS, AccessMode::Type::READ);

  Result res = trx.begin();

  if (!res.ok()) {
    std::stringstream ss;
    ss <<  "while looking up graph '" << name << "': " << res.errorMessage();
    res.reset(res.errorNumber(), ss.str());
    THROW_ARANGO_EXCEPTION(res);
  }
  VPackBuilder b;
  {
    VPackObjectBuilder guard(&b);
    b.add(StaticStrings::KeyString, VPackValue(name));
  }

  // Default options are enough here
  OperationOptions options;

  OperationResult result = trx.document(GRAPHS, b.slice(), options);

  // Commit or abort.
  res = trx.finish(result.code);

  if (!result.successful()) {
    THROW_ARANGO_EXCEPTION_FORMAT(result.code, "while looking up graph '%s'",
                                  name.c_str());
  }
  if (!res.ok()) {
    std::stringstream ss;
    ss <<  "while looking up graph '" << name << "': " << res.errorMessage();
    res.reset(res.errorNumber(), ss.str());
    THROW_ARANGO_EXCEPTION(res);
  }

  VPackSlice info = result.slice();
  if (info.isExternal()) {
    info = info.resolveExternal();
  }

  return new arangodb::aql::Graph(info);
}
#endif
