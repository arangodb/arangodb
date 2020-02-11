// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function inherits(childCtor, parentCtor) {
  childCtor.prototype.__proto__ = parentCtor.prototype;
};

/**
 * A thin wrapper around shell's 'read' function showing a file name on error.
 */
function readFile(fileName) {
  try {
    return read(fileName);
  } catch (e) {
    print(fileName + ': ' + (e.message || e));
    throw e;
  }
}

/**
 * Parser for dynamic code optimization state.
 */
function parseState(s) {
  switch (s) {
  case "": return Profile.CodeState.COMPILED;
  case "~": return Profile.CodeState.OPTIMIZABLE;
  case "*": return Profile.CodeState.OPTIMIZED;
  }
  throw new Error("unknown code state: " + s);
}


function IcProcessor() {
  var propertyICParser = [parseInt, parseInt, parseInt, parseString,
      parseString, parseInt, parseString, parseString, parseString];
  LogReader.call(this, {
      'code-creation': {
          parsers: [parseString, parseInt, parseInt, parseInt, parseInt,
              parseString, parseVarArgs],
          processor: this.processCodeCreation },
      'code-move': { parsers: [parseInt, parseInt],
          processor: this.processCodeMove },
      'code-delete': { parsers: [parseInt],
          processor: this.processCodeDelete },
      'sfi-move': { parsers: [parseInt, parseInt],
          processor: this.processFunctionMove },
      'LoadGlobalIC': {
        parsers : propertyICParser,
        processor: this.processPropertyIC.bind(this, "LoadGlobalIC") },
      'StoreGlobalIC': {
        parsers : propertyICParser,
        processor: this.processPropertyIC.bind(this, "StoreGlobalIC") },
      'LoadIC': {
        parsers : propertyICParser,
        processor: this.processPropertyIC.bind(this, "LoadIC") },
      'StoreIC': {
        parsers : propertyICParser,
        processor: this.processPropertyIC.bind(this, "StoreIC") },
      'KeyedLoadIC': {
        parsers : propertyICParser,
        processor: this.processPropertyIC.bind(this, "KeyedLoadIC") },
      'KeyedStoreIC': {
        parsers : propertyICParser,
        processor: this.processPropertyIC.bind(this, "KeyedStoreIC") },
      'StoreInArrayLiteralIC': {
        parsers : propertyICParser,
        processor: this.processPropertyIC.bind(this, "StoreInArrayLiteralIC") },
      });
  this.profile_ = new Profile();

  this.LoadGlobalIC = 0;
  this.StoreGlobalIC = 0;
  this.LoadIC = 0;
  this.StoreIC = 0;
  this.KeyedLoadIC = 0;
  this.KeyedStoreIC = 0;
  this.StoreInArrayLiteralIC = 0;
}
inherits(IcProcessor, LogReader);

/**
 * @override
 */
IcProcessor.prototype.printError = function(str) {
  print(str);
};

IcProcessor.prototype.processString = function(string) {
  var end = string.length;
  var current = 0;
  var next = 0;
  var line;
  var i = 0;
  var entry;
  while (current < end) {
    next = string.indexOf("\n", current);
    if (next === -1) break;
    i++;
    line = string.substring(current, next);
    current = next + 1;
    this.processLogLine(line);
  }
}

IcProcessor.prototype.processLogFile = function(fileName) {
  this.collectEntries = true
  this.lastLogFileName_ = fileName;
  var line;
  while (line = readline()) {
    this.processLogLine(line);
  }
  print();
  print("=====================");
  print("LoadGlobal: " + this.LoadGlobalIC);
  print("StoreGlobal: " + this.StoreGlobalIC);
  print("Load: " + this.LoadIC);
  print("Store: " + this.StoreIC);
  print("KeyedLoad: " + this.KeyedLoadIC);
  print("KeyedStore: " + this.KeyedStoreIC);
  print("StoreInArrayLiteral: " + this.StoreInArrayLiteralIC);
};

IcProcessor.prototype.addEntry = function(entry) {
  this.entries.push(entry);
}

IcProcessor.prototype.processCodeCreation = function(
    type, kind, timestamp, start, size, name, maybe_func) {
  if (maybe_func.length) {
    var funcAddr = parseInt(maybe_func[0]);
    var state = parseState(maybe_func[1]);
    this.profile_.addFuncCode(type, name, timestamp, start, size, funcAddr, state);
  } else {
    this.profile_.addCode(type, name, timestamp, start, size);
  }
};


IcProcessor.prototype.processCodeMove = function(from, to) {
  this.profile_.moveCode(from, to);
};


IcProcessor.prototype.processCodeDelete = function(start) {
  this.profile_.deleteCode(start);
};


IcProcessor.prototype.processFunctionMove = function(from, to) {
  this.profile_.moveFunc(from, to);
};

IcProcessor.prototype.formatName = function(entry) {
  if (!entry) return "<unknown>"
  var name = entry.func.getName();
  var re = /(.*):[0-9]+:[0-9]+$/;
  var array = re.exec(name);
  if (!array) return name;
  return entry.getState() + array[1];
}

IcProcessor.prototype.processPropertyIC = function (
    type, pc, line, column, old_state, new_state, map, name, modifier,
    slow_reason) {
  this[type]++;
  var entry = this.profile_.findEntry(pc);
  print(type + " (" + old_state + "->" + new_state + modifier + ") at " +
        this.formatName(entry) + ":" + line + ":" + column + " " + name +
        " (map 0x" + map.toString(16) + ")" +
        (slow_reason ? " " + slow_reason : ""));
}


class ArgumentsProcessor extends BaseArgumentsProcessor {
  getArgsDispatch() {
    return {
      '--range': ['range', 'auto,auto',
          'Specify the range limit as [start],[end]'],
      '--source-map': ['sourceMap', null,
          'Specify the source map that should be used for output']
    };
  }
  getDefaultResults() {
   return {
      logFileName: 'v8.log',
      range: 'auto,auto',
    };
  }
}
