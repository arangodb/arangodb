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
const crypto = require('@arangodb/crypto');
const crashUtils = require('@arangodb/testutils/crash-utils');
const tu = require('@arangodb/testutils/test-utils');
const {versionHas} = require("@arangodb/test-helper");
const internal = require('internal');

var regex = /[^\u0000-\u00ff]/; // Small performance gain from pre-compiling the regex
function containsDoubleByte(str) {
  if (!str.length) return false;
  if (str.charCodeAt(0) > 255) return true;
  return regex.test(str);
}

let foundReportFiles = new Set();
const coverage_name = 'LLVM_PROFILE_FILE';

class sanHandler {
  constructor(binaryName, options) {
    this.binaryName = binaryName;
    this.sanOptions = _.cloneDeep(options.sanOptions);
    this.covOptions = _.cloneDeep(options.covOptions);
    this.isCov = options.isCov;
    this.enabled = options.isSan;
    this.extremeVerbosity = options.extremeVerbosity;
    this.sanitizerLogPaths = {};
    this.backup = {};
  }
  detectLogfiles(rootDir, tmpDir) {
    this.tmpDir = tmpDir;
    if (this.enabled) {
      if (containsDoubleByte(rootDir)) {
        rootDir = tmpDir;
      }
      for (const [key, value] of Object.entries(this.sanOptions)) {
        // ASAN/LSAN/UBSAN cannot use different logfiles, so we need to use the same name for all
        let logName = key === "TSAN_OPTIONS" ? "tsan.log" : "alubsan.log";
        let oneLogFile = fs.join(rootDir, logName);
        // we need the log files to contain the exe name, otherwise our code to pick them up won't find them
        this.sanOptions[key]['log_exe_name'] = "true";
        const origPath = this.sanOptions[key]['log_path'];
        this.sanOptions[key]['log_path'] = oneLogFile;
        this.sanitizerLogPaths[key] = { upstream: origPath, local: oneLogFile };
      }
    }
  }
  getSanOptions() {
    let subProcesEnv = [`TMPDIR=${this.tmpDir}`];
    if (this.enabled) {
      for (const [key, value] of Object.entries(this.sanOptions)) {
        let oneSet = "";
        for (const [keyOne, valueOne] of Object.entries(value)) {
          if (oneSet.length > 0) {
            oneSet += ":";
          }
          let val = valueOne.replace(/,/g, '_');
          oneSet += `${keyOne}=${val}`;
        }
        subProcesEnv.push(`${key}=${oneSet}`);
      }
    }

    if (this.isCov) {
      let path = this.covOptions[coverage_name].split(fs.pathSeparator);
      if (path[path.length - 1] === "testingjs") {
        path.pop();
      }
      path.push(crypto.md5(String(internal.time() + Math.random())));
      subProcesEnv.push(`${coverage_name}=${fs.pathSeparator + fs.join(...path)}_${this.binaryName}`);
    }
    return subProcesEnv;
  }

  fetchSanFileAfterExit(pid) {
    if (!this.enabled) {
      return false;
    }
    let ret = false;
    let suffix = `.${this.binaryName}.${pid}`;
    for (const [key, value] of Object.entries(this.sanitizerLogPaths)) {
      const { upstream, local } = value;
      let fn = `${local}${suffix}`;
      if (foundReportFiles.has(fn)) {
        // we don't want to process the same file twice
        continue;
      }
      if (this.extremeVerbosity) {
        print(`checking for ${fn}: ${fs.exists(fn)}`);
      }
      if (fs.exists(fn)) {
        foundReportFiles.add(fn);
        let content = fs.read(fn);
        if (upstream) {
          let outFn = `${upstream}${suffix}`;
          print("found file ", fn, " - writing file ", outFn);
          fs.write(outFn, content);
        }
        if (content.length > 10) {
          crashUtils.GDB_OUTPUT += `Report of '${this.binaryName}' in ${fn} contains: \n`;
          crashUtils.GDB_OUTPUT += content;
          ret = true;
        }
      }
    }
    return ret;
  }

}

exports.sanHandler = sanHandler;
exports.getNumSanitizerReports = () => foundReportFiles.size;
exports.registerOptions = function(optionsDefaults, optionsDocumentation, optionHandlers) {
  const isCoverage = versionHas('coverage');
  const isSan = versionHas('asan') || versionHas('tsan');
  const isInstrumented = versionHas('asan') || versionHas('tsan') || versionHas('coverage');
  tu.CopyIntoObject(optionsDefaults, {
    'isCov': isCoverage,
    'covOptions': {},
    'sanitizer': isSan,
    'isSan': isSan,
    'sanOptions': {},
    'isInstrumented': isInstrumented,
    'oneTestTimeout': (isInstrumented? 25 : 15) * 60,
  });

  tu.CopyIntoList(optionsDocumentation, [
    'SUT instrumented binaries',
    '   - `sanitizer`: if set the programs are run with enabled sanitizer',
    '   - `isSan`: doubles oneTestTimeot value if set to true (for ASAN-related builds)',
    '     and need longer timeouts',
    '   - `isCov`: doubles oneTestTimeot value if set to true',
  ]);
  optionHandlers.push(function(options) {
    // fiddle in suppressions for sanitizers if not already set from an
    // outside script. this can populate the environment variables
    // - ASAN_OPTIONS
    // - UBSAN_OPTIONS
    // - LSAN_OPTIONS
    // - TSAN_OPTIONS
    // note: this code repeats existing logic that can be found in
    // scripts/unittest as well. according to @dothebart it must be
    // present in both code locations.
    if (options.isSan) {
      ['asan', 'lsan', 'ubsan', 'tsan'].forEach(whichSan => {
        let fileName = whichSan + "_arangodb_suppressions.txt";
        let fullNameSup = `${fs.join(fs.makeAbsolute(''), fileName)}`;
        let sanOpt = `${whichSan.toUpperCase()}_OPTIONS`;
        options.sanOptions[sanOpt] = {};
        if (process.env.hasOwnProperty(sanOpt)) {
          let opt = process.env[sanOpt];
          delete process.env[sanOpt];
          opt.split(':').forEach(oneOpt => {
            let pair = oneOpt.split('=');
            if (pair.length === 2) {
              options.sanOptions[sanOpt][pair[0]] = pair[1];
            }
          });
        }
        else {
          options.sanOptions[sanOpt] = {};
        }
        if (fs.exists(fileName)) {
          options.sanOptions[sanOpt]['suppressions'] = fullNameSup;
        }
      });
    }
    if (options.isCov && process.env.hasOwnProperty(coverage_name)) {
      options.covOptions[coverage_name] = process.env[coverage_name];
      delete process.env[coverage_name];
    }
  });
};
