/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true */
/*global module, require, exports */

// Foxx is an easy way to create APIs and simple web applications
// from within **ArangoDB**.
// It is inspired by Sinatra, the classy Ruby web framework. If FoxxApplication is Sinatra,
// [ArangoDB Actions](http://www.arangodb.org/manuals/current/UserManualActions.html)
// are the corresponding `Rack`. They provide all the HTTP goodness.
//
// So let's get started, shall we?

// FoxxApplication uses Underscore internally. This library is wonderful.
var FoxxApplication,
  baseMiddleware,
  formatMiddleware,
  _ = require("underscore"),
  db = require("org/arangodb").db,
  fs = require("fs"),
  console = require("console"),
  internal = {};

// ArangoDB uses a certain structure we refer to as `UrlObject`.
// With the following function (which is only internal, and not
// exported) you can create an UrlObject with a given URL,
// a constraint and a method. For example:
//
//     internal.createUrlObject('/lecker/gans', null, 'get')
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

// ## Creating a new Application
// And that's FoxxApplication. It's a constructor, so call it like this:
//
//     app = new FoxxApplication({
//       urlPrefix: "/wiese",
//       templateCollection: "my_templates"
//     })
//
// It takes two optional arguments as displayed above:
//
// * **The URL Prefix:** All routes you define within will be prefixed with it
// * **The Template Collection:** More information in the template section
FoxxApplication = function (options) {
  'use strict';
  var urlPrefix, templateCollection, myMiddleware;

  options = options || {};

  urlPrefix = options.urlPrefix;
  templateCollection = options.templateCollection;

  this.routingInfo = {
    routes: []
  };

  this.requires = {};
  this.models = {};
  this.helperCollection = {};

  if (_.isString(urlPrefix)) {
    this.routingInfo.urlPrefix = urlPrefix;
  }

  if (_.isString(templateCollection)) {
    this.routingInfo.templateCollection = db._collection(templateCollection) ||
      db._create(templateCollection);
    myMiddleware = baseMiddleware(templateCollection, this.helperCollection);
  } else {
    myMiddleware = baseMiddleware();
  }

  this.routingInfo.middleware = [
    {
      url: {match: "/*"},
      action: {callback: myMiddleware}
    }
  ];
};

