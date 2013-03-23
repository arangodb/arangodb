/*jslint indent: 2, nomen: true, maxlen: 100 */
/*global require, exports */

// Fuxx is an easy way to create APIs and simple web applications
// from within **ArangoDB**.
// It is inspired by Sinatra, the classy Ruby web framework. If FuxxApplication is Sinatra,
// [ArangoDB Actions](http://www.arangodb.org/manuals/current/UserManualActions.html)
// are the corresponding `Rack`. They provide all the HTTP goodness.
//
// So let's get started, shall we?

// FuxxApplication uses Underscore internally. This library is wonderful.
var FuxxApplication,
  BaseMiddleware,
  FormatMiddleware,
  _ = require("underscore"),
  db = require("org/arangodb").db,
  fs = require("fs"),
  console = require("console"),
  internal = {};

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a manifest file
////////////////////////////////////////////////////////////////////////////////

exports.loadManifest = function (path) {
  var name;
  var content;
  var manifest;
  var key;

  name = fs.join(path, "manifest.json");
  content = fs.read(name);
  manifest = JSON.parse(content);

  for (key in manifest.apps) {
    if (manifest.apps.hasOwnProperty(key)) {
      var app = manifest.apps[key];

      console.info("loading app '%s' from '%s'", key, app);

      module.loadAppScript(path, manifest, app);
    }
  }
};

