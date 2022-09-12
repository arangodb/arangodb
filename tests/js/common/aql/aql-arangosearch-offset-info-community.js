/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
///
/// The Programs (which include both the software and documentation) contain
/// proprietary information of ArangoDB GmbH; they are provided under a license
/// agreement containing restrictions on use and disclosure and are also
/// protected by copyright, patent and other intellectual and industrial
/// property laws. Reverse engineering, disassembly or decompilation of the
/// Programs, except to the extent required to obtain interoperability with
/// other independently created software or as specified by law, is prohibited.
///
/// It shall be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of
/// applications if the Programs are used for purposes such as nuclear,
/// aviation, mass transit, medical, or other inherently dangerous applications,
/// and ArangoDB GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of ArangoDB
/// GmbH. You shall not disclose such confidential and proprietary information
/// and shall use it only in accordance with the terms of the license agreement
/// you entered into with ArangoDB GmbH.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

let arangodb = require("@arangodb");
let analyzers = require("@arangodb/analyzers");
let db = arangodb.db;
let internal = require("internal");
let jsunity = require("jsunity");
let errors = arangodb.errors;
let dbName = "OffsetInfoAqlTestSuiteCommunity";
let isEnterprise = internal.isEnterprise();

function IResearchOffsetInfoAqlTestSuiteCommunity() {
  let cleanup = function() {
    db._useDatabase("_system");
    try { db._dropDatabase(dbName); } catch (err) {}
  };

  return {
    setUpAll : function () { 
      cleanup();

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      db._create("docs");
      db._createView("docsView", "arangosearch",
          { links: { docs: { includeAllFields : true } } });
    },
    tearDownAll : function () { 
      cleanup();
    },

    testSimpleOffset: function() {
      try {
        db._query("FOR d IN docsView SEARCH d._key == 'a' RETURN OFFSET_INFO(d, '_key')");
        assertTrue(false);
      } catch (e) {
        assertEqual(errors.ERROR_NOT_IMPLEMENTED.code, e.errorNum);
      }
    }
  }
}

if (!isEnterprise) {
  jsunity.run(IResearchOffsetInfoAqlTestSuiteCommunity);
}

return jsunity.done();
