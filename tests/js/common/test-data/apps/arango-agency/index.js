const createRouter = require('@arangodb/foxx/router');
const router = createRouter();
const db = require('internal').db;
const ERRORS = require('@arangodb').errors;

let agencyCall = function(f) {
  let result = false;
  try {
    f();
  } catch (err) {
    result = (err.errorNum === ERRORS.ERROR_FORBIDDEN.code);
  }
  return result;
};

let testCases = {};

// ArangoClusterInfo
testCases["AgencyClusterInfoUniqid"] = function() {
  let testee = global.ArangoClusterInfo;
  let result = false;
  try {
    testee.uniqid();
  } catch (err) {
    result = (err.errorNum === ERRORS.ERROR_FORBIDDEN.code);
  }
  return result;
};

// ArangoAgency
["agency", "read", "write", "transact", "transient", "cas", "get", "createDirectory",
 "increaseVersion", "remove", "endpoints", "set", "uniqid"].forEach((func) => {
  testCases["ArangoAgency" + func] = function() {
    let testee = global.ArangoAgency;
    return agencyCall(testee[func]);
  };
});
  
// ArangoAgent
["enabled", "leading", "read", "write", "state"].forEach((func) => {
  testCases["ArangoAgent" + func] = function() {
    let testee = global.ArangoAgent;
    return agencyCall(testee[func]);
  };
});

router.get('/runInsideFoxx', function (req, res) {
  let results = {};
  Object.keys(testCases).forEach((tc) => {
    results[tc] = testCases[tc]();
  });
  res.json({ results });
});

router.get('/runInsideFoxxTransaction', function (req, res) {
  let results = {};
  Object.keys(testCases).forEach((tc) => {
    results[tc] = db._executeTransaction({
      collections: {},
      action: function() { 
        return testCases[tc]();
      }
    });
  });
  res.json({ results });
});

module.context.use(router);
