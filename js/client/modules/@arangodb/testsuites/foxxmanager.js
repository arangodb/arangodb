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
  'foxx_manager': 'foxx manager tests'
};
const optionsDocumentation = [
  '   - `skipFoxxQueues`: omit the test for the foxx queues'
];

const pu = require('@arangodb/process-utils');
const fs = require('fs');

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: foxx manager
// //////////////////////////////////////////////////////////////////////////////

function foxxManager (options) {
  print('foxx_manager tests...');

  let instanceInfo = pu.startInstance('tcp', options, {}, 'foxx_manager');

  if (instanceInfo === false) {
    return {
      foxx_manager: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  let results = {};

  results.update = pu.run.arangoshCmd(options, instanceInfo, {
    'configuration': fs.join(pu.CONFIG_DIR, 'foxx-manager.conf')
  }, ['update']);

  if (results.update.status === true || options.force) {
    results.search = pu.run.arangoshCmd(options, instanceInfo, {
      'configuration': fs.join(pu.CONFIG_DIR, 'foxx-manager.conf')
    }, ['search', 'itzpapalotl']);
  }

  print('Shutting down...');
  pu.shutdownInstance(instanceInfo, options);
  print('done.');

  return results;
}

// TODO write test for 2.6-style queues
// testFuncs.queue_legacy = function (options) {
//   if (options.skipFoxxQueues) {
//     print('skipping test of legacy queue job types')
//     return {}
//   }
//   var startTime
//   var queueAppMountPath = '/test-queue-legacy'
//   print('Testing legacy queue job types')
//   var instanceInfo = pu.startInstance('tcp', options, [], 'queue_legacy')
//   if (instanceInfo === false) {
//     return {status: false, message: 'failed to start server!'}
//   }
//   var data = {
//     naive: {_key: 'potato', hello: 'world'},
//     forced: {_key: 'tomato', hello: 'world'},
//     plain: {_key: 'banana', hello: 'world'}
//   }
//   var results = {}
//   results.install = pu.run.arangoshCmd(options, instanceInfo, {
//     'configuration': 'etc/testing/foxx-manager.conf'
//   }, [
//     'install',
//     'js/common/test-data/apps/queue-legacy-test',
//     queueAppMountPath
//   ])

//   print('Restarting without foxx-queues-warmup-exports...')
//   pu.shutdownInstance(instanceInfo, options)
//   instanceInfo = pu.startInstance('tcp', options, {
//     'server.foxx-queues-warmup-exports': 'false'
//   }, 'queue_legacy', instanceInfo.flatTmpDataDir)
//   if (instanceInfo === false) {
//     return {status: false, message: 'failed to restart server!'}
//   }
//   print('done.')

//   var res, body
//   startTime = time()
//   try {
//     res = download(
//       instanceInfo.url + queueAppMountPath + '/',
//       JSON.stringify(data.naive),
//       {method: 'POST'}
//     )
//     body = JSON.parse(res.body)
//     results.naive = {status: body.success === false, message: JSON.stringify({body: res.body, code: res.code})}
//   } catch (e) {
//     results.naive = {status: true, message: JSON.stringify({body: res.body, code: res.code})}
//   }
//   results.naive.duration = time() - startTime

//   startTime = time()
//   try {
//     res = download(
//       instanceInfo.url + queueAppMountPath + '/?allowUnknown=true',
//       JSON.stringify(data.forced),
//       {method: 'POST'}
//     )
//     body = JSON.parse(res.body)
//     results.forced = (
//       body.success
//       ? {status: true}
//       : {status: false, message: body.error, stacktrace: body.stacktrace}
//      )
//   } catch (e) {
//     results.forced = {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//   }
//   results.forced.duration = time() - startTime

//   print('Restarting with foxx-queues-warmup-exports...')
//   pu.shutdownInstance(instanceInfo, options)
//   instanceInfo = pu.startInstance('tcp', options, {
//     'server.foxx-queues-warmup-exports': 'true'
//   }, 'queue_legacy', instanceInfo.flatTmpDataDir)
//   if (instanceInfo === false) {
//     return {status: false, message: 'failed to restart server!'}
//   }
//   print('done.')

//   startTime = time()
//   try {
//     res = download(instanceInfo.url + queueAppMountPath + '/', JSON.stringify(data.plain), {method: 'POST'})
//     body = JSON.parse(res.body)
//     results.plain = (
//       body.success
//       ? {status: true}
//       : {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//     )
//   } catch (e) {
//     results.plain = {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//   }
//   results.plain.duration = time() - startTime

//   startTime = time()
//   try {
//     for (var i = 0; i < 60; i++) {
//       wait(1)
//       res = download(instanceInfo.url + queueAppMountPath + '/')
//       body = JSON.parse(res.body)
//       if (body.length === 2) {
//         break
//       }
//     }
//     results.final = (
//       body.length === 2 && body[0]._key === data.forced._key && body[1]._key === data.plain._key
//       ? {status: true}
//       : {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//     )
//   } catch (e) {
//     results.final = {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//   }
//   results.final.duration = time() - startTime

//   results.uninstall = pu.run.arangoshCmd(options, instanceInfo, {
//     'configuration': 'etc/testing/foxx-manager.conf'
//   }, [
//     'uninstall',
//     queueAppMountPath
//   ])
//   print('Shutting down...')
//   pu.shutdownInstance(instanceInfo, options)
//   print('done.')
//   if ((!options.skipLogAnalysis) &&
//       instanceInfo.hasOwnProperty('importantLogLines') &&
//       Object.keys(instanceInfo.importantLogLines).length > 0) {
//     print('Found messages in the server logs: \n' + yaml.safeDump(instanceInfo.importantLogLines))
//   }
//   return results
// }

function setup (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['foxx_manager'] = foxxManager;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}

exports.setup = setup;
