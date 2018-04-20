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

const servers = {};
let possibleAgent = null;

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

function loadAgency(conn, seen) {
  
  var agency = conn.POST("/_api/agency/read", '[["/"]]');
  seen[conn.getEndpoint()] = true;

  if (agency.code === 404) {
    WARN("not talking to an agent, got: " + JSON.stringify(agency));
    return null;
  }

  if (agency.code === 307) {
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
        return loadAgencyAgency(arango, seen);
      }
    }
  }

  if (agency.code !== undefined) {
    WARN("failed to load agency, got: " + JSON.stringify(agency));
    return null;
  }

  return agency;
}

function loadAgency() {
  return loadAgency(arango, {});
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
    console.error(possibleAgent);
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

    var plan = loadAgency();

    if (plan !== null) {
      locateServers(plan);
    }
  } catch (e) {
    print(e);
  }
}());

exports.show = require("@arangodb/doctor/show");












