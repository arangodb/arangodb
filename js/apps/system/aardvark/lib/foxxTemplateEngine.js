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

      fs.write(fs.join(this.folder, "scripts", "setup.js"), this.buildSetup(this.collectionNames));

      fs.write(fs.join(this.folder, "scripts", "teardown.js"), this.buildTeardown(this.collectionNames));
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
          collectionStart,
          repositoryBase = collectionName.substr(1),
          repositoryName,
          repositoryPath,
          repositoryInstance,
          modelPath;
        collectionStart = collectionName.charAt(0);
        if (collectionStart.match("[a-zA-Z]") === null) {
          throw "Collection Name has to start with a letter";
        }
        if (modelName === collectionName) {
          repositoryBase += "_repo";
        }
        repositoryName = collectionStart.toUpperCase() + repositoryBase;
        repositoryInstance = collectionStart.toLowerCase() + repositoryBase;
        repositoryPath = 'repositories/' + collectionName;
        modelPath = 'models/' + modelName;

        this.collectionNames.push(collectionName);
        this.controllers.push({
          url: '/' + collectionName,
          path: 'controllers/' + collectionName + '.js',
          repositoryInstance: repositoryName,
          repository: repositoryInstance,
          repositoryPath: repositoryPath,
          model: i.titleize(modelName),
          modelPath: modelPath,
          basePath: collectionName,
          collectionName: collectionName
        });
        this.repositories.push({
          path: repositoryPath + '.js'
        });
        this.models.push({
          path: modelPath + '.js'
        });
      }, this);
    },

    buildManifest: function() {
      var manifest = {
        name: this.name,
        description: this.description,
        author: this.author,
        version: '0.0.1',
        license: this.license,
        isSystem: false,

        contributors: [],

        controllers: {},

        setup: "scripts/setup.js",
        teardown: "scripts/teardown.js"
      };

      _.each(this.controllers, function (controller) {
        manifest.controllers[controller.url] = controller.path;
      });

      return JSON.stringify(manifest, 0, 2);
    },


    buildSetup: function(collections) {
      var templ = this.template("setup.js.tmpl");

      return templ({
        collections: collections
      });
    },

    buildTeardown: function(collections) {
      var templ = this.template("teardown.js.tmpl");

      return templ({
        collections: collections
      });
    },

    buildController: function(controller) {
      var templ = this.template("controller.js.tmpl");

      return templ(controller);
    },


    buildRepository: function() {
      var templ = this.template("repository.js.tmpl");

      return templ();
    },

    buildModel: function() {
      var templ = this.template("model.js.tmpl");

      return templ();
    },
  });

  exports.Engine = Engine;
}());
