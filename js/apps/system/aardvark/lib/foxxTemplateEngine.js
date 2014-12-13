(function() {
  "use strict";

  var fs = require("fs"),
    _ = require("underscore"),
    Engine;

  Engine = function(opts) {
    this.applicationContext = opts.applicationContext;
    this.path = opts.path;
    this.name = opts.name;
    this.authenticated = opts.authenticated;
    this.author = opts.author;
    this.description = opts.description;
    this.license = opts.license;
    this.determineFromCollectionNames(opts.collectionNames);
    this._path = this.applicationContext.foxxFilename("templates");
    this.folder = fs.join(this.path, this.name);
  };

  _.extend(Engine.prototype, {
    write: function() {
      fs.makeDirectory(this.folder);
      fs.write(fs.join(this.folder, "manifest.json"), this.buildManifest());
    },

    template: function(name) {
      return _.template(fs.read(fs.join(this._path, name)));
    },

    determineFromCollectionNames: function (collectionNames) {
      this.collectionNames = [];
      this.controllers = [];

      _.each(collectionNames, function (collectionName) {
        this.collectionNames.push(collectionName);
        this.controllers.push({
          url: '/' + collectionName,
          path: 'controllers/' + collectionName + '.js'
        });
      }, this);
    },

    buildManifest: function() {
      var manifest = this.template("manifest.json.tmpl");

      return manifest({
        name: this.name,
        description: this.description,
        author: this.author,
        license: this.license,
        controllers: this.controllers
      });
    }
  });

  exports.Engine = Engine;
}());
