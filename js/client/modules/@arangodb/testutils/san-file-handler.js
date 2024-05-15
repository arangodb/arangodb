/* jshint strict: false, sub: true */
/* global print, arango */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const fs = require('fs');
const crashUtils = require('@arangodb/testutils/crash-utils');

var regex = /[^\u0000-\u00ff]/; // Small performance gain from pre-compiling the regex
function containsDoubleByte(str) {
    if (!str.length) return false;
    if (str.charCodeAt(0) > 255) return true;
    return regex.test(str);
}

class sanHandler {
  constructor(binaryName, sanOptions, isSan, extremeVerbosity) {
    this.binaryName = binaryName;
    this.sanOptions = _.clone(sanOptions);
    this.enabled = isSan;
    this.extremeVerbosity = extremeVerbosity;
    this.sanitizerLogPaths = {};
    this.backup = {};
  }
  detectLogfiles(rootDir, tmpDir) {
    if (this.enabled) {
      if (containsDoubleByte(rootDir)) {
        rootDir = tmpDir;
      }
      for (const [key, value] of Object.entries(this.sanOptions)) {
        let oneLogFile = fs.join(rootDir, key.toLowerCase().split('_')[0] + '.log');
        // we need the log files to contain the exe name, otherwise our code to pick them up won't find them
        this.sanOptions[key]['log_exe_name'] = "true";
        const origPath = this.sanOptions[key]['log_path'];
        this.sanOptions[key]['log_path'] = oneLogFile;
        this.sanitizerLogPaths[key] = { upstream: origPath, local: oneLogFile };
      }
    }
  }
  setSanOptions() {
    if (this.enabled) {
      print("Using sanOptions ", this.sanOptions);
      for (const [key, value] of Object.entries(this.sanOptions)) {
        let oneSet = "";
        for (const [keyOne, valueOne] of Object.entries(value)) {
          if (oneSet.length > 0) {
            oneSet += ":";
          }
          let val = valueOne.replace(/,/g, '_');
          oneSet += `${keyOne}=${val}`;
        }
        this.backup[key] = process.env[key];
        process.env[key] = oneSet;
      }
    }
  }
  resetSanOptions() {
    if (this.enabled) {
      for (const [key, value] of Object.entries(this.backup)) {
        process.env[key] = value;
      }
    }
  }

  fetchSanFileAfterExit(pid) {
    if (!this.enabled) {
      return false;
    }
    let ret = false;
    for (const [key, value] of Object.entries(this.sanitizerLogPaths)) {
      print("processing ", value);
      const { upstream, local } = value;
      let fn = `${local}.${this.binaryname}.${pid}`;
      if (this.extremeVerbosity) {
        print(`checking for ${fn}: ${fs.exists(fn)}`);
      }
      if (fs.exists(fn)) {
        let content = fs.read(fn);
        if (upstream) {
          print("found file ", fn, " - writing file ", `${upstream}.${this.binaryName}.${this.pid}`);
          fs.write(`${upstream}.${this.binaryName}.${this.pid}`, content);
        }
        if (content.length > 10) {
          crashUtils.GDB_OUTPUT += `Report of '${this.name}' in ${fn} contains: \n`;
          crashUtils.GDB_OUTPUT += content;
          ret = true;
        }
      }
    }
    return ret;
  }

}

exports.sanHandler = sanHandler;
