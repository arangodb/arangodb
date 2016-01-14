/*jshint unused: false */
/* global arango */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

const arangosh = require("@arangodb/arangosh");
const printf = require("internal").printf;

////////////////////////////////////////////////////////////////////////////////
/// @brief request work information
////////////////////////////////////////////////////////////////////////////////

const workOverview = function() {
  const res = arango.GET("/_admin/work-monitor");
  arangosh.checkRequestResult(res);

  const work = res.work;

  const output = function(indent, w) {
   let txt = "";

    if (w.type === "thread") {
      txt = w.name;
    } else if (w.type === "AQL query") {
      txt = w.description;
    } else if (w.type === "http-handler") {
      txt = w.url + " (started: " + w.startTime + ")";
    }
	 
    printf("%s%s: %s\n", indent, w.type, txt);

    if (w.hasOwnProperty('parent')) {
      output("  " + indent, w.parent);
    }
  };

  work.forEach(function(w) {output("", w);});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief exports
////////////////////////////////////////////////////////////////////////////////

exports.workOverview = workOverview;

