(function() {
  "use strict";

  var fs = require("fs");
  var _ = require("underscore");

  var Engine = function(path) {
    this._path = path;
  };

  Engine.prototype.buildManifest = function(info) {
    if (!this.hasOwnProperty("_manifest")) {
      this._manifest = _.template(
        fs.read(fs.join(this._path, "manifest.json.tmpl"))
      );
    }
    return this._manifest(info);
  };

  exports.Engine = Engine;
}());
