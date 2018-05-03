/* global require, arango, print, db */
'use strict';

// /////////////////////////////////////////////////////////////////////////////
// @brief ArangoDB Doctor
// 
// @file
// 
// DISCLAIMER
// 
// Copyright 2018 ArangoDB GmbH, Cologne, Germany
// 
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// Copyright holder is ArangoDB GmbH, Cologne, Germany
// 
// @author Dr. Frank Celler
// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
// /////////////////////////////////////////////////////////////////////////////

var request = require("@arangodb/request");
const _ = require("lodash");

const { exec } = require('child_process');

const servers = {};
let possibleAgent = null;

var healthRecord = {};

function INFO() {
  let args = Array.prototype.slice.call(arguments);
  print.apply(print, ["INFO"].concat(args));
}

function WARN() {
  let args = Array.prototype.slice.call(arguments);
  print.apply(print, ["WARN"].concat(args));
}

function loadAgencyConfig() {
  var configuration = arango.GET("/_api/agency/config");
  return configuration;
}


/**
 * @brief Sort shard keys according to their numbers omitting startign 's' 
 * 
 * @param keys      Keys
 */
function sortShardKeys(keys) {
  var ret = [];
  // Get rid of 's' up front
  keys.forEach(function(key, index, theArray) {
    theArray[index] = key.substring(1);
  });
  // Sort keeping indices
  var ind = range(0,keys.length-1);
  ind.sort(function compare(i, j) {
    return parseInt(keys[i]) > parseInt(keys[j]);
  });
  ind.forEach(function (i) {
    ret.push('s'+keys[i]);
  });
  return ret;
}


function agencyDoctor(obj) {

  var nerrors = 0;
  var nwarnings = 0;
  var ndatabases = 0;
  var ncollections = 0;
  var nshards = 0;
  var ncolinplannotincurrent = 0;

  INFO('Analysing agency dump ...');
  
  // Must be array with length 1
  if (obj.length != 1) {
    ERROR('agency dump must be an array with a single object inside');
    process.exit(1);
  }

  // Check for /arango and assign
  if (!obj[0].hasOwnProperty('arango')) {
    ERROR('agency must have attribute "arango" as root');
    process.exit(1);
  }
  const agency = obj[0]['arango'];

  // Must have Plan, Current, Supervision, Target, Sync
  if (!agency.hasOwnProperty('Plan')) {
    ERROR('agency must have attribute "arango/Plan" object');
    process.exit(1);  
  }
  const plan = agency.Plan;
  if (!plan.hasOwnProperty('Version')) {
    ERROR('plan has no version');
    process.exit(1);    
  }
  if (!agency.hasOwnProperty('Current')) {
    ERROR('agency must have attribute "arango/Current" object');
    process.exit(1);  
  }
  const current = agency.Current;

  // Start sanity of plan
  INFO('Plan (version ' + plan.Version+ ')');

  // Planned databases check if also in collections and current
  INFO("  Databases")
  if (!plan.hasOwnProperty('Databases')) {
    ERROR('no databases in plan');
    process.exit(1);  
  }
  Object.keys(plan.Databases).forEach(function(database) {
    INFO('    ' + database);
    if (!plan.Collections.hasOwnProperty(database)) {
      WARN('found planned database "' + database + '" without planned collectinos');
    }
    if (!current.Databases.hasOwnProperty(database)) {
      WARN('found planned database "' + database + '" missing in "Current"');
    }
  });

  INFO("  Collections")
  // Planned collections
  if (!plan.hasOwnProperty('Collections')) {
    ERROR('no collections in plan');
    process.exit(1);  
  }

  var warned = false;
  Object.keys(plan.Collections).forEach(function(database) {
    ++ndatabases;
    INFO('    ' + database);
    if (!plan.Databases.hasOwnProperty(database)) {
      ERROR('found planned collections in unplanned database ' + database);
    }

    Object.keys(plan.Collections[database]).forEach(function(collection) {
      ++ncollections;
      const col = plan.Collections[database][collection];
      INFO('      ' + col.name);
      const distributeShardsLike = col.distributeShardsLike;
      var myShardKeys = sortShardKeys(Object.keys(col.shards));

      if (distributeShardsLike && distributeShardsLike != "") {      
        const prototype = plan.Collections[database][distributeShardsLike];
        if (prototype.replicationFactor != col.replicationFactor) {
          ERROR('distributeShardsLike: replicationFactor mismatch');
        }
        if (prototype.numberOfShards != col.numberOfShards) {
          ERROR('distributeShardsLike: numberOfShards mismatch');
        }      
        var prototypeShardKeys = sortShardKeys(Object.keys(prototype.shards));
        var ncshards = myShardKeys.length;
        if (prototypeShardKeys.length != ncshards) {
          ERROR('distributeShardsLike: shard map mismatch');
        }
        for (var i = 0; i < ncshards; ++i) {
          const shname = myShardKeys[i];
          const ccul = current.Collections[database];     
          if (JSON.stringify(col.shards[shname]) !=
              JSON.stringify(prototype.shards[prototypeShardKeys[i]])) {
            ERROR(
              'distributeShardsLike: planned shard map mismatch between "/arango/Plan/Collections/' +
                database + '/' + collection + '/shards/' + myShardKeys[i] +
                '" and " /arango/Plan/Collections/'  + database + '/' + distributeShardsLike +
                '/shards/' + prototypeShardKeys[i] + '"');
            INFO('      ' + JSON.stringify(col.shards[shname]));
            INFO('      ' + JSON.stringify(prototype.shards[prototypeShardKeys[i]]));
          }
        }
      }

      myShardKeys.forEach(function(shname) {
        ++nshards;
        if (current.Collections.hasOwnProperty(database)) {
          if (!current.Collections[database].hasOwnProperty(collection)) {
            WARN('missing collection "' + collection + '" in current');
          } else {
            if (!current.Collections[database][collection].hasOwnProperty(shname)) {
              WARN('missing shard "' + shname + '" in current');           
            }
            const shard = current.Collections[database][collection][shname];
            if (shard) {
              if (JSON.stringify(shard.servers) != JSON.stringify(col.shards[shname])) {
                if (shard.servers[0] != col.shards[shname][0]) {
                  ERROR('/arango/Plan/Collections/' + database + '/' + collection + '/shards/'
                        + shname + ' and /arango/Current/Collections/' + database + '/'
                        + collection + '/' + shname + '/servers do not match');
                } else {
                  var sortedPlan = (shard.servers).sort();
                  var sortedCurrent = (col.shards[shname]).sort();
                  if (JSON.stringify(sortedPlan) == JSON.stringify(sortedCurrent)) {
                    WARN('/arango/Plan/Collections/' + database + '/' + collection + '/shards/'
                         + shname + ' and /arango/Current/Collections/' + database + '/'
                         + collection + '/' + shname + '/servers follower do not match in order');
                  }
                }
              }
            }
          }
        } else {
          if (!warned) {
            WARN('planned database "' + database + '" missing entirely in current');
            warned = true;
          }
        }
      });

    });
  });

  INFO('Server health')
  INFO('  DB Servers')
  const supervision = agency.Supervision;
  const target = agency.Target;
  var servers = plan.DBServers;
  Object.keys(servers).forEach(function(serverId) {
    if (!target.MapUniqueToShortID.hasOwnProperty(serverId)) {
      WARN('incomplete planned db server ' + serverId + ' is missing in "Target"');
    } else { 
      INFO('    ' + serverId + '(' + target.MapUniqueToShortID[serverId].ShortName + ')');
      if (!supervision.Health.hasOwnProperty(serverId)) {
        ERROR('planned db server ' + serverId + ' missing in supervision\'s health records.');
      }
      servers[serverId] = supervision.Health[serverId];
      if (servers[serverId].Status == "BAD") {
        WARN('bad db server ' + serverId + '(' + servers[serverId].ShortName+ ')');
      } else if (servers[serverId].Status == "FAILED") {
        ERROR('failed db server ' + serverId + '(' + servers[serverId].ShortName+ ')');
      }
    }
  });

  INFO('  Coordinators')
  var servers = plan.Coordinators;
  Object.keys(servers).forEach(function(serverId) {
    if (!target.MapUniqueToShortID.hasOwnProperty(serverId)) {
      WARN('incomplete planned db server ' + serverId + ' is missing in "Target"');
    } else { 
      INFO('    ' + serverId + '(' + target.MapUniqueToShortID[serverId].ShortName + ')');
      if (!supervision.Health.hasOwnProperty(serverId)) {
        ERROR('planned db server ' + serverId + ' missing in supervision\'s health records.');
      }
      servers[serverId] = supervision.Health[serverId];
      if (servers[serverId].Status != "GOOD") {
        WARN('unhealthy db server ' + serverId + '(' + servers[serverId].ShortName+ ')');
      }
    }
  });

  const jobs = {
    ToDo: target.ToDo,
    Pending: target.Pending,
    Finished: target.Finished,
    Failed: target.Failed
  }
  var njobs = [];
  var nall;
  Object.keys(jobs).forEach(function (state) {
    njobs.push(Object.keys(jobs[state]).length);
  });

  var i = 0;
  INFO('Supervision activity');
  INFO('  Jobs: ' + nall+ '(' +
       'To do: ' + njobs[i++] + ', ' +
       'Pending: ' + njobs[i++] + ', ' +
       'Finished: ' + njobs[i++] + ', ' +
       'Failed: ' + njobs[i] + ')'
      );
  
  INFO('Summary');
  if (nerrors > 0) {
    ERROR('  ' + nerrors + ' errors');
  }
  if (nwarnings > 0) {
    WARN('  ' + nwarnings + ' warnings');
  }
  INFO('  ' + ndatabases + ' databases');
  INFO('  ' + ncollections + ' collections ');
  INFO('  ' + nshards + ' shards ');
  INFO('... agency analysis finished.')
  
}


/**
 * @brief Create an integer range as [start,end]
 * 
 * @param start     Start of range 
 * @param end       End of range
 */
function range(start, end) {
  return Array(end - start + 1).fill().map((_, idx) => start + idx);
}

function loadAgency(conn, seen) {

  var agencyDump = conn.POST("/_api/agency/read", '[["/"]]');
  seen[conn.getEndpoint()] = true;

  if (agencyDump.code === 404) {
    WARN("not talking to an agent, got: " + JSON.stringify(agencyDump));
    return null;
  }

  if (agencyDump.code === 307) {
    var red = conn.POST_RAW("/_api/agency/read", '[["/"]]');

    if (red.code === 307) {
      INFO("got redirect to " + red.headers.location);

      let leader = red.headers.location;
      let reg = /^(http|https):\/\/(.*)\/_api\/agency\/read$/;
      let m = reg.exec(leader);

      if (m === null) {
        WARN("unknown redirect " + leader);
        return null;
      } else {
        if (m[1] === "http") {
          leader = "tcp://" + m[2];
        }
        else if (m[1] === "https") {
          leader = "ssl://" + m[2];
        }

        if (leader in seen) {
          WARN("cannot find leader, tried: " + Object.keys(seen).join(", "));
          return null;
        }

        INFO("switching to " + leader);

        conn.reconnect(leader, "_system");
        return loadAgencyConfig(arango, seen);
      }
    }
  }

  if (agencyDump.code !== undefined) {
    WARN("failed to load agency, got: " + JSON.stringify(agencyDump));
    return null;
  }

  return agencyDump;
  
}

function defineServer(type, id, source) {
  if (id in servers) {
    _.merge(servers[id].source, source);
  } else {
    servers[id] = {
      type: type,
      id: id,
      source: source
    };
  }
}

function defineServerEndpoint(id, endpoint) {
  const server = servers[id];

  if ('endpoint' in server) {
    if (server.endpoint !== endpoint) {
      INFO("changing endpoint for " + id + " from "
           + server.endpoint + " to " + endpoint);
    }
  }

  server.endpoint = endpoint;
}

function defineServerStatus(id, status) {
  const server = servers[id];

  if ('status' in server) {
    if (server.status !== status) {
      INFO("changing status for " + id + " from "
           + server.status + " to " + status);
    }
  }

  server.status = status;
}

function defineAgentLeader(id, leading) {
  const server = servers[id];

  if ('leading' in server) {
    if (server.leading !== leading) {
      INFO("changing leading for " + id + " from "
           + server.leading + " to " + leading);
    }
  }

  server.leading = leading;
}

function defineAgentFromStatus(status, endpoint) {
  let id = status.agent.id;
  let leader = status.agent.leaderId;

  defineServer('AGENT', id, { status: endpoint });
  defineServerEndpoint(id, endpoint);

  arango.reconnect(endpoint, "_system");

  const cfg = db.configuration.toArray()[0].cfg;

  _.forEach(cfg.active, function(id) {
    defineServer('AGENT', id, { active: endpoint });
  });

  _.forEach(cfg.pool, function(loc, id) {
    defineServer('AGENT', id, { pool: endpoint });
    defineServerEndpoint(id, loc);
    defineAgentLeader(id, id === leader);
  });

  defineAgentLeader(id,status.agent.leading);
}

