/* global require, arango */
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

const servers = {};

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

function loadAgency(conn) {

  
  var agency = arango.POST("/_api/agency/read", '[["/"]]');

  if (agency.code === 404) {
    WARN("not talking to an agent, got: " + JSON.stringify(agency));
    return {};
  }

  if (agency.code !== undefined) {
    WARN("failed to load agency, got: " + JSON.stringify(agency));
    return {};
  }

  return agency;
}

function defineAgent(status, endpoint) {
  let id = status.agent.id;

  if (id in servers) {
  } else {
    servers[id] = {
      type: 'AGENT',
      id: id,
      endpoint: endpoint,
      source: "status"
    };
  }

  if (status.agent.leading) {
    arango.reconnect(endpoint, "_system");

    const cfg = db.configuration.toArray()[0].cfg;

    print("active");
    cfg.active.map(a => print(a));

    print("pool");
    Object.keys(cfg.pool).forEach(a => print(a));
  } else {
  }
}

function serverBasics(conn) {
  if (!conn) {
    conn = arango;
  }

  const version = conn.GET("/_admin/status");
  const role = version.serverInfo.role;

  if (role === 'AGENT') {
    INFO("talking to an agent");
    defineAgent(version, conn.getEndpoint());
  } else if (role === 'PRIMARY') {
    INFO("talking to a primary db server");
    definePrimary(version);
  } else if (role === 'COORDINATOR') {
    INFO("talking to a coordinator");
    defineCoordinator(version);
  } else if (role === 'SINGLE') {
    INFO("talking to a single server");
    defineSingle(version);
  } else {
    INFO("talking to a unknown server, role: " + role);
  }
}

function listServers() {
  return servers;
}

exports.loadAgencyConfig = loadAgencyConfig;
exports.serverBasics = serverBasics;
exports.loadAgency = loadAgency;
exports.listServers = listServers;
