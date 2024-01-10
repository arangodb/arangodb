/* global ArangoServerState, ArangoClusterInfo, arango */
'use strict';

/// helpers for cluster tests. these rely on objects not necessarily
/// available to regular users

exports.supervisionState = function () {
  try {
    var result = arango.POST("/_admin/execute", `return global.ArangoAgency.get('Target')`);
    result = result.arango.Target;
    var proj = { ToDo: result.ToDo, Pending: result.Pending,
      Failed: result.Failed, Finished: result.Finished,
    error: false };
    return proj;
  } catch (err) {
    return { error: true, errorMsg: 'could not read /Target in agency',
    exception: err };
  }
};

exports.queryAgencyJob = function (id) {
  let job = null;
  let states = ["Finished", "Pending", "Failed", "ToDo"];
  for (let s of states) {
    try {
      let arg = 'Target/' + s + '/' + id;
      job = arango.POST("/_admin/execute", `return global.ArangoAgency.get(${JSON.stringify(arg)})`);
      job = job.arango.Target[s];
      if (Object.keys(job).length !== 0 && job.hasOwnProperty(id)) {
        return {error: false, id, status: s, job: job[id]};
      }
    } catch (err) {
    }
  }
  return {error: true, errorMsg: "Did not find job.", id, job: null};
};
