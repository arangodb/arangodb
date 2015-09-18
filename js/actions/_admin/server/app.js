/*jshint strict: false */
/*global ArangoServerState */

////////////////////////////////////////////////////////////////////////////////
/// @brief administration actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("org/arangodb/actions");

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_admin_server_role
/// @brief Get to know whether this server is a Coordinator or DB-Server
///
/// @RESTHEADER{GET /_admin/server/role, Return role of a server in a cluster}
///
/// @RESTDESCRIPTION
///
/// Returns the role of a server in a cluster.
/// The role is returned in the *role* attribute of the result.
/// Possible return values for *role* are:
/// - *COORDINATOR*: the server is a coordinator in a cluster
/// - *PRIMARY*: the server is a primary database server in a cluster
/// - *SECONDARY*: the server is a secondary database server in a cluster
/// - *UNDEFINED*: in a cluster, *UNDEFINED* is returned if the server role cannot be
///    determined. On a single server, *UNDEFINED* is the only possible return
///    value.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned in all cases.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/server/role",
  prefix : false,

  callback : function (req, res) {
    actions.resultOk(req, res, actions.HTTP_OK, { role: ArangoServerState.role() });
  }
});

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|/// @startDocuBlock\\|// --SECTION--\\|/// @\\}"
// End:
