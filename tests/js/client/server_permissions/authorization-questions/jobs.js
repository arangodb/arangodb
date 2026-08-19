/*jshint globalstrict:false, strict:false */
/* global getOptions, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

// Authorization questions asked by the async-job API (/_api/job, /_admin/job).
//
// Observation-based counterpart of tests/api/apitests/jobs.mjs.
//
// Handler: arangod/RestHandler/RestJobHandler.cpp
//
// RestJobHandler performs NO ExecContext checks of its own. The only
// authorization questions are the base `UseApiVersion version=0` and
// `UseDatabase name=_system level=read` for every request (the job
// endpoints carry no /_db prefix, so the connected database _system applies;
// we spell it out explicitly). The per-job ownership filtering inside
// AsyncJobManager uses exec.user()/exec.isSuperuser(), which do not call can()
// and therefore log nothing. Both mount points (/_api/job and /_admin/job)
// route to the same handler and behave identically.

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true',
    // keep background threads from asking questions of their own
    'foxx.queues': 'false'
  };
}

const jsunity = require('jsunity');
const {
  beginObserve,
  endObserve,
  disableObserve,
  assertPermissions
} = require('@arangodb/testutils/permissions-observer');

function jobApiAuthzSuite () {

  // create one async job (owned by the connected user) by firing an async
  // /_api/version request; returns the job id from the x-arango-async-id header
  function createJob () {
    const r = arango.GET_RAW(`/_db/_system/_api/version`,
                             { 'x-arango-async': 'store' });
    const jobId = r.headers['x-arango-async-id'];
    if (!jobId) {
      throw new Error(`x-arango-async-id header missing; status ${r.code}`);
    }
    return jobId;
  }
  function deleteJob (jobId) {
    arango.DELETE_RAW(`/_db/_system/_api/job/${jobId}`);
  }

  return {
    tearDown: function () {
      disableObserve();
    },

    // GET /_api/job/done - list done jobs for the caller
    testListDone: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/job/done`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_api/job/pending - list pending jobs for the caller
    testListPending: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/job/pending`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_api/job/{id} - job status by id
    testGetJobById: function () {
      const jobId = createJob();
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/job/${jobId}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
      deleteJob(jobId);
    },

    // PUT /_api/job/{id} - fetch (and consume) the job result
    testFetchJobResult: function () {
      const jobId = createJob();
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_api/job/${jobId}`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
      deleteJob(jobId);
    },

    // PUT /_api/job/{id}/cancel - cancel the job
    testCancelJob: function () {
      const jobId = createJob();
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_api/job/${jobId}/cancel`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
      deleteJob(jobId);
    },

    // DELETE /_api/job/all - delete all jobs owned by the caller
    testDeleteAll: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/job/all`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // DELETE /_api/job/expired?stamp=0 - delete expired jobs of the caller
    testDeleteExpired: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/job/expired?stamp=0`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // DELETE /_api/job/{id} - delete one specific job result
    testDeleteJobById: function () {
      const jobId = createJob();
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/job/${jobId}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
      deleteJob(jobId);
    },

    // GET /_admin/job/done - alternative mount point, identical semantics
    testAdminMountPoint: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/job/done`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },
  };
}

jsunity.run(jobApiAuthzSuite);
return jsunity.done();
