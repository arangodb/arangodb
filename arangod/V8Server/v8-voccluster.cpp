////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-vocbase.h"

#include "Ahuacatl/ahuacatl-codegen.h"
#include "Ahuacatl/ahuacatl-collections.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-explain.h"
#include "Ahuacatl/ahuacatl-result.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Query.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/json-utilities.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "CapConstraint/cap-constraint.h"
#include "FulltextIndex/fulltext-index.h"
#include "HttpServer/ApplicationEndpointServer.h"
#include "Replication/InitialSyncer.h"
#include "Rest/SslInterface.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "Utils/AhuacatlGuard.h"
#include "Utils/AhuacatlTransaction.h"
#include "Utils/DocumentHelper.h"
#include "Utils/transactions.h"
#include "Utils/V8ResolverGuard.h"
#include "V8/v8-conv.h"
#include "V8/v8-execution.h"
#include "V8/v8-utils.h"
#include "Wal/LogfileManager.h"
#include "VocBase/auth.h"
#include "VocBase/datafile.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/general-cursor.h"
#include "VocBase/key-generator.h"
#include "VocBase/replication-applier.h"
#include "VocBase/replication-dump.h"
#include "VocBase/server.h"
#include "VocBase/voc-shaper.h"
#include "VocBase/index.h"
#include "v8.h"
#include "V8/JSLoader.h"
#include "Basics/JsonHelper.h"
#include "Cluster/AgencyComm.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"

#include "unicode/timezone.h"
#include "unicode/utypes.h"
#include "unicode/datefmt.h"
#include "unicode/smpdtfmt.h"
#include "unicode/dtfmtsym.h"

#include "v8-vocbaseprivate.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;
