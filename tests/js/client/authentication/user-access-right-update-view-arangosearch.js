/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertEqual, assertTrue, assertFalse, assertFail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

const jsunity = require('jsunity');
const analyzers = require("@arangodb/analyzers");
const testHelper = require('@arangodb/test-helper');
const isEqual = testHelper.isEqual;
const deriveTestSuite = testHelper.deriveTestSuite;
const deriveTestSuiteWithnamespace = testHelper.deriveTestSuiteWithnamespace;
const users = require('@arangodb/users');
const helper = require('@arangodb/testutils/user-helper');
const errors = require('@arangodb').errors;
const db = require('@arangodb').db;
const arango = require('internal').arango;
let connectionHandle = arango.getConnectionHandle();

const namePrefix = helper.namePrefix;
const dbName = helper.dbName;
const testViewName = `${namePrefix}ViewNew`;
const testViewRename = `${namePrefix}ViewRename`;
const testCol1Name = `${namePrefix}Col1New`;
const testCol2Name = `${namePrefix}Col2New`;
const indexName = `${namePrefix}Inverted`;

const userSet = helper.userSet;

// names will have the from : UnitTest_<systemLevel>_<dbLevel>_<colLevel>
let name = "";

// systemLevel, dbLevel and collLevel will have each 3 attributes "rw", "ro", "none"
// each of the attributes will give access to a set containing the test names.

const rightLevels = helper.rightLevels;
let systemLevel = helper.systemLevel;
let dbLevel = helper.dbLevel;
let colLevel = helper.colLevel;
for (let l of rightLevels) {
  systemLevel[l] = new Set();
  dbLevel[l] = new Set();
  colLevel[l] = new Set();
}

// helper functions ///////////////////////////////////////////////////////////

const assertFail = (text) => { assertTrue(false, text); };

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
    let c = db._create(colName);
    if (colName === testCol1Name) {
      c.ensureIndex({ type: "inverted", name: indexName, fields: [ { name: "value" } ] });
    }

    if (colLevel['none'].has(name)) {
      users.grantCollection(name, dbName, colName, 'none');
    } else if (colLevel['ro'].has(name)) {
      users.grantCollection(name, dbName, colName, 'ro');
    } else if (colLevel['rw'].has(name)) {
      users.grantCollection(name, dbName, colName, 'rw');
    }
  }
  helper.switchUser(name, dbName);
};

function rootPrepareCollection (colName, numDocs = 1, defKey = true) {
  if (rootTestCollection(colName, false)) {
    db._collection(colName).truncate({ compact: false });
    let docs = [];
    for (let i = 0; i < numDocs; ++i) {
      let doc = {prop1: colName + "_1", propI: i, plink: "lnk" + i};
      if (!defKey) {
        doc._key = colName + i;
      }
      docs.push(doc);
    }
    db._collection(colName).insert(docs);
  }
  helper.switchUser(name, dbName);
};