function definePrimaryFromStatus(status, endpoint) {
  let id = status.serverInfo.serverId;

  defineServer('PRIMARY', id, { status: endpoint });
  defineServerEndpoint(id, endpoint);

  let agentEndpoints = status.agency.agencyComm.endpoints;

  if (0 < agentEndpoints.length) {
    possibleAgent = agentEndpoints[0];
    return agentEndpoints[0];
  } else {
    console.error("Failed to find an agency endpoint");
    return "";
  }
}

function defineCoordinatorFromStatus(status, endpoint) {
  let id = status.serverInfo.serverId;

  defineServer('COORDINATOR', id, { status: endpoint });
  defineServerEndpoint(id, endpoint);

  let agentEndpoints = status.agency.agencyComm.endpoints;

  if (0 < agentEndpoints.length) {
    possibleAgent = agentEndpoints[0];
    return agentEndpoints[0];
  } else {
    console.error("Failed to find an agency endpoint");
    return "";
  }
}

function defineSingleFromStatus(status, endpoint) {
  defineServer('SINGLE', 'SINGLE', { status: endpoint });
  defineServerEndpoint('SINGLE', endpoint);
}

function serverBasics(conn) {
  if (!conn) {
    conn = arango;
  }

  const status = conn.GET("/_admin/status");
  const role = status.serverInfo.role;

  if (role === 'AGENT') {
    INFO("talking to an agent");
    defineAgentFromStatus(status, conn.getEndpoint());
  } else if (role === 'PRIMARY') {
    INFO("talking to a primary db server");
    definePrimaryFromStatus(status, conn.getEndpoint());
  } else if (role === 'COORDINATOR') {
    INFO("talking to a coordinator");
    defineCoordinatorFromStatus(status);
  } else if (role === 'SINGLE') {
    INFO("talking to a single server");
    defineSingleFromStatus(status);
  } else {
    INFO("talking to a unknown server, role: " + role);
    return "unknown";
  }

  return role;
}

function locateServers(plan) {
  let health = plan[0].arango.Supervision.Health;
  let cluster = plan[0].arango.Cluster;

  _.forEach(health, function(info, id) {
    const type = id.substr(0, 4);
    
    if (type === "PRMR") {
      defineServer('PRIMARY', id, { supervision: cluster });
      defineServerEndpoint(id, info.Endpoint);
      defineServerStatus(id, info.Status);
    } else if (type === "CRDN") {
      defineServer('COORDINATOR', id, { supervision: cluster });
      defineServerEndpoint(id, info.Endpoint);
      defineServerStatus(id, info.Status);
    }
  });
}

function listServers() {
  return servers;
}

function getServerData(arango) {
  var current = arango.getEndpoint();
  var servers = listServers();
  var report = {};
  Object.keys(servers).forEach(
    function (server) {
      arango.reconnect(servers[server].endpoint, '_system');
      report[server] = {
        version: arango.GET('_api/version'), log: arango.GET('_admin/log').text};
    });
  arango.reconnect(current, '_system');
  return report;
}

exports.loadAgencyConfig = loadAgencyConfig;
exports.serverBasics = serverBasics;

exports.loadAgency = loadAgency;
exports.locateServers = locateServers;
exports.listServers = listServers;

(function() {
  try {
    var type = serverBasics();

    if (type !== 'AGENT') {
      if (possibleAgent !== null) {
        arango.reconnect(possibleAgent, "_system");
        serverBasics();
      }
    }
    
    // Agency dump and analysis
    var agencyConfig = loadAgencyConfig();
    var agencyDump = loadAgency(arango, {});
    if (agencyDump !== null) {
      locateServers(agencyDump);
    }
    agencyDoctor(agencyDump);
    healthRecord['agency'] = agencyDump[0].arango;
    
    // Get logs from all servers
    healthRecord['servers'] = getServerData(arango);

    require('fs').writeFileSync('arango-doctor.json', JSON.stringify(healthRecord));
    
  } catch (e) {
    print(e);
  }
}());

exports.show = require("@arangodb/doctor/show");