// ## The functions on a created FoxxApplication
//
// When you have created your FoxxApplication you can now define routes
// on it. You provide each with a function that will handle
// the request. It gets two arguments (four, to be honest. But the
// other two are not relevant for now):
//
// * The request object
// * The response object
//
// You can find more about those in their individual sections.
_.extend(FoxxApplication.prototype, {
  // Sometimes it is a good idea to actually start the application
  // you wrote. If this precious moment has arrived, you should
  // use this function.
  // You have to provide the start function with the `applicationContext`
  // variable.
  start: function (context) {
    'use strict';
    var models = this.models,
      requires = this.requires;

    _.each(this.routingInfo.routes, function (route) {
      route.action.context = context;
      route.action.requires = requires;
      route.action.models = models;
    });

    db._collection("_routing").save(this.routingInfo);
  },

  // The `handleRequest` method is the raw way to create a new
  // route. You probably wont call it directly, but it is used
  // in the other request methods:
  //
  //     app.handleRequest("get", "/gaense", function (req, res) {
  //       //handle the request
  //     });
  //
  // When defining a route you can also define a so called 'parameterized'
  // route like `/gaense/:stable`. In this case you can later get the value
  // the user provided for `stable` via the `params` function (see the Request
  // object).
  handleRequest: function (method, route, argument1, argument2) {
    'use strict';
    var newRoute = {}, options, callback;

    if (_.isUndefined(argument2)) {
      callback = argument1;
      options = {};
    } else {
      options = argument1;
      callback = argument2;
    }

    newRoute.url = internal.createUrlObject(route, options.constraint, method);
    newRoute.action = {
      callback: String(callback)
    };

    this.routingInfo.routes.push(newRoute);
  },

  // ### Handle a `head` request
  // This handles requests from the HTTP verb `head`.
  // As with all other requests you can give an option as the third
  // argument, or leave it blank. You have to give a function as
  // the last argument however. It will get a request and response
  // object as its arguments
  head: function (route, argument1, argument2) {
    'use strict';
    this.handleRequest("head", route, argument1, argument2);
  },

  // ### Manage a `get` request
  // This handles requests from the HTTP verb `get`.
  // See above for the arguments you can give. An example:
  //
  //     app.get('/gaense/stall', function (req, res) {
  //       // Take this request and deal with it!
  //     });
  get: function (route, argument1, argument2) {
    'use strict';
    this.handleRequest("get", route, argument1, argument2);
  },

  // ### Tackle a `post` request
  // This handles requests from the HTTP verb `post`.
  // See above for the arguments you can give. An example:
  //
  //     app.post('/gaense/stall', function (req, res) {
  //       // Take this request and deal with it!
  //     });
  post: function (route, argument1, argument2) {
    'use strict';
    this.handleRequest("post", route, argument1, argument2);
  },

  // ### Sort out a `put` request
  // This handles requests from the HTTP verb `put`.
  // See above for the arguments you can give. An example:
  //
  //     app.put('/gaense/stall', function (req, res) {
  //       // Take this request and deal with it!
  //     });
  put: function (route, argument1, argument2) {
    'use strict';
    this.handleRequest("put", route, argument1, argument2);
  },

  // ### Take charge of a `patch` request
  // This handles requests from the HTTP verb `patch`.
  // See above for the arguments you can give. An example:
  //
  //     app.patch('/gaense/stall', function (req, res) {
  //       // Take this request and deal with it!
  //     });
  patch: function (route, argument1, argument2) {
    'use strict';
    this.handleRequest("patch", route, argument1, argument2);
  },

  // ### Respond to a `delete` request
  // This handles requests from the HTTP verb `delete`.
  // See above for the arguments you can give.
  // **A word of warning:** Do not forget that `delete` is
  // a reserved word in JavaScript so call it as follows:
  //
  //     app['delete']('/gaense/stall', function (req, res) {
  //       // Take this request and deal with it!
  //     });
  'delete': function (route, argument1, argument2) {
    'use strict';
    this.handleRequest("delete", route, argument1, argument2);
  },

  // ## Before and After Hooks
  // You can use the following two functions to do something
  // before or respectively after the normal routing process
  // is happening. You could use that for logging or to manipulate
  // the request or response (translate it to a certain format for
  // example).

  // ### Before
  // The before function takes a path on which it should watch
  // and a function that it should execute before the routing
  // takes place. If you do omit the path, the function will
  // be executed before each request, no matter the path.
  // Your function gets a Request and a Response object.
  // An example:
  //
  //     app.before('/high/way', function(req, res) {
  //       //Do some crazy request logging
  //     });
  before: function (path, func) {
    'use strict';
    if (_.isUndefined(func)) {
      func = path;
      path = "/*";
    }

    this.routingInfo.middleware.push({
      url: {match: path},
      action: {callback: String(function (req, res, opts, next) {
        func(req, res);
        next();
      })}
    });
  },

  // ### After
  // This works pretty similar to the before function.
  // But it acts after the execution of the handlers
  // (Big surprise, I suppose).
  //
  // An example:
  //
  //     app.after('/high/way', function(req, res) {
  //       //Do some crazy response logging
  //     });
  after: function (path, func) {
    'use strict';
    if (_.isUndefined(func)) {
      func = path;
      path = "/*";
    }

    this.routingInfo.middleware.push({
      url: {match: path},
      action: {callback: String(function (req, res, opts, next) {
        next();
        func(req, res);
      })}
    });
  },

  // ## The ViewHelper concept
  // If you want to use a function inside your templates, the ViewHelpers
  // will come to rescue you. Define them on your app like this:
  //
  //     app.helper("link_to", function (identifier) {
  //       return urlRepository[identifier];
  //     });
  //
  // Then you can just call this function in your template's JavaScript
  // blocks.
  helper: function (name, func) {
    'use strict';
    this.helperCollection[name] = func;
  },

  // ## Shortform for using the formatMiddleware
  //
  // More information about the formatMiddleware in the corresponding section.
  // This is a shortcut to add the middleware to your application:
  //
  //     app.accepts(["json"], "json");
  accepts: function (allowedFormats, defaultFormat) {
    'use strict';

    this.routingInfo.middleware.push({
      url:    { match: "/*" },
      action: { callback: formatMiddleware(allowedFormats, defaultFormat) }
    });
  }
});


