/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// Copyright 202 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const base = require("fs").join(require('internal').pathForTesting('client'), 
    'shell', 'shell-improved-metrics-accounting.inc');

const arangosearch_base = require("fs").join(require('internal').pathForTesting('client'), 
    'shell', 'api', 'arangosearch-memory-metrics.inc');    

const ImprovedMemoryAccounting = require("internal").load(base);
const ImprovedMemoryAccountingArangoSearch = require("internal").load(arangosearch_base);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

if (internal.debugCanUseFailAt()) {
    const base_fail_at = require("fs").join(require('internal').pathForTesting('client'), 
    'shell', 'shell-improved-metrics-accounting-fail-at.inc');
    const ImprovedMemoryAccountingFailAt = require("internal").load(base_fail_at);

    jsunity.run(function ImprovedMemoryAccountingFailAtTestSuite_no_repl() {
        let suite = {
        };
      
        deriveTestSuite(
            ImprovedMemoryAccountingFailAt("ImprovedMemoryAccountingFailAtTestSuite_NoRepl",null, {}),
            suite,
            "_NoRepl"
        );
        return suite;
      });
}

jsunity.run(function ImprovedMemoryAccountingTestSuite_no_repl() {
    let suite = {
    };
  
    deriveTestSuite(
      ImprovedMemoryAccounting("ImprovedMemoryAccountingTestSuite_NoRepl", null, {}),
        suite,
        "_NoRepl"
    );
    return suite;
});

jsunity.run(function ImprovedMemoryAccountingArangoSearchTestSuite_no_repl() {
    let suite = {
    };
  
    deriveTestSuite(
      ImprovedMemoryAccountingArangoSearch("ImprovedMemoryAccountingArangoSearch_NoRepl", null, {}),
        suite,
        "_NoRepl"
    );
    return suite;
});

return jsunity.done();
