// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class BaseArgumentsProcessor {
  constructor(args) {
    this.args_ = args;
    this.result_ = this.getDefaultResults();
    console.assert(this.result_ !== undefined)
    console.assert(this.result_.logFileName !== undefined);
    this.argsDispatch_ = this.getArgsDispatch();
    console.assert(this.argsDispatch_ !== undefined);
  }

  getDefaultResults() {
    throw "Implement in getDefaultResults in subclass";
  }

  getArgsDispatch() {
    throw "Implement getArgsDispatch in subclass";
  }

  result() { return this.result_ }

  printUsageAndExit() {
    print('Cmdline args: [options] [log-file-name]\n' +
          'Default log file name is "' +
          this.result_.logFileName + '".\n');
    print('Options:');
    for (var arg in this.argsDispatch_) {
      var synonyms = [arg];
      var dispatch = this.argsDispatch_[arg];
      for (var synArg in this.argsDispatch_) {
        if (arg !== synArg && dispatch === this.argsDispatch_[synArg]) {
          synonyms.push(synArg);
          delete this.argsDispatch_[synArg];
        }
      }
      print('  ' + synonyms.join(', ').padEnd(20) + " " + dispatch[2]);
    }
    quit(2);
  }

  parse() {
    while (this.args_.length) {
      var arg = this.args_.shift();
      if (arg.charAt(0) != '-') {
        this.result_.logFileName = arg;
        continue;
      }
      var userValue = null;
      var eqPos = arg.indexOf('=');
      if (eqPos != -1) {
        userValue = arg.substr(eqPos + 1);
        arg = arg.substr(0, eqPos);
      }
      if (arg in this.argsDispatch_) {
        var dispatch = this.argsDispatch_[arg];
        var property = dispatch[0];
        var defaultValue = dispatch[1];
        if (typeof defaultValue == "function") {
          userValue = defaultValue(userValue);
        } else if (userValue == null) {
          userValue = defaultValue;
        }
        this.result_[property] = userValue;
      } else {
        return false;
      }
    }
    return true;
  }
}

function parseBool(str) {
  if (str == "true" || str == "1") return true;
  return false;
}