// ## The Base Middleware
// The `baseMiddleware` manipulates the request and response
// objects to give you a nicer API.
baseMiddleware = function (templateCollection, helperCollection) {
  'use strict';
  var middleware = function (request, response, options, next) {
    var responseFunctions,
      requestFunctions,
      _ = require("underscore");

    // ### The Request Object
    // Every request object has the following attributes from the underlying Actions,
    // amongst others:
    //
    // * path: The complete path as supplied by the user
    //
    // FoxxApplication adds the following methods to this request object:
    requestFunctions = {
      // ### The superfluous `body` function
      // Get the body of the request
      body: function () {
        return this.requestBody;
      },

      // ### The jinxed `params` function
      // Get the parameters of the request. This process is two-fold:
      //
      // 1. If you have defined an URL like `/test/:id` and the user
      // requested `/test/1`, the call `params("id")` will return `1`.
      // 2. If you have defined an URL like `/test` and the user gives a
      // query component, the query parameters will also be returned.
      // So for example if the user requested `/test?a=2`, the call
      // `params("a")` will return `2`.
      params: function (key) {
        var ps = {};
        _.extend(ps, this.urlParameters);
        _.extend(ps, this.parameters);
        return ps[key];
      }
    };

    // ### The Response Object
    // Every response object has the following attributes from the underlying Actions:
    //
    // * contentType
    // * responseCode
    // * body
    // * headers (an object)
    //
    // FoxxApplication adds the following methods to this response object.
    responseFunctions = {
      // ### The straightforward `status` function
      // Set the status code of your response, for example:
      //
      //     response.status(404);
      status: function (code) {
        this.responseCode = code;
      },

      // ### The radical `set` function
      // Set a header attribute, for example:
      //
      //     response.set("Content-Length", 123);
      //     response.set("Content-Type", "text/plain");
      //
      // or alternatively:
      //
      //     response.set({
      //       "Content-Length": "123",
      //       "Content-Type": "text/plain"
      //     });
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

      // ### The magical `json` function
      // Set the content type to JSON and the body to the
      // JSON encoded object you provided.
      //
      //     response.json({'born': 'December 12, 1915'});
      json: function (obj) {
        this.contentType = "application/json";
        this.body = JSON.stringify(obj);
      },

      // ### The mysterious `render` function
      // If you initialize your FoxxApplication with a `templateCollection`,
      // you're in luck now.
      // It expects documents in the following form in this collection:
      //
      //     {
      //       path: "high/way",
      //       content: "hallo <%= username %>",
      //       contentType: "text/plain",
      //       templateLanguage: "underscore"
      //     }
      //
      // The `content` is the string that will be rendered by the template
      // processor. The `contentType` is the type of content that results
      // from this call. And with the `templateLanguage` you can choose
      // your template processor. There is only one choice now: `underscore`.
      //
      // If you call render, FoxxApplication will look
      // into the this collection and search by the path attribute.
      // It will then render the template with the given data:
      //
      //     response.render("high/way", {username: 'FoxxApplication'})
      //
      // Which would set the body of the response to `hello FoxxApplication` with the
      // template defined above. It will also set the `contentType` to
      // `text/plain` in this case.
      //
      // In addition to the attributes you provided, you also have access to
      // all your view helpers. How to define them? Read above in the
      // ViewHelper section.
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

    // Now enhance the request and response as described above and call next at the
    // end of this middleware (otherwise your application would never
    // be executed. Would be a shame, really).
    request = _.extend(request, requestFunctions);
    response = _.extend(response, responseFunctions);
    next();
  };

  return String(middleware);
};

// ## The Format Middleware
// Unlike the `baseMiddleware` this Middleware is only loaded if you
// want it. This Middleware gives you Rails-like format handling via
// the `extension` of the URL or the accept header.
// Say you request an URL like `/people.json`:
// The `formatMiddleware` will set the format of the request to JSON
// and then delete the `.json` from the request. You can therefore write
// handlers that do not take an `extension` into consideration and instead
// handle the format via a simple String.
// To determine the format of the request it checks the URL and then
// the `accept` header. If one of them gives a format or both give
// the same, the format is set. If the formats are not the same,
// an error is raised.
//
// Use it by calling:
//
//     formatMiddleware = require('foxx').formatMiddleware;
//     app.before("/*", formatMiddleware.new(['json']));
//
// or the shortcut:
//
//     app.accepts(['json']);
//
// In both forms you can give a default format as a second parameter,
// if no format could be determined. If you give no `defaultFormat` this
// case will be handled as an error.
formatMiddleware = function (allowedFormats, defaultFormat) {
  'use strict';
  var middleware, urlFormatToMime, mimeToUrlFormat, determinePathAndFormat;

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

  determinePathAndFormat = function (path, headers) {
    var parsed = {
      contentType: headers.accept
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

  middleware = function (request, response, options, next) {
    var parsed;

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

  return middleware;
};

// We finish off with exporting FoxxApplication and the middlewares.
// Everything else will remain our secret.

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a manifest file
////////////////////////////////////////////////////////////////////////////////

exports.installApp = function (name, mount, options) {
  'use strict';
  var version = options && options.version; // TODO currently ignored
  var prefix = options && options.collectionPrefix;
  var context = {};
  var apps;
  var description;
  var i;
  var root;

  root = module.appRootModule(name); // TODO use version

  if (root === null) {
    if (version === undefined) {
      throw "cannot find application '" + name + "'";
    }
    else {
      throw "cannot find application '" + name + "' in version '" + version + "'";
    }
  }

  description = root._appDescription;

  if (mount === "") {
    mount = "/";
  }
  else if (mount[0] !== "/") {
    mount = "/" + mount;
  }

  if (prefix === undefined) {
    context.collectionPrefix = mount.replace(/\\/g, "_");
  }
  else {
    context.collectionPrefix = prefix;
  }

  context.name = description.manifest.name;
  context.version = description.manifest.version;
  context.mount = mount;

  apps = root._appDescription.manifest.apps;

  for (i in apps) {
    if (apps.hasOwnProperty(i)) {
      var file = apps[i];

      context.appMount = i;
      context.prefix = mount + "/" + i;

      root.loadAppScript(root, file, context);
    }
  }
};

exports.FoxxApplication = FoxxApplication;
exports.baseMiddleware = baseMiddleware;
//TODO: Make a String exports.formatMiddleware = formatMiddleware;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
