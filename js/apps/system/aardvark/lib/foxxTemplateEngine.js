(function() {
  "use strict";

  var fs = require("fs"),
    _ = require("underscore"),
    i = require('i')(),
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
      fs.makeDirectory(fs.join(this.folder, 'controllers'));
      fs.makeDirectory(fs.join(this.folder, 'models'));
      fs.makeDirectory(fs.join(this.folder, 'repositories'));
      fs.makeDirectory(fs.join(this.folder, 'scripts'));
      fs.write(fs.join(this.folder, 'manifest.json'), this.buildManifest());

      _.each(this.repositories, function (repository) {
        fs.write(fs.join(this.folder, repository.path), this.buildRepository());
      }, this);

      _.each(this.models, function (model) {
        fs.write(fs.join(this.folder, model.path), this.buildModel());
      }, this);

      _.each(this.controllers, function (controller) {
        fs.write(fs.join(this.folder, controller.path), this.buildController(controller));
      }, this);
    },

    template: function(name) {
      return _.template(fs.read(fs.join(this._path, name)));
    },

    determineFromCollectionNames: function (collectionNames) {
      this.collectionNames = [];
      this.controllers = [];
      this.repositories = [];
      this.models = [];

      _.each(collectionNames, function (collectionName) {
        var modelName = i.singularize(collectionName),
          repositoryPath = 'repositories/' + collectionName + '.js',
          modelPath = 'models/' + modelName + '.js';

        this.collectionNames.push(collectionName);
        this.controllers.push({
          url: '/' + collectionName,
          path: 'controllers/' + collectionName + '.js',
          repositoryInstance: collectionName,
          repository: i.titleize(collectionName),
          repositoryPath: repositoryPath,
          model: i.titleize(modelName),
          modelPath: modelPath,
          basePath: collectionName,
          collectionName: collectionName
        });
        this.repositories.push({
          path: repositoryPath
        });
        this.models.push({
          path: modelPath
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
    },

    buildController: function(controller) {
      var manifest = this.template("controller.js.tmpl");

      return manifest(controller);
    },


    buildRepository: function() {
      var manifest = this.template("repository.js.tmpl");

      return manifest();
    },

    buildModel: function() {
      var manifest = this.template("model.js.tmpl");

      return manifest();
    },
  });

  exports.Engine = Engine;
}());
