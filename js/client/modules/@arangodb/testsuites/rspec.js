/* jshint strict: false, sub: true */
/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2014 triagens GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'server_http': 'http server tests in Mocha',
  'http_replication': 'http replication tests',
  'http_server': 'http server tests in Ruby',
  'ssl_server': 'https server tests'
};
const optionsDocumentation = [
  '   - `skipSsl`: omit the ssl_server rspec tests.',
  '   - `rspec`: the location of rspec program',
  '   - `ruby`: the location of ruby program; if empty start rspec directly'
];

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
const yaml = require('js-yaml');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

function serverHttp (options) {
  // first starts to replace rspec:
  let testCases = tu.scanTestPath('js/common/tests/http');

  return tu.performTests(options, testCases, 'server_http', tu.runThere);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs ruby tests using RSPEC
// //////////////////////////////////////////////////////////////////////////////

function camelize (str) {
  return str.replace(/(?:^\w|[A-Z]|\b\w)/g, function (letter, index) {
    return index === 0 ? letter.toLowerCase() : letter.toUpperCase();
  }).replace(/\s+/g, '');
}

function rubyTests (options, ssl) {
  let instanceInfo;

  if (ssl) {
    instanceInfo = pu.startInstance('ssl', options, {}, 'ssl_server');
  } else {
    instanceInfo = pu.startInstance('tcp', options, {}, 'http_server');
  }

  if (instanceInfo === false) {
    return {
      status: false,
      message: 'failed to start server!'
    };
  }

  const tmpname = fs.getTempFile() + '.rb';

  let rspecConfig = 'RSpec.configure do |c|\n' +
                    '  c.add_setting :ARANGO_SERVER\n' +
                    '  c.ARANGO_SERVER = "' +
                    instanceInfo.endpoint.substr(6) + '"\n' +
                    '  c.add_setting :ARANGO_SSL\n' +
                    '  c.ARANGO_SSL = "' + (ssl ? '1' : '0') + '"\n' +
                    '  c.add_setting :ARANGO_USER\n' +
                    '  c.ARANGO_USER = "' + options.username + '"\n' +
                    '  c.add_setting :ARANGO_PASSWORD\n' +
                    '  c.ARANGO_PASSWORD = "' + options.password + '"\n' +
                    '  c.add_setting :SKIP_TIMECRITICAL\n' +
                    '  c.SKIP_TIMECRITICAL = ' + JSON.stringify(options.skipTimeCritical) + '\n' +
                    'end\n';

  fs.write(tmpname, rspecConfig);

  if (options.extremeVerbosity === true) {
    print('rspecConfig: \n' + rspecConfig);
  }

  try {
    fs.makeDirectory(pu.LOGS_DIR);
  } catch (err) {}

  let files = fs.list(fs.join('UnitTests', 'HttpInterface'));

  let continueTesting = true;
  let filtered = {};
  let result = {};

  let args;
  let command;
  let rspec;

  if (options.ruby === '') {
    command = options.rspec;
    rspec = undefined;
  } else {
    command = options.ruby;
    rspec = options.rspec;
  }

  const parseRspecJson = function (testCase, res, totalDuration) {
    let tName = camelize(testCase.description);
    let status = (testCase.status === 'passed');

    res[tName] = {
      status: status,
      message: testCase.full_description,
      duration: totalDuration // RSpec doesn't offer per testcase time...
    };

    res.total++;

    if (!status) {
      const msg = yaml.safeDump(testCase)
            .replace(/.*rspec\/core.*\n/gm, '')
            .replace(/.*rspec\\core.*\n/gm, '')
            .replace(/.*lib\/ruby.*\n/, '')
            .replace(/.*- >-.*\n/gm, '')
            .replace(/\n *`/gm, ' `');
      print('RSpec test case falied: \n' + msg);
      res[tName].message += '\n' + msg;
    }
    return status ? 0 : 1;
  };

  let count = 0;
  files = tu.splitBuckets(options, files);

  for (let i = 0; i < files.length; i++) {
    const te = files[i];

    if (te.substr(0, 4) === 'api-' && te.substr(-3) === '.rb') {
      let tfn = fs.join('UnitTests', 'HttpInterface', te);
      if (tu.filterTestcaseByOptions(tfn, options, filtered)) {
        count += 1;
        if (!continueTesting) {
          print('Skipping ' + te + ' server is gone.');

          result[te] = {
            status: false,
            message: instanceInfo.exitStatus
          };

          instanceInfo.exitStatus = 'server is gone.';
          break;
        }
        const subFolder = ssl ? 'ssl_server' : 'http_server';
        const resultfn = fs.join(options.testOutputDirectory, subFolder, te + '.json');

        args = ['--color',
                '-I', fs.join('UnitTests', 'HttpInterface'),
                '--format', 'd',
                '--format', 'j',
                '--out', resultfn,
                '--require', tmpname,
                tfn
               ];

        if (rspec !== undefined) {
          args = [rspec].concat(args);
        }

        print('\n' + Date() + ' rspec trying', tfn, '...');
        const res = pu.executeAndWait(command, args, options, 'arangosh', instanceInfo.rootDir);

        result[te] = {
          total: 0,
          failed: 0,
          status: res.status
        };

        try {
          const jsonResult = JSON.parse(fs.read(resultfn));

          if (options.extremeVerbosity) {
            print(yaml.safeDump(jsonResult));
          }

          for (let j = 0; j < jsonResult.examples.length; ++j) {
            result[te].failed += parseRspecJson(
              jsonResult.examples[j], result[te],
              jsonResult.summary.duration);
          }

          result[te].duration = jsonResult.summary.duration;
        } catch (x) {
          print('Failed to parse rspec result: ' + x);
          result[te]['complete_' + te] = res;

          if (res.status === false) {
            options.cleanup = false;

            if (!options.force) {
              break;
            }
          }
        }

        continueTesting = pu.arangod.check.instanceAlive(instanceInfo, options);
      } else {
        if (options.extremeVerbosity) {
          print('Skipped ' + te + ' because of ' + filtered.filter);
        }
      }
    }
  }

  print('Shutting down...');

  if (count === 0) {
    result['ALLTESTS'] = {
      status: false,
      skipped: true
    };
    result.status = false;
    print(RED + 'No testcase matched the filter.' + RESET);
  }

  fs.remove(tmpname);
  pu.shutdownInstance(instanceInfo, options);
  print('done.');

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: http_replication
// //////////////////////////////////////////////////////////////////////////////

function httpReplication (options) {
  var opts = {
    'replication': true
  };
  _.defaults(opts, options);

  return rubyTests(opts, false);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: http_server
// //////////////////////////////////////////////////////////////////////////////

function httpServer (options) {
  var opts = {
    'httpTrustedOrigin': 'http://was-erlauben-strunz.it'
  };
  _.defaults(opts, options);
  return rubyTests(opts, false);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: ssl_server
// //////////////////////////////////////////////////////////////////////////////

function sslServer (options) {
  if (options.skipSsl) {
    return {
      ssl_server: {
        status: true,
        skipped: true
      }
    };
  }
  var opts = {
    'httpTrustedOrigin': 'http://was-erlauben-strunz.it'
  };
  _.defaults(opts, options);
  return rubyTests(opts, true);
}

function setup (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['http_replication'] = httpReplication;
  testFns['http_server'] = httpServer;
  testFns['server_http'] = serverHttp;
  testFns['ssl_server'] = sslServer;

  defaultFns.push('server_http');
  defaultFns.push('http_server');
  defaultFns.push('ssl_server');

  opts['skipSsl'] = false;
  opts['rspec'] = 'rspec';
  opts['ruby'] = '';

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}

exports.setup = setup;
