'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx template engine
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
/// @author Michael Hackstein
/// @author Alan Plum
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var _ = require('underscore');
var fs = require('fs');
var internal = require('internal');
var i = require('i')();
var templatePath = fs.join(
  internal.startupPath,
  'server',
  'modules',
  'org',
  'arangodb',
  'foxx',
  'templates'
);

function Engine(opts) {
  this._path = templatePath;
  this.folder = opts.path;
  this.name = opts.name;
  this.authenticated = opts.authenticated;
  this.author = opts.author;
  this.description = opts.description;
  this.license = opts.license;
  this.determineFromCollectionNames(opts.collectionNames);
}

_.extend(Engine.prototype, {
  write: function() {
    fs.makeDirectory(this.folder);
    fs.makeDirectory(fs.join(this.folder, 'controllers'));
    fs.makeDirectory(fs.join(this.folder, 'models'));
    fs.makeDirectory(fs.join(this.folder, 'repositories'));
    fs.makeDirectory(fs.join(this.folder, 'scripts'));
    fs.makeDirectory(fs.join(this.folder, 'test'));
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

    fs.write(fs.join(this.folder, 'scripts', 'setup.js'), this.buildSetup(this.collectionNames));
    fs.write(fs.join(this.folder, 'scripts', 'teardown.js'), this.buildTeardown(this.collectionNames));
    fs.write(fs.join(this.folder, 'test', 'example.js'), this.buildTest());
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
      var modelBase = i.singularize(collectionName).substr(1),
        collectionStart,
        repositoryBase = collectionName.substr(1),
        repositoryName,
        repositoryPath,
        repositoryInstance,
        modelPath,
        modelName,
        modelInstance;
      collectionStart = collectionName.charAt(0);
      if (collectionStart.match(/[a-z]/i) === null) {
        throw new Error('Collection name has to start with a letter');
      }
      if (modelBase === repositoryBase) {
        repositoryBase += 'Repo';
      }
      modelName = collectionStart.toUpperCase() + modelBase;
      modelInstance = collectionStart.toLowerCase() + modelBase;
      repositoryName = collectionStart.toUpperCase() + repositoryBase;
      repositoryInstance = collectionStart.toLowerCase() + repositoryBase;
      repositoryPath = 'repositories/' + collectionName;
      modelPath = 'models/' + modelName.toLowerCase();

      this.collectionNames.push(collectionName);
      this.controllers.push({
        url: '/' + collectionName,
        path: 'controllers/' + collectionName + '.js',
        repositoryInstance: repositoryInstance,
        repository: repositoryName,
        repositoryPath: repositoryPath,
        modelInstance: modelInstance,
        model: modelName,
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
      controllers: {},
      scripts: {
        setup: 'scripts/setup.js',
        teardown: 'scripts/teardown.js'
      },
      tests: 'test/**/*.js'
    };

    _.each(this.controllers, function (controller) {
      manifest.controllers[controller.url] = controller.path;
    });

    return JSON.stringify(manifest, 0, 2);
  },


  buildSetup: function(collections) {
    var templ = this.template('setup.js.tmpl');

    return templ({
      collections: collections
    });
  },

  buildTeardown: function(collections) {
    var templ = this.template('teardown.js.tmpl');

    return templ({
      collections: collections
    });
  },

  buildController: function(controller) {
    var templ = this.template('controller.js.tmpl');

    return templ(controller);
  },


  buildRepository: function() {
    var templ = this.template('repository.js.tmpl');

    return templ();
  },

  buildModel: function() {
    var templ = this.template('model.js.tmpl');

    return templ();
  },

  buildTest: function () {
    var templ = this.template('test.js.tmpl');

    return templ();
  }
});

exports.Engine = Engine;
