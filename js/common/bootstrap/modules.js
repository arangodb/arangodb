/*jshint ignore:start */
/*eslint no-unused-vars:0, quotes:0 */
/*eslint-disable */
;(function () {
  'use strict'
  /*eslint-enable */

  // //////////////////////////////////////////////////////////////////////////////
  // / DISCLAIMER
  // /
  // / Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
  // / Copyright holder is triAGENS GmbH, Cologne, Germany
  // /
  // / @author Alan Plum
  // / @author Copyright 2015-2016, ArangoDB GmbH, Cologne, Germany
  // /
  // / Based on Node.js 4.1.0 /lib/module.js:
  // /
  // / Copyright Joyent, Inc. and other Node contributors.
  // /
  // / Permission is hereby granted, free of charge, to any person obtaining a
  // / copy of this software and associated documentation files (the
  // / "Software"), to deal in the Software without restriction, including
  // / without limitation the rights to use, copy, modify, merge, publish,
  // / distribute, sublicense, and/or sell copies of the Software, and to permit
  // / persons to whom the Software is furnished to do so, subject to the
  // / following conditions:
  // /
  // / The above copyright notice and this permission notice shall be included
  // / in all copies or substantial portions of the Software.
  // /
  // / THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  // / OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  // / MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
  // / NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  // / DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
  // / OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
  // / USE OR OTHER DEALINGS IN THE SOFTWARE.
  // /
  // //////////////////////////////////////////////////////////////////////////////

  const NATIVE_MODULES = global.SCAFFOLDING_MODULES;
  const fs = NATIVE_MODULES.fs;
  const path = NATIVE_MODULES.path;
  const assert = NATIVE_MODULES.assert;
  const internal = NATIVE_MODULES.internal;
  const console = NATIVE_MODULES.console;
  delete global.SCAFFOLDING_MODULES;
  delete global.DEFINE_MODULE;
  const LOADING = [];
  const $_MODULE_ROOT = Symbol.for('@arangodb/module.root');
  const $_MODULE_CONTEXT = Symbol.for('@arangodb/module.context');

  const GLOBAL_PATHS = [];
  const ROOT_PATH = fs.normalize(fs.makeAbsolute(internal.startupPath));
  global.MODULES_PATH.forEach(function (p) {
    p = fs.normalize(fs.makeAbsolute(p));
    GLOBAL_PATHS.push(p);
    if (p.match(/[\/\\]node[\/\\]?$/)) {
      GLOBAL_PATHS.push(path.join(p, 'node_modules'));
    }
  });
  delete global.MODULES_PATH;

  function internalModuleStat (path) {
    if (fs.isDirectory(path)) return 1;
    else if (fs.isFile(path)) return 0;
    else return -1;
  }

  // If obj.hasOwnProperty has been overridden, then calling
  // obj.hasOwnProperty(prop) will break.
  // See: https://github.com/joyent/node/issues/1707
  function hasOwnProperty (obj, prop) {
    return Object.prototype.hasOwnProperty.call(obj, prop);
  }

  function createRequire (module) {
    function require (path) {
      assert(path, 'missing path');
      assert(typeof path === 'string', 'path must be a string');
      if (path === '@arangodb/locals') {
        return {context: module.context};
      }
      return Module._load(path, module);
    }

    require.resolve = function (request) {
      return Module._resolveFilename(request, module);
    };

    // Enable support to add extra extension types
    require.extensions = Module._extensions;

    require.cache = Module._cache;

    require.aliases = new Map();

    return require;
  }

  function Module (id, parent) {
    this.id = id;
    this.exports = {};
    this.require = createRequire(this);
    this.parent = parent;
    if (parent && parent.children) {
      parent.children.push(this);
    }

    Object.defineProperty(this, $_MODULE_CONTEXT, {
      value: {
        print: internal.print,
        process: NATIVE_MODULES.process,
        console: NATIVE_MODULES.console,
        module: this,
        exports: this.exports,
        require: this.require,
        __filename: null,
        __dirname: null
      }
    });

    if (parent) {
      this.context = parent.context;
      this.require.cache = parent.require.cache;
      this.require.aliases = parent.require.aliases;
      this[$_MODULE_ROOT] = parent[$_MODULE_ROOT];
      Object.keys(parent[$_MODULE_CONTEXT]).forEach(function (key) {
        if (!hasOwnProperty(this[$_MODULE_CONTEXT], key)) {
          this[$_MODULE_CONTEXT][key] = parent[$_MODULE_CONTEXT][key];
        }
      }.bind(this));
    }

    Object.defineProperty(this, 'filename', {
      configurable: true,
      enumerable: true,
      get() {
        return this[$_MODULE_CONTEXT].__filename;
      },
      set(filename) {
        this[$_MODULE_CONTEXT].__filename = filename;
        this[$_MODULE_CONTEXT].__dirname = filename === null ? null : path.dirname(filename);
      }
    });

    this.filename = null;
    this.loaded = false;
    this.children = [];
  }
  NATIVE_MODULES.module = Module;

  Module._cache = {};
  Module._pathCache = {};
  Module._dbCache = {};
  Module._extensions = {};
  var modulePaths = GLOBAL_PATHS;
  Module.globalPaths = modulePaths;

  // given a module name, and a list of paths to test, returns the first
  // matching file in the following precedence.
  //
  // require("a.<ext>")
  //   -> a.<ext>
  //
  // require("a")
  //   -> a
  //   -> a.<ext>
  //   -> a/index.<ext>

  // check if the directory is a package.json dir
  const packageMainCache = {};

  function readPackage (requestPath) {
    if (hasOwnProperty(packageMainCache, requestPath)) {
      return packageMainCache[requestPath];
    }

    var jsonPath = path.resolve(requestPath, 'package.json');
    var longPath = path._makeLong(jsonPath);

    if (!fs.exists(longPath)) {
      return false;
    }

    var json = fs.read(longPath);

    try {
      var pkg = JSON.parse(json).main;
      packageMainCache[requestPath] = pkg;
    } catch (e) {
      e.path = jsonPath;
      e.message = 'Error parsing ' + jsonPath + ': ' + e.message;
      throw e;
    }
    return pkg;
  }

  function tryPackage (requestPath, exts) {
    var pkg = readPackage(requestPath);

    if (!pkg) return false;

    var filename = path.resolve(requestPath, pkg);
    return tryFile(filename) || tryExtensions(filename, exts) ||
      tryExtensions(path.resolve(filename, 'index'), exts);
  }

  // check if the file exists and is not a directory
  function tryFile (requestPath) {
    const rc = internalModuleStat(path._makeLong(requestPath));
    return rc === 0 && requestPath;
  }

  // given a path check a the file exists with any of the set extensions
  function tryExtensions (p, exts) {
    for (var i = 0, EL = exts.length; i < EL; i++) {
      var filename = tryFile(p + exts[i]);

      if (filename) {
        return filename;
      }
    }
    return false;
  }

  var warned = false;
  Module._findPath = function (request, paths) {
    var exts = Object.keys(Module._extensions);

    if (path.isAbsolute(request)) {
      paths = [''];
    }

    var s = request.slice(-1);
    var trailingSlash = (s === '/' || s === '\\');

    var cacheKey = JSON.stringify({request: request, paths: paths});
    if (Module._pathCache[cacheKey]) {
      return Module._pathCache[cacheKey];
    }

    // For each path
    for (var i = 0, PL = paths.length; i < PL; i++) {
      // Don't search further if path doesn't exist
      if (paths[i] && internalModuleStat(path._makeLong(paths[i])) < 1) continue;
      var basePath = path.resolve(paths[i], request);
      var filename;

      if (!trailingSlash) {
        const rc = internalModuleStat(path._makeLong(basePath));
        if (rc === 0) { // File.
          filename = basePath;
        } else if (rc === 1) { // Directory.
          filename = tryPackage(basePath, exts);
        }

        if (!filename) {
          // try it with each of the extensions
          filename = tryExtensions(basePath, exts);
        }
      }

      if (!filename) {
        filename = tryPackage(basePath, exts);
      }

      if (!filename) {
        // try it with each of the extensions at "index"
        filename = tryExtensions(path.resolve(basePath, 'index'), exts);
      }

      if (filename) {
        // Warn once if '.' resolved outside the module dir
        if (request === '.' && i > 0 && !warned) {
          warned = true;
          console.warnLines(
            "warning: require('.') resolved outside the package " +
            'directory. This functionality is deprecated and will be removed ' +
            'soon.'
          );
        }

        Module._pathCache[cacheKey] = filename;
        return filename;
      }
    }
    return false;
  };

  // 'from' is the __dirname of the module.
  Module._nodeModulePaths = function (from, root) {
    // guarantee that 'from' is absolute.
    from = path.resolve(from);

    // note: this approach *only* works when the path is guaranteed
    // to be absolute.  Doing a fully-edge-case-correct path.split
    // that works on both Windows and Posix is non-trivial.
    var splitRe = (internal.platform === 'win32' || internal.platform === 'win64') ? /[\/\\]/ : /\//;
    var paths = [];
    var parts = from.split(splitRe);
    var inRoot = root && from.indexOf(root) === 0;
    if (!inRoot) {
      root = ROOT_PATH;
      inRoot = from.indexOf(root) === 0;
    }

    for (var tip = parts.length - 1; tip >= 0; tip--) {
      // don't search in .../node_modules/node_modules
      if (parts[tip] === 'node_modules') continue;
      var dir = parts.slice(0, tip + 1).concat('node_modules').join(path.sep);
      if (inRoot && dir.indexOf(root) !== 0) break;
      paths.push(dir);
    }

    return paths;
  };

  Module._resolveLookupPaths = function (request, parent) {
    if (hasOwnProperty(NATIVE_MODULES, request)) {
      return [request, []];
    }

    var start = request.substring(0, 2);
    if (start !== './' && start !== '..') {
      var paths = modulePaths.slice(0);
      if (parent) {
        if (!parent.paths) parent.paths = [];
        paths = parent.paths.concat(paths);
      }

      // Maintain backwards compat with certain broken uses of require('.')
      // by putting the module's directory in front of the lookup paths.
      if (request === '.') {
        if (parent && parent.filename) {
          paths.splice(0, 0, path.dirname(parent.filename));
        } else {
          paths.splice(0, 0, path.resolve(request));
        }
      }

      return [request, paths];
    }

    // with --eval, parent.id is not set and parent.filename is null
    if (!parent || !parent.id || !parent.filename) {
      // make require('./path/to/foo') work - normally the path is taken
      // from realpath(__filename) but with eval there is no filename
      var mainPaths = ['.'].concat(modulePaths);
      mainPaths = Module._nodeModulePaths('.').concat(mainPaths);
      return [request, mainPaths];
    }

    // Is the parent an index module?
    // We can assume the parent has a valid extension,
    // as it already has been accepted as a module.
    var isIndex = /^index\.\w+?$/.test(path.basename(parent.filename));
    var parentIdPath = isIndex ? parent.id : path.dirname(parent.id);
    var id = path.resolve(parentIdPath, request);

    // make sure require('./path') and require('path') get distinct ids, even
    // when called from the toplevel js file
    if (parentIdPath === '.' && id.indexOf('/') === -1) {
      id = './' + id;
    }

    return [id, [path.dirname(parent.filename)]];
  };

  Module._resolveDbModule = function (request) {
    if (request.charAt(0) !== '/') {
      request = '/' + request;
    }

    try {
      var dbModule = Module._dbCache[request];

      // _modules is an optional collection. only use it if it is present.
      if (!dbModule && internal.db !== undefined && internal.db._modules) {
        dbModule = internal.db._modules.firstExample({path: request});

        if (!dbModule) {
          // try again, but prefix module with '/db' as some modules seem
          // to have been saved with that prefix...
          dbModule = internal.db._modules.firstExample({path: '/db:' + request});

          if (!dbModule) {
            return null;
          }
        }

        Module._dbCache[request] = dbModule;
      }

      return dbModule;
    } catch (err) {
      // something went wrong while accessing _modules collection
      // ArangoDB will continue to run without it, so just log a message and
      // continue operations
      console.debug("unable to load module '%s': %s", request, String(err));
      return null;
    }
  };

  function isGlobalModule (filename) {
    return modulePaths.some(function (modulePath) {
      return filename.indexOf(modulePath) === 0;
    });
  }

  // Check the cache for the requested file.
  // 1. If a module already exists in the cache: return its exports object.
  // 2. If the module is native: call `NativeModule.require()` with the
  //    filename and return the result.
  // 3. Otherwise, create a new module for the file and save it to the cache.
  //    Then have it load  the file contents before returning its exports
  //    object.
  Module._load = function (request, parent, isMain) {
    request = request.replace(/^org\/arangodb/, '@arangodb');
    request = (parent && parent.require.aliases.get(request)) || request;

    var filename = request;
    var dbModule = false;
    var match = request.match(/^\/?db:(\/(\/_modules)?)?(\/.+)/);

    if (match) {
      dbModule = Module._resolveDbModule(match[3]);
      if (!dbModule) {
        throw new internal.ArangoError({
          errorNum: internal.errors.ERROR_MODULE_NOT_FOUND.code,
          errorMessage: internal.errors.ERROR_MODULE_NOT_FOUND.message
            + '\nFile: ' + request
        });
      }
    } else {
      try {
        filename = Module._resolveFilename(request, parent);
      } catch (e) {
        dbModule = Module._resolveDbModule(request);
        if (!dbModule) {
          throw e;
        }
      }
    }

    if (dbModule) {
      filename = dbModule.path;
    }

    var cache;
    if (parent && (dbModule || !isGlobalModule(filename))) {
      cache = parent.require.cache;
    } else {
      cache = Module._cache;
    }

    var cachedModule = cache[filename];
    if (cachedModule) {
      return cachedModule.exports;
    }

    if (hasOwnProperty(NATIVE_MODULES, filename)) {
      return NATIVE_MODULES[filename];
    }

    var module = new Module(filename, parent);

    if (isMain) {
      module.id = '.';
    }

    cache[filename] = module;
    var loading = {cache, filename};
    LOADING.push(loading);

    var hadException = true;

    try {
      if (dbModule) {
        module._loadDbModule(dbModule);
      } else {
        module.load(filename);
      }
      hadException = false;
    } finally {
      if (hadException) {
        delete cache[filename];
      }

      var i = LOADING.indexOf(loading);
      if (i !== -1) {
        LOADING.splice(i, 1);
      }
    }

    return module.exports;
  };

  Module._cleanupCancelation = function () {
    while (LOADING.length) {
      let loading = LOADING.pop();
      delete loading.cache[loading.filename];
    }
  };

  Module._resolveFilename = function (request, parent) {
    if (hasOwnProperty(NATIVE_MODULES, request)) {
      return request;
    }

    var resolvedModule = Module._resolveLookupPaths(request, parent);
    var id = resolvedModule[0];
    var paths = resolvedModule[1];

    // look up the filename first, since that's the cache key.
    var filename = Module._findPath(request, paths);
    if (!filename) {
      throw new internal.ArangoError({
        errorNum: internal.errors.ERROR_MODULE_NOT_FOUND.code,
        errorMessage: internal.errors.ERROR_MODULE_NOT_FOUND.message
          + '\nFile: ' + request
      });
    }
    return filename;
  };

  // Given a file name, pass it to the proper extension handler.
  Module.prototype.load = function (filename) {
    assert(!this.loaded);
    this.filename = filename;
    this.paths = Module._nodeModulePaths(path.dirname(filename), this[$_MODULE_ROOT]);

    var extension = path.extname(filename) || '.js';
    if (!Module._extensions[extension]) extension = '.js';

    try {
      Module._extensions[extension](this, filename);
    } catch (e) {
      if (e.errorNum !== internal.errors.ERROR_MODULE_FAILURE.code) {
        let msg = `${internal.errors.ERROR_MODULE_FAILURE.message}\nReason: ${e}`;

        if (e.fileName !== undefined) {
          msg += `\nFile: ${e.fileName}`;
        } else {
          msg += `\nFile: ${filename}`;
        }
        if (e.lineNumber !== undefined) {
          msg += `\nLine: ${e.lineNumber}`;
        }
        if (e.columnNumber !== undefined) {
          msg += `\nColumn: ${e.columnNumber}`;
        }
        throw Object.assign(
          new internal.ArangoError({
            errorNum: internal.errors.ERROR_MODULE_FAILURE.code,
            errorMessage: msg
          }),
          {cause: e}
        );
      }
      throw e;
    }

    this.loaded = true;
  };

  Module.prototype._loadDbModule = function (dbModule) {
    assert(!this.loaded);
    const filename = `db://_modules${dbModule.path}`;
    this.filename = filename;
    this._compile(dbModule.content, filename);
    this.loaded = true;
  };

  // Run the file contents in the correct scope or sandbox. Expose
  // the correct helper variables (require, module, exports) to
  // the file.
  // Returns exception, if any.
  Module.prototype._compile = function (content, filename) {
    // remove shebang
    content = content.replace(/^\#\!.*/, '');

    this.filename = filename;

    var args = this[$_MODULE_CONTEXT];
    var keys = Object.keys(args);
    // Do not use Function constructor or line numbers will be wrong
    var wrapper = `(function (${keys.join(', ')}) {${content}\n})`;

    var fn = internal.executeScript(wrapper, undefined, filename);

    if (typeof fn !== 'function') {
      throw new TypeError(`Expected internal.executeScript to return a function, not ${typeof fn}`);
    }

    fn.apply(args.exports, keys.map(function (key) {
      return args[key];
    }));
  };

  function stripBOM (content) {
    // Remove byte order marker. This catches EF BB BF (the UTF-8 BOM)
    // because the buffer-to-string conversion in `fs.readFileSync()`
    // translates it to FEFF, the UTF-16 BOM.
    if (content.charCodeAt(0) === 0xFEFF) {
      content = content.slice(1);
    }
    return content;
  }

  // Native extension for .js
  Module._extensions['.js'] = function (module, filename) {
    var content = fs.readFileSync(filename, 'utf8');
    module._compile(stripBOM(content), filename);
  };

  // Native extension for .json
  Module._extensions['.json'] = function (module, filename) {
    var content = fs.readFileSync(filename, 'utf8');
    try {
      module.exports = JSON.parse(stripBOM(content));
    } catch (err) {
      err.message = filename + ': ' + err.message;
      throw err;
    }
  };

  // backwards compatibility
  Module.Module = Module;

  global.require = function (request) {
    if (request === '@arangodb/locals') {
      return {};
    }
    return Module._load(request);
  };
}());
