/* jshint strict: false, sub: true */
'use strict';

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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'fail'   : 'this job will always produce a failed result',
  'fail2'  : 'this job will always produce a failed result',
  'success': 'this job will always produce a sucessfull result'
};

const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');

const testPaths = {
  'fail': [],
  'fail2': [],
  'success': []
};

function fail (options) {
  return {
    failSuite: {
      status: false,
      total: 1,
      message: 'this suite will always fail.',
      duration: 2,
      failed: 1,
      failTest: {
        status: false,
        total: 1,
        duration: 1,
        message: 'this testcase will always fail.'
      },
      failSuccessTest: {
        status: true,
        duration: 1,
        message: 'this testcase will always succeed, though its in the fail testsuite.'
      }
    },
    successSuite: {
      status: true,
      total: 1,
      message: 'this suite will always be successfull',
      duration: 1,
      failed: 0,
      success: {
        status: true,
        message: 'this testcase will always be successfull',
        duration: 1
      }
    }
  };
}

function fail2 (options) {
  return {
    failSuite: {
      status: false,
      total: 1,
      message: 'this suite will always fail.',
      duration: 2,
      failed: 1,
      failTest: {
        status: false,
        total: 1,
        duration: 1,
        message: 'this testcase will always fail.'
      },
      failSuccessTest: {
        status: true,
        duration: 1,
        message: 'this testcase will always succeed, though its in the fail testsuite.'
      }
    },
    successSuite: {
      status: true,
      total: 1,
      message: 'this suite will always be successfull',
      duration: 1,
      failed: 0,
      success: {
        status: true,
        message: 'this testcase will always be successfull',
        duration: 1
      }
    }
  };
}

function success (options) {
  return {
    successSuite2: {
      status: true,
      total: 1,
      message: 'this suite will always succeed.',
      duration: 2,
      failed: 1,
      failTest: {
        status: true,
        total: 1,
        duration: 1,
        message: 'this testcase will always succeed.'
      },
      failSuccessTest: {
        status: true,
        duration: 1,
        message: 'this testcase will always succeed, since its in the success testsuite.'
      }
    },
    successSuite3: {
      status: true,
      total: 1,
      message: 'this suite will always be successfull',
      duration: 1,
      failed: 0,
      success: {
        status: true,
        message: 'this testcase will always be successfull',
        duration: 1
      }
    }
  };
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['fail'] = fail;
  testFns['fail2'] = fail2;
  testFns['success'] = success;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
