'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief FoxxContext type
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triagens GmbH, Cologne, Germany
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
/// @author Alan Plum
/// @author Copyright 2015-2016, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const internal = require('internal');
const mimeTypes = require('mime-types');


module.exports = class FoxxContext {
  constructor(service) {
    this.service = service;
    this.argv = [];
  }

  use(path, router) {
    this.service.router.use(path, router);
  }

  registerType(type, def) {
    assert(
      (
        type instanceof RegExp
        || typeof type === 'string'
        || typeof type === 'function'
      ),
      'Type name must be a string, function or RegExp'
    );

    type = mimeTypes.lookup(type) || type;

    assert(
      typeof def === 'object',
      'Type handler definition must be an object'
    );

    def = _.clone(def);

    assert(
      !def.forClient || typeof def.forClient === 'function',
      `Type forClient handler must be a function, not ${typeof def.forClient}`
    );

    assert(
      !def.fromClient || typeof def.fromClient === 'function',
      `Type fromClient handler must be a function, not ${typeof def.fromClient}`
    );

    if (!def.fromClient && typeof def.toClient === 'function') {
      console.log(
        `Found unexpected "toClient" method on type handler for "${type}".`
        + ' Did you mean "forClient"?'
      );
    }

    this.service.types.set(type, def);
  }

  fileName(filename) {
    return fs.safeJoin(this.basePath, filename);
  }

  file(filename, encoding) {
    return fs.readFileSync(this.fileName(filename), encoding);
  }

  path(name) {
    return path.join(this.basePath, name);
  }

  collectionName(name) {
    let fqn = (
      this.collectionPrefix
      + name.replace(/[^a-z0-9]/ig, '_').replace(/(^_+|_+$)/g, '').substr(0, 64)
    );
    if (!fqn.length) {
      throw new Error(`Cannot derive collection name from "${name}"`);
    }
    return fqn;
  }

  collection(name) {
    return internal.db._collection(this.collectionName(name));
  }

  get basePath() {
    return this.service.basePath;
  }

  get baseUrl() {
    return `/_db/${encodeURIComponent(internal.db._name())}/${this.service.mount.slice(1)}`;
  }

  get collectionPrefix() {
    return this.service.collectionPrefix;
  }

  get mount() {
    return this.service.mount;
  }

  get name() {
    return this.service.name;
  }

  get version() {
    return this.service.version;
  }

  get manifest() {
    return this.service.manifest;
  }

  get isDevelopment() {
    return this.service.isDevelopment;
  }

  get isProduction() {
    return !this.isDevelopment;
  }

  get options() {
    return this.service.options;
  }

  get configuration() {
    return this.service.configuration;
  }

  get dependencies() {
    return this.service.dependencies;
  }
};
