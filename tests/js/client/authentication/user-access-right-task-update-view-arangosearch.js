/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertEqual, assertTrue, assertFalse, assertFail */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for user access rights
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / @author Vadim Kondratyev
// / @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';
var jsunity = require('jsunity');
var analyzers = require("@arangodb/analyzers");
const testHelper = require('@arangodb/test-helper');
const isEqual = testHelper.isEqual;
const deriveTestSuite = testHelper.deriveTestSuite;
const deriveTestSuiteWithnamespace = testHelper.deriveTestSuiteWithnamespace;
const users = require('@arangodb/users');
const helper = require('@arangodb/user-helper');
const tasks = require('@arangodb/tasks');
const pu = require('@arangodb/process-utils');
const download = require('internal').download;
const errors = require('@arangodb').errors;
const db = require('@arangodb').db;
const namePrefix = helper.namePrefix;
const dbName = helper.dbName;
const rightLevels = helper.rightLevels;
const testViewName = `${namePrefix}ViewNew`;
const testViewRename = `${namePrefix}ViewRename`;
const testViewType = "arangosearch";
const testCol1Name = `${namePrefix}Col1New`;
const testCol2Name = `${namePrefix}Col2New`;
const keySpaceId = 'task_update_view_keyspace';
const userSet = helper.userSet;
const systemLevel = helper.systemLevel;
const dbLevel = helper.dbLevel;
const colLevel = helper.colLevel;

const arango = require('internal').arango;
for (let l of rightLevels) {
    systemLevel[l] = new Set();
    dbLevel[l] = new Set();
    colLevel[l] = new Set();
}

const wait = (keySpaceId, key) => {
  for (let i = 0; i < 200; i++) {
    if (getKey(keySpaceId, key)) {
      break;
    }
    require('internal').wait(0.1);
  }
};

const createKeySpace = (keySpaceId) => {
    return executeJS(`return global.KEYSPACE_CREATE('${keySpaceId}', 128, true);`).body === 'true';
};

const dropKeySpace = (keySpaceId) => {
    executeJS(`global.KEYSPACE_DROP('${keySpaceId}');`);
};

const getKey = (keySpaceId, key) => {
    return executeJS(`return global.KEY_GET('${keySpaceId}', '${key}');`).body === 'true';
};

const setKey = (keySpaceId, name) => {
    return executeJS(`global.KEY_SET('${keySpaceId}', '${name}', false);`);
};

const executeJS = (code) => {
    let httpOptions = pu.makeAuthorizationHeaders({
      username: 'root',
      password: ''
    });
    httpOptions.method = 'POST';
    httpOptions.timeout = 1800;
    httpOptions.returnBodyOnError = true;
    return download(arango.getEndpoint().replace('tcp', 'http') + `/_db/${dbName}/_admin/execute?returnAsJSON=true`,
                    code, httpOptions);
};


function hasIResearch (db) {
    return !(db._views() === 0); // arangosearch views are not supported
}
let name = "";
function rootTestCollection (colName, switchBack = true) {
    helper.switchUser('root', dbName);
    let col = db._collection(colName);
    if (switchBack) {
        helper.switchUser(name, dbName);
    }
    return col !== null;
};

function rootCreateCollection (colName) {
    if (!rootTestCollection(colName, false)) {
        db._create(colName);
        if (colLevel['none'].has(name)) {
            if (helper.isLdapEnabledExternal()) {
                users.grantCollection(':role:' + name, dbName, colName, 'none');
            } else {
                users.grantCollection(name, dbName, colName, 'none');
            }
        } else if (colLevel['ro'].has(name)) {
            if (helper.isLdapEnabledExternal()) {
                users.grantCollection(':role:' + name, dbName, colName, 'ro');
            } else {
                users.grantCollection(name, dbName, colName, 'ro');
            }
        } else if (colLevel['rw'].has(name)) {
            if (helper.isLdapEnabledExternal()) {
                users.grantCollection(':role:' + name, dbName, colName, 'rw');
            } else {
                users.grantCollection(name, dbName, colName, 'rw');
            }
        }
    }
    helper.switchUser(name, dbName);
};

