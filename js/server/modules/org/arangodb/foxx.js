/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx application
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

var FoxxApplication,
  RequestContext,
  BaseMiddleware,
  FormatMiddleware,
  _ = require("underscore"),
  db = require("org/arangodb").db,
  fs = require("fs"),
  console = require("console"),
  INTERNAL = require("internal"),
  foxxManager = require("org/arangodb/foxx-manager"),
  internal = {};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_createUrlObject
/// @brief create a new url object
///
/// ArangoDB uses a certain structure we refer to as `UrlObject`.
/// With the following function (which is only internal, and not
/// exported) you can create an UrlObject with a given URL,
/// a constraint and a method. For example:
///
/// @EXAMPLES
///     internal.createUrlObject('/lecker/gans', null, 'get')
////////////////////////////////////////////////////////////////////////////////

internal.createUrlObject = function (url, constraint, method) {
  'use strict';
  var urlObject = {};

  if (!_.isString(url)) {
    throw "URL has to be a String";
  }

  urlObject.match = url;
  urlObject.methods = [method];

  if (!_.isUndefined(constraint)) {
    urlObject.constraint = constraint;
  }

  return urlObject;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_initializer
/// @brief Create a new Application
///
/// And that's FoxxApplication. It's a constructor, so call it like this:
/// It takes two optional arguments as displayed above:
/// * **The URL Prefix:** All routes you define within will be prefixed with it
/// * **The Template Collection:** More information in the template section
///
/// @EXAMPLES
///     app = new FoxxApplication({
///       urlPrefix: "/wiese",
///       templateCollection: "my_templates"
///     });
////////////////////////////////////////////////////////////////////////////////

FoxxApplication = function (options) {
  'use strict';
  var urlPrefix, templateCollection, myMiddleware;

  options = options || {};

  urlPrefix = options.urlPrefix || "";
  templateCollection = options.templateCollection;

  this.routingInfo = {
    routes: []
  };

  this.requiresLibs = {};
  this.requiresModels = {};
  this.helperCollection = {};

  this.routingInfo.urlPrefix = urlPrefix;

  if (_.isString(templateCollection)) {
    this.routingInfo.templateCollection = db._collection(templateCollection) ||
      db._create(templateCollection);
    myMiddleware = new BaseMiddleware(templateCollection, this.helperCollection);
  } else {
    myMiddleware = new BaseMiddleware();
  }

  this.routingInfo.middleware = [
    {
      url: {match: "/*"},
      action: {callback: myMiddleware.stringRepresentation}
    }
  ];
};

_.extend(FoxxApplication.prototype, {
////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_start
/// @brief Start the application
///
/// Sometimes it is a good idea to actually start the application
/// you wrote. If this precious moment has arrived, you should
/// use this function.
/// You have to provide the start function with the `applicationContext`
/// variable.
////////////////////////////////////////////////////////////////////////////////

  start: function (context) {
    'use strict';
    var models = this.requiresModels,
      requires = this.requiresLibs,
      prefix = context.prefix;

    this.routingInfo.urlPrefix = prefix + "/" + this.routingInfo.urlPrefix;

    _.each(this.routingInfo.routes, function (route) {
      route.action.context = context.context;
      route.action.requiresLibs = requires;
      route.action.requiresModels = models;
    });

    context.routingInfo = this.routingInfo;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_handleRequest
/// @brief Handle a request
///
/// The `handleRequest` method is the raw way to create a new
/// route. You probably wont call it directly, but it is used
/// in the other request methods:
///
/// When defining a route you can also define a so called 'parameterized'
/// route like `/gaense/:stable`. In this case you can later get the value
/// the user provided for `stable` via the `params` function (see the Request
/// object).
///
/// @EXAMPLES
///     app.handleRequest("get", "/gaense", function (req, res) {
///       //handle the request
///     });
////////////////////////////////////////////////////////////////////////////////
  handleRequest: function (method, route, callback) {
    'use strict';
    var newRoute = {
      url: internal.createUrlObject(route, undefined, method),
      action: {
        callback: String(callback)
      },
      docs: {
        parameters: [],
        errorResponses: [],
        httpMethod: method.toUpperCase()
      }
    };

    this.routingInfo.routes.push(newRoute);
    return new RequestContext(newRoute);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_head
/// @brief Handle a `head` request
///
/// This handles requests from the HTTP verb `head`.
/// As with all other requests you can give an option as the third
/// argument, or leave it blank. You have to give a function as
/// the last argument however. It will get a request and response
/// object as its arguments
////////////////////////////////////////////////////////////////////////////////
  head: function (route, callback) {
    'use strict';
    return this.handleRequest("head", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_get
/// @brief Manage a `get` request
///
/// This handles requests from the HTTP verb `get`.
/// See above for the arguments you can give. An example:
///
/// @EXAMPLES
///     app.get('/gaense/stall', function (req, res) {
///       // Take this request and deal with it!
///     });
////////////////////////////////////////////////////////////////////////////////
  get: function (route, callback) {
    'use strict';
    return this.handleRequest("get", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_post
/// @brief Tackle a `post` request
///
/// This handles requests from the HTTP verb `post`.
/// See above for the arguments you can give. An example:
///
/// @EXAMPLES
///     app.post('/gaense/stall', function (req, res) {
///       // Take this request and deal with it!
///     });
////////////////////////////////////////////////////////////////////////////////
  post: function (route, callback) {
    'use strict';
    return this.handleRequest("post", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_put
/// @brief Sort out a `put` request
///
/// This handles requests from the HTTP verb `put`.
/// See above for the arguments you can give. An example:
///
/// @EXAMPLES
///     app.put('/gaense/stall', function (req, res) {
///       // Take this request and deal with it!
///     });
////////////////////////////////////////////////////////////////////////////////
  put: function (route, callback) {
    'use strict';
    return this.handleRequest("put", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_patch
/// @brief Take charge of a `patch` request
///
/// This handles requests from the HTTP verb `patch`.
/// See above for the arguments you can give. An example:
///
/// @EXAMPLES
///     app.patch('/gaense/stall', function (req, res) {
///       // Take this request and deal with it!
///     });
////////////////////////////////////////////////////////////////////////////////
  patch: function (route, callback) {
    'use strict';
    return this.handleRequest("patch", route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_delete
/// @brief Respond to a `delete` request
///
/// This handles requests from the HTTP verb `delete`.
/// See above for the arguments you can give.
/// **A word of warning:** Do not forget that `delete` is
/// a reserved word in JavaScript and therefore needs to be
/// called as `app['delete']`. There is also an alias `del`
/// for this very reason.
///
/// @EXAMPLES
///     app['delete']('/gaense/stall', function (req, res) {
///       // Take this request and deal with it!
///     });
///
///     app.del('/gaense/stall', function (req, res) {
///       // Take this request and deal with it!
///     });
////////////////////////////////////////////////////////////////////////////////
  'delete': function (route, callback) {
    'use strict';
    return this.handleRequest("delete", route, callback);
  },

  del: function (route, callback) {
    'use strict';
    return this['delete'](route, callback);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_before
/// @brief Before
///
/// The before function takes a path on which it should watch
/// and a function that it should execute before the routing
/// takes place. If you do omit the path, the function will
/// be executed before each request, no matter the path.
/// Your function gets a Request and a Response object.
/// An example:
///
/// @EXAMPLES
///     app.before('/high/way', function(req, res) {
///       //Do some crazy request logging
///     });
////////////////////////////////////////////////////////////////////////////////
  before: function (path, func) {
    'use strict';
    if (_.isUndefined(func)) {
      func = path;
      path = "/*";
    }

    this.routingInfo.middleware.push({
      url: {match: path},
      action: {
        callback: "function (req, res, opts, next) { " + String(func) + "(req, res); next(); })"
      }
    });
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_after
/// @brief After
///
/// This works pretty similar to the before function.
/// But it acts after the execution of the handlers
/// (Big surprise, I suppose).
///
/// An example:
///
/// @EXAMPLES
///     app.after('/high/way', function(req, res) {
///       //Do some crazy response logging
///     });
////////////////////////////////////////////////////////////////////////////////
  after: function (path, func) {
    'use strict';
    if (_.isUndefined(func)) {
      func = path;
      path = "/*";
    }

    this.routingInfo.middleware.push({
      url: {match: path},
      action: {
        callback: "function (req, res, opts, next) { next(); " + String(func) + "(req, res); })"
      }
    });
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_helper
/// @brief The ViewHelper concept
///
/// If you want to use a function inside your templates, the ViewHelpers
/// will come to rescue you. Define them on your app like this:
///
/// @EXAMPLES
///     app.helper("link_to", function (identifier) {
///       return urlRepository[identifier];
///     });
///
/// Then you can just call this function in your template's JavaScript
/// blocks.
////////////////////////////////////////////////////////////////////////////////
  helper: function (name, func) {
    'use strict';
    this.helperCollection[name] = func;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_application_accepts
/// @brief Shortform for using the FormatMiddleware
///
/// More information about the FormatMiddleware in the corresponding section.
/// This is a shortcut to add the middleware to your application:
///
/// @EXAMPLES
///     app.accepts(["json"], "json");
////////////////////////////////////////////////////////////////////////////////
  accepts: function (allowedFormats, defaultFormat) {
    'use strict';

    this.routingInfo.middleware.push({
      url:    { match: "/*" },
      action: {
        callback: (new FormatMiddleware(allowedFormats, defaultFormat)).stringRepresentation
      }
    });
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
};

_.extend(RequestContext.prototype, {
////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_pathParam
/// @brief Describe a Path Parameter
///
/// If you defined a route "/foxx/:id", you can constrain which format the id
/// can have by giving a type. We currently support the following types:
///
/// * int
/// * string
///
/// You can also provide a description of this parameter.
///
/// @EXAMPLE:
///     app.get("/foxx/:id", function {
///       // Do something
///     }).pathParam("id", {
///       description: "Id of the Foxx",
///       dataType: "int"
///     });
////////////////////////////////////////////////////////////////////////////////
  pathParam: function (paramName, attributes) {
    'use strict';
    var url = this.route.url,
      docs = this.route.docs,
      constraint = url.constraint || {};

    constraint[paramName] = this.typeToRegex[attributes.dataType];
    this.route.url = internal.createUrlObject(url.match, constraint, url.methods[0]);
    this.route.docs.parameters.push({
      paramType: "path",
      name: paramName,
      description: attributes.description,
      dataType: attributes.dataType,
      required: true
    });

    return this;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_queryParam
/// @brief Describe a Query Parameter
///
/// If you defined a route "/foxx", you can constrain which format a query
/// parameter (`/foxx?a=12`) can have by giving it a type.
/// We currently support the following types:
///
/// * int
/// * string
///
/// You can also provide a description of this parameter, if it is required and
///  if you can provide the parameter multiple times.
///
/// @EXAMPLE:
///     app.get("/foxx", function {
///       // Do something
///     }).queryParam("id", {
///       description: "Id of the Foxx",
///       dataType: "int",
///       required: true,
///       allowMultiple: false
///     });
////////////////////////////////////////////////////////////////////////////////

  queryParam: function (paramName, attributes) {
    'use strict';
    this.route.docs.parameters.push({
      paramType: "query",
      name: paramName,
      description: attributes.description,
      dataType: attributes.dataType,
      required: attributes.required,
      allowMultiple: attributes.allowMultiple
    });

    return this;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_nickname
/// @brief Set the nickname for this route in the documentation
////////////////////////////////////////////////////////////////////////////////

  nickname: function (nickname) {
    'use strict';
    this.route.docs.nickname = nickname;
    return this;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_summary
/// @brief Set the summary for this route in the documentation
///
/// Can't be longer than 60 characters
////////////////////////////////////////////////////////////////////////////////

  summary: function (summary) {
    'use strict';
    if (summary.length > 60) {
      throw "Summary can't be longer than 60 characters";
    }
    this.route.docs.summary = summary;
    return this;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_RequestContext_notes
/// @brief Set the notes for this route in the documentation
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
/// Document the error response for a given error code with a reason for the
/// occurrence.
////////////////////////////////////////////////////////////////////////////////

  errorResponse: function (code, reason) {
    'use strict';
    this.route.docs.errorResponses.push({
      code: code,
      reason: reason
    });
    return this;
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_initializer
/// @brief The Base Middleware
///
/// The `BaseMiddleware` manipulates the request and response
/// objects to give you a nicer API.
////////////////////////////////////////////////////////////////////////////////
BaseMiddleware = function (templateCollection, helperCollection) {
  'use strict';
  var middleware = function (request, response, options, next) {
    var responseFunctions,
      requestFunctions,
      _ = require("underscore");

    requestFunctions = {
////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_request_body
/// @brief The superfluous `body` function
///
/// Get the body of the request
////////////////////////////////////////////////////////////////////////////////
      body: function () {
        return this.requestBody;
      },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_request_params
/// @brief The jinxed `params` function
///
/// Get the parameters of the request. This process is two-fold:
///
/// 1. If you have defined an URL like `/test/:id` and the user
/// requested `/test/1`, the call `params("id")` will return `1`.
/// 2. If you have defined an URL like `/test` and the user gives a
/// query component, the query parameters will also be returned.
/// So for example if the user requested `/test?a=2`, the call
/// `params("a")` will return `2`.
////////////////////////////////////////////////////////////////////////////////
      params: function (key) {
        var ps = {};
        _.extend(ps, this.urlParameters);
        _.extend(ps, this.parameters);
        return ps[key];
      }
    };

    responseFunctions = {
////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_response_status
/// @brief The straightforward `status` function
///
/// Set the status code of your response, for example:
///
/// @EXAMPLES
///     response.status(404);
////////////////////////////////////////////////////////////////////////////////
      status: function (code) {
        this.responseCode = code;
      },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_response_set
/// @brief The radical `set` function
///
/// Set a header attribute, for example:
///
/// @EXAMPLES
///     response.set("Content-Length", 123);
///     response.set("Content-Type", "text/plain");
///
/// or alternatively:
///
/// @EXAMPLES
///     response.set({
///       "Content-Length": "123",
///       "Content-Type": "text/plain"
///     });
////////////////////////////////////////////////////////////////////////////////
      set: function (key, value) {
        var attributes = {};
        if (_.isUndefined(value)) {
          attributes = key;
        } else {
          attributes[key] = value;
        }

        _.each(attributes, function (value, key) {
          key = key.toLowerCase();
          this.headers = this.headers || {};
          this.headers[key] = value;

          if (key === "content-type") {
            this.contentType = value;
          }
        }, this);
      },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_response_json
/// @brief The magical `json` function
///
/// Set the content type to JSON and the body to the
/// JSON encoded object you provided.
///
/// @EXAMPLES
///     response.json({'born': 'December 12, 1915'});
////////////////////////////////////////////////////////////////////////////////
      json: function (obj) {
        this.contentType = "application/json";
        this.body = JSON.stringify(obj);
      },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_response_render
/// @brief The mysterious `render` function
///
/// If you initialize your FoxxApplication with a `templateCollection`,
/// you're in luck now.
/// It expects documents in the following form in this collection:
///
/// @EXAMPLES
///     {
///       path: "high/way",
///       content: "hallo <%= username %>",
///       contentType: "text/plain",
///       templateLanguage: "underscore"
///     }
///
/// The `content` is the string that will be rendered by the template
/// processor. The `contentType` is the type of content that results
/// from this call. And with the `templateLanguage` you can choose
/// your template processor. There is only one choice now: `underscore`.
///
/// If you call render, FoxxApplication will look
/// into the this collection and search by the path attribute.
/// It will then render the template with the given data:
///
/// @EXAMPLES
///     response.render("high/way", {username: 'FoxxApplication'})
///
/// Which would set the body of the response to `hello FoxxApplication` with the
/// template defined above. It will also set the `contentType` to
/// `text/plain` in this case.
///
/// In addition to the attributes you provided, you also have access to
/// all your view helpers. How to define them? Read above in the
/// ViewHelper section.
////////////////////////////////////////////////////////////////////////////////
      render: function (templatePath, data) {
        var template;

        if (_.isUndefined(templateCollection)) {
          throw "No template collection has been provided when creating a new FoxxApplication";
        }

        template = templateCollection.firstExample({path: templatePath });

        if (_.isNull(template)) {
          throw "Template '" + templatePath + "' does not exist";
        }

        if (template.templateLanguage !== "underscore") {
          throw "Unknown template language '" + template.templateLanguage + "'";
        }

        this.body = _.template(template.content, _.extend(data, helperCollection));
        this.contentType = template.contentType;
      }
    };

    request = _.extend(request, requestFunctions);
    response = _.extend(response, responseFunctions);
    next();
  };

  return {
    stringRepresentation: String(middleware),
    functionRepresentation: middleware
  };
};

FormatMiddleware = function (allowedFormats, defaultFormat) {
  'use strict';
  var stringRepresentation, middleware = function (request, response, options, next) {
    var parsed, determinePathAndFormat;

    determinePathAndFormat = function (path, headers) {
      var urlFormatToMime, mimeToUrlFormat, parsed;

      parsed = {
        contentType: headers.accept
      };

      urlFormatToMime = {
        json: "application/json",
        html: "text/html",
        txt: "text/plain"
      };

      mimeToUrlFormat = {
        "application/json": "json",
        "text/html": "html",
        "text/plain": "txt"
      };
      path = path.split('.');

      if (path.length === 1) {
        parsed.path = path.join();
      } else {
        parsed.format = path.pop();
        parsed.path = path.join('.');
      }

      if (parsed.contentType === undefined && parsed.format === undefined) {
        parsed.format = defaultFormat;
        parsed.contentType = urlFormatToMime[defaultFormat];
      } else if (parsed.contentType === undefined) {
        parsed.contentType = urlFormatToMime[parsed.format];
      } else if (parsed.format === undefined) {
        parsed.format = mimeToUrlFormat[parsed.contentType];
      }

      if (parsed.format !== mimeToUrlFormat[parsed.contentType]) {
        throw "Contradiction between Accept Header and URL.";
      }

      if (allowedFormats.indexOf(parsed.format) < 0) {
        throw "Format '" + parsed.format + "' is not allowed.";
      }

      return parsed;
    };

    try {
      parsed = determinePathAndFormat(request.path, request.headers);
      request.path = parsed.path;
      request.format = parsed.format;
      response.contentType = parsed.contentType;
      next();
    } catch (e) {
      response.responseCode = 406;
      response.body = e;
    }
  };

  stringRepresentation = String(middleware)
    .replace("allowedFormats", JSON.stringify(allowedFormats))
    .replace("defaultFormat", JSON.stringify(defaultFormat));

  return {
    functionRepresentation: middleware,
    stringRepresentation: stringRepresentation
  };
};

/// We finish off with exporting FoxxApplication and the middlewares.
/// Everything else will remain our secret.

exports.installApp = foxxManager.installApp;
exports.installDevApp = foxxManager.installDevApp;
exports.uninstallApp = foxxManager.uninstallApp;
exports.scappAppDirectory = foxxManager.scanAppDirectory;
exports.FoxxApplication = FoxxApplication;
exports.BaseMiddleware = BaseMiddleware;
exports.FormatMiddleware = FormatMiddleware;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

/// -----------------------------------------------------------------------------
/// --SECTION--                                                       END-OF-FILE
/// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
