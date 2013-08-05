/*jslint indent: 2, nomen: true, maxlen: 120 */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Request Context
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Lucas Dohmen
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var RequestContext,
  extend = require("underscore").extend,
  internal = require("org/arangodb/foxx/internals");

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_initializer
/// @brief Context of a Request Definition
///
/// Used for documenting and constraining the routes.
////////////////////////////////////////////////////////////////////////////////

RequestContext = function (route) {
  'use strict';
  this.route = route;
  this.typeToRegex = {
    "int": "/[0-9]+/",
    "string": "/.+/"
  };
  this.route.docs.nickname = internal.constructNickname(route.docs.httpMethod, route.url.match);
};

extend(RequestContext.prototype, {

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_pathParam
/// @brief Describe a path parameter
///
/// meow
////////////////////////////////////////////////////////////////////////////////

  pathParam: function (paramName, attributes) {
    'use strict';
    var url = this.route.url,
      docs = this.route.docs,
      constraint = url.constraint || {};

    constraint[paramName] = this.typeToRegex[attributes.dataType];
    this.route.url = internal.constructUrlObject(url.match, constraint, url.methods[0]);
    this.route.docs.parameters.push(internal.constructPathParamDoc(
      paramName,
      attributes.description,
      attributes.dataType
    ));
    return this;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_queryParam
/// @brief Describe a Query Parameter
///
/// @FUN{FoxxApplication::queryParam(@FA{id}, @FA{options})}
///
/// Describe a query parameter:
///
/// If you defined a route "/foxx", you can constrain which format a query
/// parameter (`/foxx?a=12`) can have by giving it a type.  We currently support
/// the following types:
///
/// * int
/// * string
///
/// You can also provide a description of this parameter, if it is required and
/// if you can provide the parameter multiple times.
///
/// @EXAMPLES
///
/// @code
///     app.get("/foxx", function {
///       // Do something
///     }).queryParam("id", {
///       description: "Id of the Foxx",
///       dataType: "int",
///       required: true,
///       allowMultiple: false
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

  queryParam: function (paramName, attributes) {
    'use strict';
    this.route.docs.parameters.push(internal.constructQueryParamDoc(
      paramName,
      attributes.description,
      attributes.dataType,
      attributes.required,
      attributes.allowMultiple
    ));
    return this;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_summary
/// @brief Set the summary for this route in the documentation
///
/// @FUN{FoxxApplication::summary(@FA{description})}
///
/// Set the summary for this route in the documentation. Can't be longer than 60.
/// characters
////////////////////////////////////////////////////////////////////////////////

  summary: function (summary) {
    'use strict';
    if (summary.length > 60) {
      throw new Error("Summary can't be longer than 60 characters");
    }
    this.route.docs.summary = summary;
    return this;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_notes
/// @brief Set the notes for this route in the documentation
///
/// @FUN{FoxxApplication::notes(@FA{description})}
///
/// Set the notes for this route in the documentation
////////////////////////////////////////////////////////////////////////////////

  notes: function (notes) {
    'use strict';
    this.route.docs.notes = notes;
    return this;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_errorResponse
/// @brief Document an error response
///
/// @FUN{FoxxApplication::errorResponse(@FA{code}, @FA{description})}
///
/// Document the error response for a given error @FA{code} with a reason for
/// the occurrence.
////////////////////////////////////////////////////////////////////////////////

  errorResponse: function (code, reason) {
    'use strict';
    this.route.docs.errorResponses.push(internal.constructErrorResponseDoc(code, reason));
    return this;
  }
});

exports.RequestContext = RequestContext;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
