/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief administration actions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler
// / @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var console = require('console');

var actions = require('@arangodb/actions');
var arangodb = require('@arangodb');

// var queue = Foxx.queues.create("internal-demo-queue")

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_time
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/time',
  prefix: false,

  callback: function (req, res) {
    actions.resultOk(req, res, actions.HTTP_OK, { time: internal.time() });
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_sleep
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/sleep',
  prefix: false,

  callback: function (req, res) {
    var time = parseFloat(req.parameters.duration);
    if (isNaN(time)) {
      time = 3.0;
    }
    internal.wait(time);
    actions.resultOk(req, res, actions.HTTP_OK, { duration: time });
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_echo
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/echo',
  prefix: true,

  callback: function (req, res) {
    res.responseCode = actions.HTTP_OK;
    res.contentType = 'application/json; charset=utf-8';
    req.rawRequestBody = require('internal').rawRequestBody(req);
    res.body = JSON.stringify(req);
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_long_echo
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/long_echo',
  prefix: true,

  callback: function (req, res) {
    require('console').log('long polling request from client %s', req.client.id);

    res.responseCode = actions.HTTP_OK;
    res.contentType = 'application/json; charset=utf-8';
    res.headers = { 'transfer-encoding': 'chunked' };

    res.body = JSON.stringify(req);
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_statistics
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/statistics',
  prefix: false,

  callback: function (req, res) {
    var result;

    try {
      result = {};
      result.time = internal.time();
      result.enabled = internal.enabledStatistics();
      result.system = internal.processStatistics();
      result.client = internal.clientStatistics();
      result.http = internal.httpStatistics();
      result.server = internal.serverStatistics();

      actions.resultOk(req, res, actions.HTTP_OK, result);
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_statistics_description
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/statistics-description',
  prefix: false,

  callback: function (req, res) {
    var result;

    try {
      result = {
        groups: [
          {
            group: 'system',
            name: 'Process Statistics',
            description: 'Statistics about the ArangoDB process'
          },

          {
            group: 'client',
            name: 'Client Connection Statistics',
            description: 'Statistics about the connections.'
          },

          {
            group: 'http',
            name: 'HTTP Request Statistics',
            description: 'Statistics about the HTTP requests.'
          },

          {
            group: 'server',
            name: 'Server Statistics',
            description: 'Statistics about the ArangoDB server'
          }

        ],

        figures: [

          // .............................................................................
          // system statistics
          // .............................................................................

          {
            group: 'system',
            identifier: 'userTime',
            name: 'User Time',
            description: 'Amount of time that this process has been scheduled in user mode, ' +
              'measured in seconds.',
            type: 'accumulated',
            units: 'seconds'
          },

          {
            group: 'system',
            identifier: 'systemTime',
            name: 'System Time',
            description: 'Amount of time that this process has been scheduled in kernel mode, ' +
              'measured in seconds.',
            type: 'accumulated',
            units: 'seconds'
          },

          {
            group: 'system',
            identifier: 'numberOfThreads',
            name: 'Number of Threads',
            description: 'Number of threads in the arangod process.',
            type: 'current',
            units: 'number'
          },

          {
            group: 'system',
            identifier: 'residentSize',
            name: 'Resident Set Size',
            description: 'The total size of the number of pages the process has in real memory. ' +
              'This is just the pages which count toward text, data, or stack space. ' +
              'This does not include pages which have not been demand-loaded in, ' +
              'or which are swapped out. The resident set size is reported in bytes.',
            type: 'current',
            units: 'bytes'
          },

          {
            group: 'system',
            identifier: 'residentSizePercent',
            name: 'Resident Set Size',
            description: 'The percentage of physical memory used by the process as resident ' +
              'set size.',
            type: 'current',
            units: 'percent'
          },

          {
            group: 'system',
            identifier: 'virtualSize',
            name: 'Virtual Memory Size',
            description: 'On Windows, this figure contains the total amount of memory that the ' +
              'memory manager has committed for the arangod process. On other ' +
              'systems, this figure contains The size of the virtual memory the ' +
              'process is using.',
            type: 'current',
            units: 'bytes'
          },

          {
            group: 'system',
            identifier: 'minorPageFaults',
            name: 'Minor Page Faults',
            description: 'The number of minor faults the process has made which have ' +
              'not required loading a memory page from disk. This figure is ' +
              'not reported on Windows.',
            type: 'accumulated',
            units: 'number'
          },

          {
            group: 'system',
            identifier: 'majorPageFaults',
            name: 'Major Page Faults',
            description: 'On Windows, this figure contains the total number of page faults. ' +
              'On other system, this figure contains the number of major faults the ' +
              'process has made which have required loading a memory page from disk.',
            type: 'accumulated',
            units: 'number'
          },

          // .............................................................................
          // client statistics
          // .............................................................................

          {
            group: 'client',
            identifier: 'httpConnections',
            name: 'Client Connections',
            description: 'The number of connections that are currently open.',
            type: 'current',
            units: 'number'
          },

          {
            group: 'client',
            identifier: 'totalTime',
            name: 'Total Time',
            description: 'Total time needed to answer a request.',
            type: 'distribution',
            cuts: internal.requestTimeDistribution,
            units: 'seconds'
          },

          {
            group: 'client',
            identifier: 'requestTime',
            name: 'Request Time',
            description: 'Request time needed to answer a request.',
            type: 'distribution',
            cuts: internal.requestTimeDistribution,
            units: 'seconds'
          },

          {
            group: 'client',
            identifier: 'queueTime',
            name: 'Queue Time',
            description: 'Queue time needed to answer a request.',
            type: 'distribution',
            cuts: internal.requestTimeDistribution,
            units: 'seconds'
          },

          {
            group: 'client',
            identifier: 'bytesSent',
            name: 'Bytes Sent',
            description: 'Bytes sents for a request.',
            type: 'distribution',
            cuts: internal.bytesSentDistribution,
            units: 'bytes'
          },

          {
            group: 'client',
            identifier: 'bytesReceived',
            name: 'Bytes Received',
            description: 'Bytes receiveds for a request.',
            type: 'distribution',
            cuts: internal.bytesReceivedDistribution,
            units: 'bytes'
          },

          {
            group: 'client',
            identifier: 'connectionTime',
            name: 'Connection Time',
            description: 'Total connection time of a client.',
            type: 'distribution',
            cuts: internal.connectionTimeDistribution,
            units: 'seconds'
          },

          {
            group: 'http',
            identifier: 'requestsTotal',
            name: 'Total requests',
            description: 'Total number of HTTP requests.',
            type: 'accumulated',
            units: 'number'
          },

          {
            group: 'http',
            identifier: 'requestsAsync',
            name: 'Async requests',
            description: 'Number of asynchronously executed HTTP requests.',
            type: 'accumulated',
            units: 'number'
          },

          {
            group: 'http',
            identifier: 'requestsGet',
            name: 'HTTP GET requests',
            description: 'Number of HTTP GET requests.',
            type: 'accumulated',
            units: 'number'
          },

          {
            group: 'http',
            identifier: 'requestsHead',
            name: 'HTTP HEAD requests',
            description: 'Number of HTTP HEAD requests.',
            type: 'accumulated',
            units: 'number'
          },

          {
            group: 'http',
            identifier: 'requestsPost',
            name: 'HTTP POST requests',
            description: 'Number of HTTP POST requests.',
            type: 'accumulated',
            units: 'number'
          },

          {
            group: 'http',
            identifier: 'requestsPut',
            name: 'HTTP PUT requests',
            description: 'Number of HTTP PUT requests.',
            type: 'accumulated',
            units: 'number'
          },

          {
            group: 'http',
            identifier: 'requestsPatch',
            name: 'HTTP PATCH requests',
            description: 'Number of HTTP PATCH requests.',
            type: 'accumulated',
            units: 'number'
          },

          {
            group: 'http',
            identifier: 'requestsDelete',
            name: 'HTTP DELETE requests',
            description: 'Number of HTTP DELETE requests.',
            type: 'accumulated',
            units: 'number'
          },

          {
            group: 'http',
            identifier: 'requestsOptions',
            name: 'HTTP OPTIONS requests',
            description: 'Number of HTTP OPTIONS requests.',
            type: 'accumulated',
            units: 'number'
          },

          {
            group: 'http',
            identifier: 'requestsOther',
            name: 'other HTTP requests',
            description: 'Number of other HTTP requests.',
            type: 'accumulated',
            units: 'number'
          },

          // .............................................................................
          // server statistics
          // .............................................................................

          {
            group: 'server',
            identifier: 'uptime',
            name: 'Server Uptime',
            description: 'Number of seconds elapsed since server start.',
            type: 'current',
            units: 'seconds'
          },

          {
            group: 'server',
            identifier: 'physicalMemory',
            name: 'Physical Memory',
            description: 'Physical memory in bytes.',
            type: 'current',
            units: 'bytes'
          }

        ]
      };

      actions.resultOk(req, res, actions.HTTP_OK, result);
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_admin_test
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/test',
  prefix: false,

  callback: function (req, res) {
    var body = actions.getJsonBody(req, res);

    if (body === undefined) {
      return;
    }

    var tests = body.tests;
    if (!Array.isArray(tests)) {
      actions.resultError(req, res,
        actions.HTTP_BAD, arangodb.ERROR_HTTP_BAD_PARAMETER,
        "expected attribute 'tests' is missing");
      return;
    }

    var jsUnity = require('jsunity');
    var testResults = { passed: { }, error: false };

    tests.forEach(function (test) {
      var result = false;
      try {
        result = jsUnity.runTest(test);
      } catch (err) {}
      testResults.passed[test] = result;
      if (!result) {
        testResults.error = true;
      }
    });

    actions.resultOk(req, res, actions.HTTP_OK, testResults);
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_admin_execute
// //////////////////////////////////////////////////////////////////////////////

if (global.ALLOW_ADMIN_EXECUTE) {
  actions.defineHttp({
    url: '_admin/execute',
    prefix: false,

    callback: function (req, res) {
      /* jshint evil: true */
      var body = req.requestBody;
      var result;

      console.warn("about to execute: '%s'", body);

      if (body !== '') {
        result = eval('(function() {' + body + '}());');
      }

      if (req.parameters.hasOwnProperty('returnAsJSON') &&
        req.parameters.returnAsJSON === 'true') {
        actions.resultOk(req, res, actions.HTTP_OK, result);
      } else {
        actions.resultOk(req, res, actions.HTTP_OK, JSON.stringify(result));
      }
    }
  });
}

delete global.ALLOW_ADMIN_EXECUTE;
