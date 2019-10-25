/* global arango, print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Common replication utilities
// /
// / @file
// /
// / DISCLAIMER
// /
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Simon Grätzer
// //////////////////////////////////////////////////////////////////////////////

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;

const sleep = require('internal').sleep;

exports.compareTicks = function(l, r) {
  var i;
  if (l === null) {
    l = "0";
  }
  if (r === null) {
    r = "0";
  }
  if (l.length !== r.length) {
    return l.length - r.length < 0 ? -1 : 1;
  }

  // length is equal
  for (i = 0; i < l.length; ++i) {
    if (l[i] !== r[i]) {
      return l[i] < r[i] ? -1 : 1;
    }
  }

  return 0;
};

exports.reconnectRetry = function(endpoint, databaseName, user, password) {
  let sleepTime = 0.1;
  let ex;
  do {
    try {
      arango.reconnect(endpoint, databaseName, user, password);
      return;
    } catch (ex) {
      print(RED + "connecting " + endpoint + " failed - retrying (" + ex.message + ")" + RESET);
    }
    sleepTime *= 2;
    sleep(sleepTime);
  } while (sleepTime < 5);
  print(RED + "giving up!" + RESET);
  throw ex;
};
