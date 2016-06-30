/* jshint unused: false */
/* global arango, print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2013 triAGENS GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// //////////////////////////////////////////////////////////////////////////////

const arangosh = require('@arangodb/arangosh');
const printf = require('internal').printf;

const colors = require('internal').COLORS;
const BLUE = colors.COLOR_BLUE;
const RED = colors.COLOR_RED;
const GREEN = colors.COLOR_GREEN;
const CYAN = colors.COLOR_CYAN;
const RESET = colors.COLOR_RESET;

// //////////////////////////////////////////////////////////////////////////////
// / @brief fills left
// //////////////////////////////////////////////////////////////////////////////

const fillL = function (s, w) {
  if (s.length >= w) {
    return s;
  }

  return (new Array(w - s.length + 1)).join(' ') + s;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief fills right
// //////////////////////////////////////////////////////////////////////////////

const fillR = function (s, w) {
  if (s.length >= w) {
    return s;
  }

  return s + (new Array(w - s.length + 1)).join(' ');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief generates work description
// //////////////////////////////////////////////////////////////////////////////

const workDescription = function (level, w, desc, now) {
  const t = w.type;

  desc.level = level;

  if (level === 0) {
    desc.root = desc;
  }

  desc.level = level;

  if (t === 'thread') {
    desc.root.thread = desc;

    desc.type = 'thread';
    desc.name = w.name;
    desc.number = String(w.number);
    desc.info = '';
  } else if (t === 'AQL query') {
    desc.type = 'aql';
    desc.name = GREEN + 'AQL' + RESET;
    desc.info = w.description;
  } else if (t === 'http-handler') {
    desc.type = 'request';
    desc.name = GREEN + 'REQUEST' + RESET;
    desc.info = w.method + ' ' + w.url + ' (' + w.protocol + ')';
    desc.runtime = '' + (now - w.startTime).toFixed(2);

    desc = desc.child = {
      root: desc.root,
      level: desc.level,
      type: 'variable',
      name: CYAN + 'client ' + RESET,
      info: w.client.address + ':' + w.client.port
    };
  } else {
    desc.type = 'unknown';
    desc.name = RED + t + RESET;
    desc.info = '';
  }

  if (w.hasOwnProperty('parent')) {
    desc.child = {
      root: desc.root
    };
    workDescription(level + 1, w.parent, desc.child, now);
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief outputs work description
// //////////////////////////////////////////////////////////////////////////////

const outputWorkDescription = function (desc, opts) {
  if (desc.type === 'thread') {
    if (desc.level > 0 || desc.name === 'Work Monitor') {
      return;
    }
  }

  if (desc.level === 0) {
    let hdr = BLUE;
    hdr += fillL(desc.thread.number, opts.threadNumberWidth);
    hdr += ' ' + fillR(desc.thread.name, opts.threadWidth);
    hdr += RESET;

    print(hdr);

    if (desc.type === 'thread') {
      return;
    }
  }

  let line = fillR('', opts.threadNumberWidth + 2);

  if (desc.hasOwnProperty('runtime')) {
    line += ' ' + fillL(desc.runtime, opts.runtimeWidth) + 's ';
  } else {
    line += fillL('', opts.runtimeWidth + 3);
  }

  line += fillR('', 2 * desc.level + 1) + desc.name;
  line += ' ' + desc.info;

  print(line);

  if (desc.hasOwnProperty('child')) {
    outputWorkDescription(desc.child, opts);
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief request work information
// //////////////////////////////////////////////////////////////////////////////

const workOverview = function () {
  const res = arango.GET('/_admin/work-monitor');
  arangosh.checkRequestResult(res);

  const work = res.work;

  let descs = [];
  let w1 = 0;
  let w2 = 0;
  let w3 = 0;
  const now = res.time;

  work.forEach(function (w) {
    let desc = {};

    workDescription(0, w, desc, now);
    descs.push(desc);

    w1 = Math.max(w1, desc.root.thread.name.length);
    w3 = Math.max(w3, desc.root.thread.number.length);

    while (desc !== undefined) {
      if (desc.hasOwnProperty('runtime')) {
        w2 = Math.max(w2, desc.runtime.length);
      }

      desc = desc.child;
    }
  });

  descs.forEach(function (d) {
    outputWorkDescription(d, {
      threadWidth: w1,
      runtimeWidth: w2,
      threadNumberWidth: w3
    });
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief exports
// //////////////////////////////////////////////////////////////////////////////

exports.workOverview = workOverview;
