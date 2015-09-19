'use strict';
const _ = require('underscore');
const internal = require('internal');
const assert = require('assert');
const Module = require('module');
const path = require('path');
const fs = require('fs');
const parameterTypes = require('org/arangodb/foxx/manager-utils').parameterTypes;
const getExports = require('org/arangodb/foxx').getExports;

const APP_PATH = internal.appPath ? path.resolve(internal.appPath) : undefined;
const STARTUP_PATH = internal.startupPath ? path.resolve(internal.startupPath) : undefined;
const DEV_APP_PATH = internal.devAppPath ? path.resolve(internal.devAppPath) : undefined;

class AppContext {
}

function createConfiguration(definitions) {
  const config = {};
  Object.keys(definitions).forEach(function (name) {
    const def = definitions[name];
    if (def.default !== undefined) {
      config[name] = def.default;
    }
  });
  return config;
}

function createDependencies(definitions, options) {
  const deps = {};
  Object.keys(definitions).forEach(function (name) {
    Object.defineProperty(deps, name, {
      configurable: true,
      enumerable: true,
      get() {
        const mount = options.dependencies[name];
        return mount ? getExports(mount) : undefined;
      }
    });
  });
  return deps;
}

class FoxxService {
  constructor(data) {
    assert(data, 'no arguments');
    assert(data.mount, 'mount path required');
    assert(data.path, `local path required for app "${data.mount}"`);
    assert(data.manifest, `called without manifest for app "${data.mount}"`);

    // mount paths always start with a slash
    this.mount = data.mount;
    this.path = data.path;
    this.isDevelopment = data.isDevelopment || false;

    this.manifest = data.manifest;
    if (!this.manifest.dependencies) {
      this.manifest.dependencies = {};
    }
    if (!this.manifest.configuration) {
      this.manifest.configuration = {};
    }

    this.options = data.options;
    if (!this.options.configuration) {
      this.options.configuration = {};
    }
    if (!this.options.dependencies) {
      this.options.dependencies = {};
    }

    this.configuration = createConfiguration(this.manifest.configuration);
    this.dependencies = createDependencies(this.manifest.dependencies, this.options.dependencies);
    let warnings = this.applyConfiguration(this.options.configuration);
    if (warnings.length) {
      console.warnLines(
        `Stored configuration for app "${data.mount}" has errors:\n${warnings.join('\n  ')}`
      );
    }
    // don't need to apply deps from options -- they work automatically

    if (this.manifest.thumbnail) {
      let thumb = path.resolve(this.root, this.path, this.manifest.thumbnail);
      try {
        this.thumbnail = fs.read64(thumb);
      } catch (e) {
        console.warnLines(
          `Cannot read thumbnail "${thumb}" for app "${data.mount}": ${e.stack}`
        );
      }
    } else {
      this.thumbnail = null;
    }

    let lib = this.manifest.lib || '.';
    this.main = new Module(`foxx:${data.mount}`);
    this.main.filename = path.resolve(this.root, this.path, lib, '.foxx');
    this.main.context.applicationContext = new AppContext(this);
  }

  applyConfiguration(config) {
    const definitions = this.manifest.configuration;
    const warnings = [];

    _.each(config, function (rawValue, name) {
      const def = definitions[name];
      if (!def) {
        warnings.push(`Unexpected option "${name}"`);
        return;
      }

      if (def.required === false && (rawValue === undefined || rawValue === null)) {
        delete this.options.configuration[name];
        this.configuration[name] = def.default;
        return;
      }

      const validate = parameterTypes[def.type];
      let parsedValue = rawValue;
      let warning;

      if (validate.isJoi) {
        let result = validate.required().validate(rawValue);
        if (result.error) {
          warning = result.error.message.replace(/^"value"/, `"${name}"`);
        } else {
          parsedValue = result.value;
        }
      } else {
        try {
          parsedValue = validate(rawValue);
        } catch (e) {
          warning = `"${name}": ${e.message}`;
        }
      }

      if (warning) {
        warnings.push(warning);
      } else {
        this.options.configuration[name] = rawValue;
        this.configuration[name] = parsedValue;
      }
    }, this);

    return warnings;
  }

  applyDependencies(deps) {
    var definitions = this.manifest.dependencies;
    var warnings = [];

    _.each(deps, function (mount, name) {
      const dfn = definitions[name];
      if (!dfn) {
        warnings.push(`Unexpected dependency "${name}"`);
      } else {
        this.options.dependencies[name] = mount;
      }
    }, this);

    return warnings;
  }

  get exports() {
    return this.main.exports;
  }

  get name() {
    return this.manifest.name;
  }

  get version() {
    return this.manifest.version;
  }

  get isSystem() {
    return this.mount.charAt(1) === '_';
  }

  get root() {
    if (this.isSystem) {
      return FoxxService._systemAppPath;
    }
    return FoxxService._appPath;
  }

  get collectionPrefix() {
    if (this.isSystem) {
      return '';
    }
    return this.mount.substr(1).replace(/-/g, '_').replace(/\//g, '_') + '_';
  }

  static get _startupPath() {
    return STARTUP_PATH;
  }

  static get _appPath() {
    return APP_PATH ? (
      path.join(APP_PATH, '_db', internal.db._name())
    ) : undefined;
  }

  static get _systemAppPath() {
    return APP_PATH ? (
      path.join(this._appPath, 'apps', 'system')
    ) : undefined;
  }

  static get _oldAppPath() {
    return APP_PATH ? (
      path.join(APP_PATH, 'databases', internal.db._name())
    ) : undefined;
  }

  static get _devAppPath() {
    return DEV_APP_PATH ? (
      path.join(DEV_APP_PATH, 'databases', internal.db._name())
    ) : undefined;
  }
}

module.exports = FoxxService;
