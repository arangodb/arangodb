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

let serverRole = 'UNKNOWN';
let endpoint2role = {};
let agencyPlan = undefined;

function loadAgencyPlan(conn) {
  var plan = arango.POST("/_api/agency/read", '[["/"]]');

  if (plan.code === 404) {
    print("ERROR not talking to an agent, got: " + JSON.stringify(plan));
    return {};
  }

  if (plan.code !== undefined) {
    print("ERROR failed to load plan, got: " + JSON.stringify(plan));
    return {};
  }

  return plan;
}

function serverBasics(conn) {
  if (!conn) {
    conn = arango;
  }

  const version = conn.GET("/_admin/status");
  const role = version.serverInfo.role;

  serverRole = role;

  if (role === 'AGENT') {
    print("INFO talking to an agent");
  } else if (role === 'PRIMARY') {
    print("INFO talking to a primary db server");
  } else if (role === 'COORDINATOR') {
    print("INFO talking to a coordinator");
  } else if (role === 'SINGLE') {
    print("INFO talking to a single server");
  } else {
    print("INFO talking to a unknown server, role: " + role);
    serverRole = "UNKNOWN";
  }

  endpoint2role[conn.getEndpoint()] = serverRole;
}

exports.serverBasics = serverBasics;
exports.loadAgencyPlan = loadAgencyPlan;
