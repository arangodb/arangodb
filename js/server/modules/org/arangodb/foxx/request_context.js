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
  SwaggerDocs,
  extend = require("underscore").extend,
  internal = require("org/arangodb/foxx/internals"),
  is = require("org/arangodb/is"),
  createBubbleWrap;

createBubbleWrap = function (handler, errorClass, code, reason, errorHandler) {
  'use strict';
  if (is.notExisty(errorHandler)) {
    errorHandler = function () {
      return { error: reason };
    };
  }

  return function (req, res) {
    try {
      handler(req, res);
    } catch (e) {
      if (e instanceof errorClass) {
        res.status(code);
        res.json(errorHandler(e));
      } else {
        throw e;
      }
    }
  };
};

// Wraps the docs object of a route to add swagger compatible documentation
SwaggerDocs = function (docs) {
  'use strict';
  this.docs = docs;
};

extend(SwaggerDocs.prototype, {
  addNickname: function (httpMethod, match) {
    'use strict';
    this.docs.nickname = internal.constructNickname(httpMethod, match);
  },

  addPathParam: function (paramName, description, dataType) {
    'use strict';
    this.docs.parameters.push(internal.constructPathParamDoc(paramName, description, dataType));
  },

  addQueryParam: function (paramName, description, dataType, required, allowMultiple) {
    'use strict';
    this.docs.parameters.push(internal.constructQueryParamDoc(
      paramName,
      description,
      dataType,
      required,
      allowMultiple
    ));
  },

  addSummary: function (summary) {
    'use strict';
    this.docs.summary = summary;
  },

  addNotes: function (notes) {
    'use strict';
    this.docs.notes = notes;
  }
});

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
  this.docs = new SwaggerDocs(this.route.docs);
  this.docs.addNickname(route.docs.httpMethod, route.url.match);
};

extend(RequestContext.prototype, {

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_pathParam
/// @brief Describe a path parameter
///
/// If you defined a route "/foxx/:id", you can constrain which format a path
/// parameter (`/foxx/12`) can have by giving it a type.  We currently support
/// the following types:
///
/// * int
/// * string
///
/// You can also provide a description of this parameter.
///
/// @EXAMPLES
///
/// @code
///     app.get("/foxx/:id", function {
///       // Do something
///     }).pathParam("id", {
///       description: "Id of the Foxx",
///       type: "int"
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

  pathParam: function (paramName, attributes) {
    'use strict';
    var url = this.route.url,
      docs = this.route.docs,
      constraint = url.constraint || {};

    constraint[paramName] = this.typeToRegex[attributes.type];
    this.route.url = internal.constructUrlObject(url.match, constraint, url.methods[0]);
    this.docs.addPathParam(paramName, attributes.description, attributes.type);
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
///       type: "int",
///       required: true,
///       allowMultiple: false
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

  queryParam: function (paramName, attributes) {
    'use strict';
    this.docs.addQueryParam(
      paramName,
      attributes.description,
      attributes.type,
      attributes.required,
      attributes.allowMultiple
    );
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
    this.docs.addSummary(summary);
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
    this.docs.addNotes(notes);
    return this;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_errorResponse
/// @brief Define an error response
///
/// @FUN{FoxxApplication::errorResponse(@FA{errorClass}, @FA{code}, @FA{description})}
///
/// Define a reaction to a thrown error for this route: If your handler throws an error
/// of the defined errorClass, it will be caught and the response will have the given
/// status code and a JSON with error set to your description as the body.
///
/// If you want more control over the returned JSON, you can give an optional fourth
/// parameter in form of a function. It gets the error as an argument, the return
/// value will transformed into JSON and then be used as the body.
/// The status code will be used as described above. The description will be used for
/// the documentation.
///
/// It also adds documentation for this error response to the generated documentation.
///
/// @EXAMPLES
///
/// @code
///     app.get("/foxx", function {
///       throw new FoxxyError();
///     }).errorResponse(FoxxyError, 303, "This went completely wrong. Sorry!");
///
///     app.get("/foxx", function {
///       throw new FoxxyError();
///     }).errorResponse(FoxxyError, 303, "This went completely wrong. Sorry!", function (e) {
///       return {
///         code: 123,
///         desc: e.description
///       };
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

  errorResponse: function (errorClass, code, reason, errorHandler) {
    'use strict';
    var handler = this.route.action.callback;
    this.route.action.callback = createBubbleWrap(handler, errorClass, code, reason, errorHandler);
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