function rootGrantCollection (colName, user, explicitRight = '') {
  if (rootTestCollection(colName, false)) {
    if (explicitRight !== '' && rightLevels.includes(explicitRight)) {
      users.grantCollection(user, dbName, colName, explicitRight);
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

function rootCreateView (viewName, viewType, properties = null) {
  if (rootTestView(viewName, false)) {
    try {
      db._dropView(viewName);
    } catch (ignored) {}
  }
  let view =  db._createView(viewName, viewType, {});
  if (properties != null) {
    view.properties(properties, false);
  }
  helper.switchUser(name, dbName);
};

function rootGetDefaultViewProps (viewType) {
  helper.switchUser('root', dbName);
  let properties;
  try {
    let tmpView = db._createView(this.title + "_tmpView", viewType, {});
    properties = tmpView.properties();
    db._dropView(this.title + "_tmpView");
  } catch (ignored) { }
  helper.switchUser(name, dbName);
  return properties;
};

function rootDropView (viewName) {
  if (rootTestView(viewName, false)) {
    try {
      db._dropView(viewName);
    } catch (ignored) {}
  }
  helper.switchUser(name, dbName);
};

function checkError (e) {
  assertEqual(e.code, 403,
              "Expected to get forbidden REST error code, but got another one");
  assertEqual(e.errorNum, errors.ERROR_FORBIDDEN.code,
              "Expected to get forbidden error number, but got another one");
};

// start of tests /////////////////////////////////////////////////////////////

function UserRightsManagement(name) {
  return {

    setUpAll: function() {
      rootCreateCollection(testCol1Name);
      rootCreateCollection(testCol2Name);
      rootPrepareCollection(testCol1Name);
      rootPrepareCollection(testCol2Name);
    },

    tearDown: function() {
      rootDropView(testViewRename);
      rootDropView(testViewName);
    },

    tearDownAll: function () {
      rootDropCollection(testCol1Name);
      rootDropCollection(testCol2Name);
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

helper.switchUser('root', '_system');
helper.removeAllUsers();
helper.generateAllUsers();

// derive and run UserRightsManagement
jsunity.run(function(){
  let suite = {};
  deriveTestSuite(UserRightsManagement(name), suite, "_-_" + name);
  return suite;
});

for (let testViewType of ["arangosearch", "search-alias"]) {
  for (name of userSet) {
    try {
      helper.switchUser(name, dbName);
    } catch (e) {
      continue;
    }
  
    const key = `${testViewType}_${name}`;

    let suite = {
      testUpdateViewByName: function() {
        // require('internal').sleep(2);

        assertTrue(rootTestView(testViewName),
          'Precondition failed, view was not found');

        if (dbLevel['rw'].has(name)) {
          try {
            db._view(testViewName).rename(testViewRename);
          } catch (e) {
            //FIXME: remove try/catch block after renaming will work in cluster
            if (e.code === 501 && e.errorNum === 1470) {
              return;
            } else if (e.code === 403) {
              return; // not authorised is valid if a non-read collection is present in the view
            } else {
              throw e;
            }
          }

          assertTrue(rootTestView(testViewRename),
            'View renaming reported success, but updated view was not found afterwards');

          analyzers.save("more_text_de", "text",
            { locale: "de.UTF-8", stopwords: [ ] },
            [ "frequency", "norm", "position" ]);

        } else {
          try {
            db._view(testViewName).rename(testViewRename);
          } catch (e) {
            checkError(e);
            return;
          }

          assertFalse(rootTestView(testViewRename),
            `${name} was able to rename a view with insufficent rights`);

        }
      },

      testViewByPropertiesRemoveFull: function() {
        assertTrue(rootTestView(testViewName),
          'Precondition failed, view was not found');

        if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
          db._view(testViewName).properties({}, false);
          assertTrue(isEqual(rootGetViewProps(testViewName, true), rootGetDefaultViewProps(testViewType)),
            'View properties update reported success, but properties were not updated');
        } else {
          try {
            db._view(testViewName).properties({}, false);
            assertFail(`${name} was able to update a view with insufficent rights`);
          } catch (e) {
            checkError(e);
          }
        }
      },

    };

    if (testViewType === 'arangosearch') {
      // specific handling for views of type "arangosearch"

      suite.setUp = function() {
        rootCreateView(testViewName, testViewType, { links: { [testCol1Name] : {includeAllFields: true } } });
        db._useDatabase(dbName);
        helper.switchUser('root', dbName);
      };

      suite.testAnalyzerPermisssions = function() {
        assertTrue(rootTestView(testViewName),
          'Precondition failed, view was not found');

        if (systemLevel['rw'].has(name) && dbLevel['rw'].has(name) && colLevel['rw'].has(name)) {
          // create additional analyzer
          let res = analyzers.save("more_text_de", "text",
            { locale: "de.UTF-8", stopwords: [ ] },
            [ "frequency", "norm", "position" ]);

        } else if( (systemLevel['ro'].has(name)) &&
          (dbLevel['ro'].has(name))     &&
          (colLevel['ro'].has(name))) {
          try {
            let res = analyzers.save("more_text_de", "text",
              { locale: "de.UTF-8", stopwords: [ ] },
              [ "frequency", "norm", "position" ]);

            assertFalse(true, `${name} was able to change analyzer although we had insufficent rights`);
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_FORBIDDEN.code,
              "Expected to get forbidden error number, but got another one");
            checkError(e);
          }
        } else if( systemLevel['ro'].has(name) && dbLevel['ro'].has(name) && colLevel['ro'].has(name)) {
          let res = arango.DELETE("/_db/" + db._name()+ "/_api/analyzer/text_de");
          assertEqual(403, res.code, `${name} was able to delete analyzer although we had insufficient rights`);
        }
      };
      
      suite.testUpdateViewByPropertyExceptPartialLinks = function() {
        assertTrue(rootTestView(testViewName),
          'Precondition failed, view was not found');

        if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
          db._view(testViewName).properties({ "cleanupIntervalStep": 1 }, true);
          assertEqual(rootGetViewProps(testViewName, true)["cleanupIntervalStep"],
            1, 'View property update reported success, but property was not updated');
        } else {
          try {
            db._view(testViewName).properties({ "cleanupIntervalStep": 1 }, true);
            assertFail(`${name} was able to update a view with insufficent rights`);
          } catch (e) {
            checkError(e);
          }
        }
      };

      suite.testViewByPropertyExceptLinksFull = function() {
        assertTrue(rootTestView(testViewName),
          'Precondition failed, view was not found');

        if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
          db._view(testViewName).properties({ "cleanupIntervalStep": 1 }, false);
          assertEqual(rootGetViewProps(testViewName, true)["cleanupIntervalStep"],
            1, 'View property update reported success, but property was not updated');
        } else {
          try {
            db._view(testViewName).properties({ "cleanupIntervalStep": 1 }, false);
            assertFail(`${name} was able to update a view with insufficent rights`);
          } catch (e) {
            checkError(e);
          }
        }
      };

      suite.testViewByExistingLinkUpdatePartial = function() {
        assertTrue(rootTestView(testViewName), 'Precondition failed, view was not found');
        if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
          db._view(testViewName).properties({
            links: {
              [testCol1Name]: {
                includeAllFields: true,
                analyzers: ["text_de","text_en"]
              }
            }
          }, true);

          assertEqual(rootGetViewProps(testViewName, true)["links"][testCol1Name]["analyzers"],
            ["text_de", "text_en"],
            'View link update reported success, but property was not updated');
        } else {
          try {
            db._view(testViewName).properties({
              links: {
                [testCol1Name] : {
                  includeAllFields: true,
                  analyzers: ["text_de","text_en"] 
                }
              }
            }, true);

            assertFail(`${name} was able to update a view with insufficent rights`);
          } catch (e) {
            checkError(e);
          }
        }
      };

      suite.testViewByExistingLinkUpdateFull = function() {
        assertTrue(rootTestView(testViewName),
          'Precondition failed, view was not found');

        if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
          db._view(testViewName).properties({
            links: {
              [testCol1Name]: {
                includeAllFields: true,
                analyzers: ["text_de","text_en"]
              }
            }
          }, false);

          assertEqual(rootGetViewProps(testViewName, true)["links"][testCol1Name]["analyzers"],
            ["text_de", "text_en"],
            'View link update reported success, but property was not updated');
        } else {
          try {
            db._view(testViewName).properties({
              links: {
                [testCol1Name]: {
                  includeAllFields: true,
                  analyzers: ["text_de","text_en"]
                }
              }
            }, false);
            assertFail(`${name} was able to update a view with insufficent rights`);
          } catch (e) {
            checkError(e);
          }
        }
      };

      suite.testViewByNewLinkToRWCollectionPartial = function() {
        if (colLevel['ro'].has(name) && colLevel['none'].has(name)) {
          assertTrue(rootTestView(testViewName), 'Precondition failed, view was not found');
          rootGrantCollection(testCol2Name, name, 'rw');

          if (dbLevel['rw'].has(name) && colLevel['ro'].has(name)) {
            db._view(testViewName).properties({
              links: {
                [testCol2Name]: {
                  includeAllFields: true,
                  analyzers: ["text_de"]
                }
              }
            }, true);

            assertEqual(rootGetViewProps(testViewName, true)["links"][testCol2Name]["analyzers"],
              [db._name() + "::text_de"],
              'View link update reported success, but property was not updated');
          } else {
            try {
              db._view(testViewName).properties({
                links: {
                  [testCol2Name]: {
                    includeAllFields: true,
                    analyzers: ["text_de"]
                  }
                }
              }, true);

              assertFail(`${name} was able to update a view with insufficent rights`);
            } catch (e) {
              checkError(e);
              return;
            }
          }
        }
      };

    } else {
      // specific handling for views of type "search-alias"
      suite.setUp = function() {
        rootCreateView(testViewName, testViewType, { indexes: [ { collection: testCol1Name, index: indexName } ] });
        db._useDatabase(dbName);
        helper.switchUser('root', dbName);
      };
    }

    jsunity.run(function() {
      return deriveTestSuiteWithnamespace(
        UserRightsManagement(name),
        suite,
        "_-_" + key
      );
    });
  }

}

let ret = jsunity.done();
arango.connectHandle(connectionHandle);
db._useDatabase('_system');
return ret;
