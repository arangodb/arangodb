'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2015-2016 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const internal = require('internal');
const mimeTypes = require('mime-types');
const createSwaggerRouteHandler = require('@arangodb/foxx/swagger');

module.exports =
  class FoxxContext {
    constructor (service) {
      this.service = service;
      this.argv = [];
    }

    apiDocumentation (opts) {
      return createSwaggerRouteHandler(this.mount, opts);
    }

    createDocumentationRouter (opts) {
      const createRouter = require('@arangodb/foxx/router');
      const router = createRouter();
      router.get('/*', createSwaggerRouteHandler(this.mount, opts));
      return router;
    }

    use (path, router, name) {
      return this.service.router.use(path, router, name);
    }

    reverse (routeName, params, suffix) {
      return this.service.tree.reverse(
        this.service.router._routes,
        routeName,
        params,
        suffix
      );
    }

    registerType (type, def) {
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

      if (!def.fromClient && typeof def.toClient === 'function') {
        console.log(
          `Found unexpected "toClient" method on type handler for "${type}".`
          + ' Did you mean "forClient"?'
        );
      }

      assert(
        !def.forClient || typeof def.forClient === 'function',
        `Type forClient handler must be a function, not ${typeof def.forClient}`
      );

      assert(
        !def.fromClient || typeof def.fromClient === 'function',
        `Type fromClient handler must be a function, not ${typeof def.fromClient}`
      );

      this.service.types.set(type, def);
    }

    fileName (filename) {
      return path.join(this.basePath, filename);
    }

    file (filename, encoding) {
      return fs.readFileSync(this.fileName(filename), encoding);
    }

    collectionName (name) {
      const fqn = (
      this.collectionPrefix + name
        .replace(/[^a-z0-9]/ig, '_')
        .replace(/(^_+|_+$)/g, '')
        .substr(0, 64)
      );
      assert(fqn.length > 0, `Cannot derive collection name from "${name}"`);
      return fqn;
    }

    collection (name) {
      return internal.db._collection(this.collectionName(name));
    }

    get basePath () {
      return this.service.basePath;
    }

    get baseUrl () {
      return `/_db/${encodeURIComponent(internal.db._name())}${this.service.mount}`;
    }

    get collectionPrefix () {
      return this.service.collectionPrefix;
    }

    get mount () {
      return this.service.mount;
    }

    get manifest () {
      return this.service.manifest;
    }

    get isDevelopment () {
      return this.service.isDevelopment;
    }

    get isProduction () {
      return !this.isDevelopment;
    }

    get configuration () {
      return this.service.configuration;
    }

    get dependencies () {
      return this.service.dependencies;
    }

    toJSON () {
      return {
        argv: this.argv,
        basePath: this.basePath,
        baseUrl: this.baseUrl,
        collectionPrefix: this.collectionPrefix,
        configuration: this.configuration,
        dependencies: this.dependencies,
        isDevelopment: this.isDevelopment,
        isProduction: this.isProduction,
        manifest: this.manifest,
        mount: this.mount
      };
    }
};
