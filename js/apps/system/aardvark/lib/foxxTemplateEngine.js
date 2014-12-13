(function() {
  "use strict";

  var fs = require("fs");
  var _ = require("underscore");

  var Engine = function(opts) {
    this.applicationContext = opts.applicationContext;
    this.path = opts.path;
    this.name = opts.name;
    this.collectionNames = opts.collectionNames;
    this.authenticated = opts.authenticated;
    this.author = opts.author;
    this.description = opts.description;
    this.license = opts.license;
    this._path = this.applicationContext.foxxFilename("templates");
    this.folder = fs.join(this.path, this.name);
  };

  Engine.prototype.write = function() {
    fs.makeDirectory(this.folder);

    fs.write(fs.join(this.folder, "manifest.json"), this.buildManifest({
      name: this.name,
      description: this.description,
      author: this.author,
      license: this.license
    }));
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