function rootPrepareCollection (colName, numDocs = 1, defKey = true) {
    if (rootTestCollection(colName, false)) {
        db._collection(colName).truncate({ compact: false });
        for(var i = 0; i < numDocs; ++i)
        {
            var doc = {prop1: colName + "_1", propI: i, plink: "lnk" + i};
            if (!defKey) {
                doc._key = colName + i;
            }
            db._collection(colName).save(doc, {waitForSync: true});
        }
    }
    helper.switchUser(name, dbName);
};

function rootGrantCollection (colName, user, explicitRight = '') {
    if (rootTestCollection(colName, false)) {
        if (explicitRight !== '' && rightLevels.includes(explicitRight))
        {
            if (helper.isLdapEnabledExternal()) {
                users.grantCollection(':role:' + user, dbName, colName, explicitRight);
            } else {
                users.grantCollection(user, dbName, colName, explicitRight);
            }
        }
    }
    helper.switchUser(user, dbName);
};

function rootDropCollection (colName) {
    helper.switchUser('root', dbName);
    try {
        db._collection(colName).drop();
    } catch (ignored) { }
    helper.switchUser(name, dbName);
};

function rootTestView (viewName, switchBack = true) {
    helper.switchUser('root', dbName);
    let view = db._view(viewName);
    if (switchBack) {
        helper.switchUser(name, dbName);
    }
    return view !== null;
};

function rootGetViewProps (viewName, switchBack = true) {
    helper.switchUser('root', dbName);
    let properties = db._view(viewName).properties();
    if (switchBack) {
        helper.switchUser(name, dbName);
    }
    return properties;
};

function rootCreateView (viewName, properties = null) {
    if (rootTestView(viewName, false)) {
        try {
            db._dropView(viewName);
        } catch (ignored) {}
    }
    let view =  db._createView(viewName, testViewType, {});
    if (properties !== null) {
        view.properties(properties, false);
    }
    helper.switchUser(name, dbName);
};

function rootGetDefaultViewProps () {
    helper.switchUser('root', dbName);
    try {
        var tmpView = db._createView(this.title + "_tmpView", testViewType, {});
        var properties = tmpView.properties();
        db._dropView(this.title + "_tmpView");
    } catch (ignored) { }
    helper.switchUser(name, dbName);
    return properties;
};

function rootDropView (viewName) {
    helper.switchUser('root', dbName);
    try {
        db._dropView(viewName);
    } catch (ignored) { }
    helper.switchUser(name, dbName);
};

const checkError = (e) => {
    assertEqual(e.code, 403, "Expected to get forbidden REST error code, but got another one");
    assertEqual(e.errorNum, errors.ERROR_FORBIDDEN.code, "Expected to get forbidden error number, but got another one");
};

