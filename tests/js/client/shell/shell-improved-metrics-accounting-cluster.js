/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

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
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const base = require("fs").join(require('internal').pathForTesting('client'), 
    'shell', 'shell-improved-metrics-accounting.inc');

const ImprovedMemoryAccounting = require("internal").load(base);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////
if (internal.debugCanUseFailAt()) {
    jsunity.run(function ImprovedMemoryAccountingTestSuite_repl() {
        let suite = {
        };
      
        deriveTestSuite(
          ImprovedMemoryAccounting("InvertedIndexSearchAliasAqlTestSuite_Repl", null, {
            replicationFactor:3, 
            writeConcern:1, 
            numberOfShards : 3}, false),
            suite,
            "_Repl"
        );
        return suite;
    });
}

return jsunity.done();
