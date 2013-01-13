var Frank,
  _ = require("underscore"),
  internal = {};

internal.createUrlObject = function (url, constraint) {
  var urlObject = {};

  if(!_.isString(url)) {
    throw "URL has to be a String";
  }

  urlObject.match = url;

  if (!_.isNull(constraint)) {
    urlObject.constraint = constraint;
  }

  return urlObject;
};

Frank = function (options) {
  options = options || {};

  this.routingInfo = {
    routes: []
  };

  if (_.isString(options.urlPrefix)) {
    this.routingInfo.urlPrefix = options.urlPrefix;
  }
};

_.extend(Frank.prototype, {
  get: function (route, argument1, argument2) {
    var newRoute = {}, options, handler;

    if (arguments.length === 2) {
      handler = argument1;
      options = {};
    } else if (arguments.length === 3) {
      options = argument1;
      handler = argument2;
    } else {
      raise("ArgumentError: Give two or three arguments");
    }

    newRoute.url = internal.createUrlObject(route, options.constraint);
    newRoute.handler = handler;

    this.routingInfo.routes.push(newRoute);
  },

  /*
  setPublic: function () {
    // Can we put 404 etc. and assets here?
    var staticPagesHandler = {
      type: "StaticPages",
      key: "/static",
      url: {
        match: "/static/ *"
      },
      action: {
        controller: "org/arangodb/actions/staticContentController",
        methods: [
          "GET",
          "HEAD"
        ],
        options: {
          application: "org.example.simple",
          contentCollection: "org_example_simple_content",
          prefix: "/static"
        }
      }
    };
  }
  */
});

exports.Frank = Frank;