function UserRightsManagement(name) {
  return {
    setUpAll: function() {
      helper.switchUser(name, dbName);
      db._useDatabase(dbName);
      assertEqual(createKeySpace(keySpaceId), true, 'keySpace creation failed for user: ' + name);
      rootCreateCollection(testCol1Name);
      rootCreateCollection(testCol2Name);
      rootPrepareCollection(testCol1Name);
      rootPrepareCollection(testCol2Name);
    },

    setUp: function() {
      rootCreateView(testViewName, { links: { [testCol1Name] : {includeAllFields: true } } });
      helper.switchUser('root', dbName);
    },

    tearDownAll: function() {
      rootDropCollection(testCol1Name);
      rootDropCollection(testCol2Name);

      dropKeySpace(keySpaceId);
    },
    tearDown: function() {
      rootDropView(testViewRename);
      rootDropView(testViewName);
    },

    testCheckAllUsersAreCreated: function() {
      helper.switchUser('root', '_system');
      assertTrue(userSet.size > 0); 
      assertEqual(userSet.size, helper.userCount);
      for (let name of userSet) {
        assertTrue(users.document(name) !== undefined, `Could not find user: ${name}`);
      }
    }
  };
};

    
if (hasIResearch(db)) {
  helper.switchUser('root', '_system');
  helper.removeAllUsers();
  helper.generateAllUsers();

  jsunity.run(function() {
    let suite = {};

    deriveTestSuite(
      UserRightsManagement(name),
      suite,
      "_-_" + name);
    return suite;
  });
  for (name of userSet) {
    let canUse = true;
    try {
      helper.switchUser(name, dbName);
    } catch (e) {
      canUse = false;
    }

    if (canUse) {
      let suite = {
        testCheckAdministrateOnDBLevel: function() {
          assertTrue(userSet.size > 0);
          assertEqual(rootTestView(testViewName), true, 'Precondition failed, view was not found');
          setKey(keySpaceId, name);
          const taskId = 'task_update_view_name_' + name;
          const task = {
            id: taskId,
            name: taskId,
            command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;

                      db._view('${testViewName}').rename('${testViewRename}');
                      global.KEY_SET('${keySpaceId}', '${name}_status', true);
                    } catch (e) {
                      //FIXME: remove IF block after renaming will work in cluster
                      if (e.errorNum === 1470) {
                        global.KEY_SET('${keySpaceId}', '${name}_no_cluster_rename', true);
                        global.KEY_SET('${keySpaceId}', '${name}_status', true);
                      } else {
                        global.KEY_SET('${keySpaceId}', '${name}_no_cluster_rename', false);
                        global.KEY_SET('${keySpaceId}', '${name}_status', false);
                      }
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
          };
          if (dbLevel['rw'].has(name)) {
            tasks.register(task);
            wait(keySpaceId, name);
            if(!getKey(keySpaceId, `${name}_no_cluster_rename`)) {
              assertEqual(getKey(keySpaceId, `${name}_status`), colLevel['ro'].has(name) || colLevel['rw'].has(name), `${name} could update the view with sufficient rights`);
              assertEqual(rootTestView(testViewRename), colLevel['ro'].has(name) || colLevel['rw'].has(name), 'View renaming reported success, but updated view was not found afterwards');
            }
          } else {
            try {
              tasks.register(task);
              wait(keySpaceId, name);
              assertFail(`${name} managed to register a task with insufficient rights`);
            } catch (e) {
              checkError(e);
            } finally {
              assertFalse(getKey(keySpaceId, `${name}_status`), `${name} could update the view with insufficient rights`);
            }
          }
        },
        testViewPropertyExceptLinksPartial: function() {
          assertTrue(rootTestView(testViewName), 'Precondition failed, view was not found');
          setKey(keySpaceId, name);
          const taskId = 'task_update_view_property_not_links_partial_' + name;
          const task = {
            id: taskId,
            name: taskId,
            command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      db._view('${testViewName}').properties({ "cleanupIntervalStep": 1 }, true);
                      global.KEY_SET('${keySpaceId}', '${name}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${name}_status', false);
                    }finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
          };
          if (dbLevel['rw'].has(name)) {
            if(colLevel['rw'].has(name) || colLevel['ro'].has(name)){
              tasks.register(task);
              wait(keySpaceId, name);
              assertTrue(getKey(keySpaceId, `${name}_status`), `${name} could not update the view with sufficient rights`);
              assertEqual(rootGetViewProps(testViewName)["cleanupIntervalStep"], 1, 'View property update reported success, but property was not updated');
            } else {
              tasks.register(task);
              wait(keySpaceId, name);
              assertFalse(getKey(keySpaceId, `${name}_status`), `${name} could update the view with insufficient rights`);
            }
          } else {
            try {
              tasks.register(task);
              wait(keySpaceId, name);
              assertFail( `${name} managed to register a task with insufficient rights`);
            } catch (e) {
              checkError(e);
            } finally {
              assertFalse(getKey(keySpaceId, `${name}_status`), `${name} could update the view with insufficient rights`);
            }
          }
        },
        testViewByPropertyExceptLinksFull: function() {
          assertTrue(rootTestView(testViewName), 'Precondition failed, view was not found');
          setKey(keySpaceId, name);
          const taskId = 'task_update_view_property_not_links_full_' + name;
          const task = {
            id: taskId,
            name: taskId,
            command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      db._view('${testViewName}').properties({ "cleanupIntervalStep": 1 }, false);
                      global.KEY_SET('${keySpaceId}', '${name}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${name}_status', false);
                    }finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
          };
          if (dbLevel['rw'].has(name)) {
            if(colLevel['rw'].has(name) || colLevel['ro'].has(name)){
              tasks.register(task);
              wait(keySpaceId, name);
              assertTrue(getKey(keySpaceId, `${name}_status`), `${name} could not update the view with sufficient rights`);
              assertEqual(rootGetViewProps(testViewName)["cleanupIntervalStep"], 1, 'View property update reported success, but property was not updated');
            } else {
              tasks.register(task);
              wait(keySpaceId, name);
              assertFalse(getKey(keySpaceId, `${name}_status`), `${name} could update the view with insufficient rights`);
            }
          } else {
            try {
              tasks.register(task);
              wait(keySpaceId, name);
              assertFail(`${name} managed to register a task with insufficient rights`);
            } catch (e) {
              checkError(e);
              return;
            } finally {
              assertFalse(getKey(keySpaceId, `${name}_status`), `${name} could update the view with insufficient rights`);
            }
          }
        },
        testViewBypPropertiesRemoveFull: function() {
          assertTrue(rootTestView(testViewName), 'Precondition failed, view was not found');
          setKey(keySpaceId, name);
          const taskId = 'task_update_view_properties_remove_full_' + name;
          const task = {
            id: taskId,
            name: taskId,
            command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      db._view('${testViewName}').properties({}, false);
                      global.KEY_SET('${keySpaceId}', '${name}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${name}_status', false);
                    }finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
          };
          if (dbLevel['rw'].has(name)) {
            if(colLevel['rw'].has(name) || colLevel['ro'].has(name)){
              tasks.register(task);
              wait(keySpaceId, name);
              assertTrue(getKey(keySpaceId, `${name}_status`), `${name} could not update the view with sufficient rights`);
              assertTrue(isEqual(rootGetViewProps(testViewName, true), rootGetDefaultViewProps()),
                'View properties update reported success, but properties were not updated');
            } else {
              tasks.register(task);
              wait(keySpaceId, name);
              assertFalse(getKey(keySpaceId, `${name}_status`), `${name} could update the view with insufficient rights`);
            }
          } else {
            try {
              tasks.register(task);
              wait(keySpaceId, name);
              assertFail(`${name} managed to register a task with insufficient rights`);
            } catch (e) {
              checkError(e);
              return;
            } finally {
              assertFalse(getKey(keySpaceId, `${name}_status`), `${name} could update the view with insufficient rights`);
            }
          }
        },
        testViewByExistingLinkUpdatePartial: function() {
          assertTrue(rootTestView(testViewName), 'Precondition failed, view was not found');
          setKey(keySpaceId, name);
          const taskId = 'task_update_view_existing_link_partial_' + name;
          const task = {
            id: taskId,
            name: taskId,
            command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      db._view('${testViewName}').properties({ links: { ['${testCol1Name}']: { includeAllFields: true, analyzers: ['text_de','text_en'] } } }, true);
                      global.KEY_SET('${keySpaceId}', '${name}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${name}_status', false);
                    }finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
          };
          if (dbLevel['rw'].has(name)) {
            if(colLevel['rw'].has(name) || colLevel['ro'].has(name))
            {
              tasks.register(task);
              wait(keySpaceId, name);
              assertTrue(getKey(keySpaceId, `${name}_status`), `${name} could not update the view with sufficient rights`);
              assertEqual(rootGetViewProps(testViewName)["links"][testCol1Name]["analyzers"], ["text_de", "text_en"], 'View link update reported success, but property was not updated');
            } else {
              tasks.register(task);
              wait(keySpaceId, name);
              assertFalse(getKey(keySpaceId, `${name}_status`), `${name} could update the view with insufficient rights`);
            }
          } else {
            try {
              tasks.register(task);
              wait(keySpaceId, name);
              assertFail(`${name} managed to register a task with insufficient rights`);
            } catch (e) {
              checkError(e);
            } finally {
              assertFalse(getKey(keySpaceId, `${name}_status`), `${name} could update the view with insufficient rights`);
            }

          }
        }
      };
      jsunity.run(function() {
        return deriveTestSuiteWithnamespace(
          UserRightsManagement(name),
          suite,
          "_-_" + name);
      });
    }
  }
}
return jsunity.done();
