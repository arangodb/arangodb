'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief statistics handler
// /
// / @file
// /
// / DISCLAIMER
// /
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler, Lucas Dohmen
// / @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const cluster = require('@arangodb/cluster');
const db = internal.db;
const DEFAULT_REPLICATION_FACTOR_SYSTEM = internal.DEFAULT_REPLICATION_FACTOR_SYSTEM;

// //////////////////////////////////////////////////////////////////////////////
// / @brief statistics interval
// //////////////////////////////////////////////////////////////////////////////

exports.STATISTICS_INTERVAL = 10;

// //////////////////////////////////////////////////////////////////////////////
// / @brief statistics interval for history
// //////////////////////////////////////////////////////////////////////////////

exports.STATISTICS_HISTORY_INTERVAL = 15 * 60;
