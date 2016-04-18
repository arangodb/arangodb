'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2013 triAGENS GmbH, Cologne, Germany
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Alan Plum
////////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const version = String(require('@arangodb/database-version').CURRENT_VERSION);
const collection = db._collection('_cluster_kickstarter_plans');

module.exports = {
  hasConfig() {
    return Boolean(this.loadConfig());
  },

  clear() {
    collection.truncate();
  },

  loadConfig() {
    return collection.any();
  },

  getPlan() {
    return this.loadConfig().plan;
  },

  getRunInfo() {
    return this.loadConfig().runInfo;
  },

  getCredentials() {
    return this.loadConfig().user;
  },

  updateConfig(config) {
    const old = this.loadConfig();
    collection.update(old._id, config);
    return true;
  },

  saveCredentials(name, passwd) {
    const config = {user: {name, passwd}};
    const old = this.loadConfig();
    collection.update(old._id, config);
    return true;
  },

  storeConfig(config) {
    collection.truncate();
    config.version = version;
    collection.save(config);
    return true;
  },

  removeRunInfo() {
    const old = this.loadConfig();
    delete old.runInfo;
    collection.replace(old._id, old);
    return true;
  },

  replaceRunInfo(runInfo) {
    const old = this.loadConfig();
    collection.update(old._id, {runInfo, version});
    return true;
  }
};
