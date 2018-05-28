/* global require, arango */
'use strict';

// /////////////////////////////////////////////////////////////////////////////
// @brief ArangoDB Insector
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

const _ = require("lodash");
const inspector = require("@arangodb/inspector");
const AsciiTable = require("ascii-table");

function showServers() {
  let servers = inspector.listServers();
  let table = new AsciiTable();

  table.setHeading("type", "id", "endpoint", "status");

  _.forEach(servers, function(info, id) {
    let status = "";

    if (info.type === "AGENT") {
      status = info.leading ? "leading" : "";
    } else {
      status = info.status;
    }

    table.addRow(info.type, info.id, info.endpoint || "unknown", status);
  });

  return table;
}

exports.servers = showServers;
