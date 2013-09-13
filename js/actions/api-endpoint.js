/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, evil: true */
/*global require, exports, module */

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");

var db = arangodb.db;

var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_get_api_endpoint
/// @brief returns a list of all endpoints
///
/// @RESTHEADER{POST /_api/endpoint,returns a list of all endpoints}
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_post_api_endpoint
/// @brief connects a new endpoint or reconfigures an existing endpoint
///
/// @RESTHEADER{POST /_api/endpoint,connects a new endpoint or reconfigures an existing one}
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_delete_api_endpoint
/// @brief disconnects an existing endpoint
///
/// @RESTHEADER{DELETE /_api/endpoint/<endpoint>,disconnects an existing endpoint}
///
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/endpoint",
  context : "admin",
  prefix : true,

  callback : function (req, res) {
    try {
      var result;

      if (req.requestType === actions.GET) {
        actions.resultOk(req, res, actions.HTTP_OK, internal.listEndpoints());
      }

      else if (req.requestType === actions.POST) {
        var body = actions.getJsonBody(req, res);

        if (body === undefined || typeof body.endpoint !== 'string') {
          actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                            "invalid endpoint value");
          return;
        }

        result = internal.configureEndpoint(body.endpoint, body.databases || [ ]);
        actions.resultOk(req, res, actions.HTTP_OK, { result: result });
      }

      else if (req.requestType === actions.DELETE) {
        if (req.suffix.length !== 1) {
          actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                            "expected DELETE /" + this.url + "/<endpoint>");
          return;
        }

        var endpoint = decodeURIComponent(req.suffix[0]);
        result = internal.removeEndpoint(endpoint);
        actions.resultOk(req, res, actions.HTTP_OK, { result: result });
      }
      else {
        actions.resultUnsupported(req, res);
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}"
// End:
