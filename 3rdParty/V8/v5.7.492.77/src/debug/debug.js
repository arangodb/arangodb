// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function (global, utils) {
"use strict";

// ----------------------------------------------------------------------------
// Imports

var FrameMirror = global.FrameMirror;
var GlobalArray = global.Array;
var GlobalRegExp = global.RegExp;
var IsNaN = global.isNaN;
var JSONParse = global.JSON.parse;
var JSONStringify = global.JSON.stringify;
var LookupMirror = global.LookupMirror;
var MakeMirror = global.MakeMirror;
var MakeMirrorSerializer = global.MakeMirrorSerializer;
var MathMin = global.Math.min;
var Mirror = global.Mirror;
var MirrorType;
var ParseInt = global.parseInt;
var ValueMirror = global.ValueMirror;

utils.Import(function(from) {
  MirrorType = from.MirrorType;
});

//----------------------------------------------------------------------------

// Default number of frames to include in the response to backtrace request.
var kDefaultBacktraceLength = 10;

var Debug = {};

// Regular expression to skip "crud" at the beginning of a source line which is
// not really code. Currently the regular expression matches whitespace and
// comments.
var sourceLineBeginningSkip = /^(?:\s*(?:\/\*.*?\*\/)*)*/;

// Debug events which can occour in the V8 JavaScript engine. These originate
// from the API include file debug.h.
Debug.DebugEvent = { Break: 1,
                     Exception: 2,
                     AfterCompile: 3,
                     CompileError: 4,
                     AsyncTaskEvent: 5 };

// Types of exceptions that can be broken upon.
Debug.ExceptionBreak = { Caught : 0,
                         Uncaught: 1 };

// The different types of steps.
Debug.StepAction = { StepOut: 0,
                     StepNext: 1,
                     StepIn: 2,
                     StepFrame: 3 };

// The different types of scripts matching enum ScriptType in objects.h.
Debug.ScriptType = { Native: 0,
                     Extension: 1,
                     Normal: 2,
                     Wasm: 3};

// The different types of script compilations matching enum
// Script::CompilationType in objects.h.
Debug.ScriptCompilationType = { Host: 0,
                                Eval: 1,
                                JSON: 2 };

// The different script break point types.
Debug.ScriptBreakPointType = { ScriptId: 0,
                               ScriptName: 1,
                               ScriptRegExp: 2 };

// The different types of breakpoint position alignments.
// Must match BreakPositionAlignment in debug.h.
Debug.BreakPositionAlignment = {
  Statement: 0,
  BreakPosition: 1
};

function ScriptTypeFlag(type) {
  return (1 << type);
}

// Globals.
var next_response_seq = 0;
var next_break_point_number = 1;
var break_points = [];
var script_break_points = [];
var debugger_flags = {
  breakPointsActive: {
    value: true,
    getValue: function() { return this.value; },
    setValue: function(value) {
      this.value = !!value;
      %SetBreakPointsActive(this.value);
    }
  },
  breakOnCaughtException: {
    getValue: function() { return Debug.isBreakOnException(); },
    setValue: function(value) {
      if (value) {
        Debug.setBreakOnException();
      } else {
        Debug.clearBreakOnException();
      }
    }
  },
  breakOnUncaughtException: {
    getValue: function() { return Debug.isBreakOnUncaughtException(); },
    setValue: function(value) {
      if (value) {
        Debug.setBreakOnUncaughtException();
      } else {
        Debug.clearBreakOnUncaughtException();
      }
    }
  },
};


// Create a new break point object and add it to the list of break points.
function MakeBreakPoint(source_position, opt_script_break_point) {
  var break_point = new BreakPoint(source_position, opt_script_break_point);
  break_points.push(break_point);
  return break_point;
}


// Object representing a break point.
// NOTE: This object does not have a reference to the function having break
// point as this would cause function not to be garbage collected when it is
// not used any more. We do not want break points to keep functions alive.
function BreakPoint(source_position, opt_script_break_point) {
  this.source_position_ = source_position;
  if (opt_script_break_point) {
    this.script_break_point_ = opt_script_break_point;
  } else {
    this.number_ = next_break_point_number++;
  }
  this.active_ = true;
  this.condition_ = null;
}


BreakPoint.prototype.number = function() {
  return this.number_;
};


BreakPoint.prototype.func = function() {
  return this.func_;
};


BreakPoint.prototype.source_position = function() {
  return this.source_position_;
};


BreakPoint.prototype.active = function() {
  if (this.script_break_point()) {
    return this.script_break_point().active();
  }
  return this.active_;
};


BreakPoint.prototype.condition = function() {
  if (this.script_break_point() && this.script_break_point().condition()) {
    return this.script_break_point().condition();
  }
  return this.condition_;
};


BreakPoint.prototype.script_break_point = function() {
  return this.script_break_point_;
};


BreakPoint.prototype.enable = function() {
  this.active_ = true;
};


BreakPoint.prototype.disable = function() {
  this.active_ = false;
};


BreakPoint.prototype.setCondition = function(condition) {
  this.condition_ = condition;
};


BreakPoint.prototype.isTriggered = function(exec_state) {
  // Break point not active - not triggered.
  if (!this.active()) return false;

  // Check for conditional break point.
  if (this.condition()) {
    // If break point has condition try to evaluate it in the top frame.
    try {
      var mirror = exec_state.frame(0).evaluate(this.condition());
      // If no sensible mirror or non true value break point not triggered.
      if (!(mirror instanceof ValueMirror) || !mirror.value_) {
        return false;
      }
    } catch (e) {
      // Exception evaluating condition counts as not triggered.
      return false;
    }
  }

  // Break point triggered.
  return true;
};


// Function called from the runtime when a break point is hit. Returns true if
// the break point is triggered and supposed to break execution.
function IsBreakPointTriggered(break_id, break_point) {
  return break_point.isTriggered(MakeExecutionState(break_id));
}


// Object representing a script break point. The script is referenced by its
// script name or script id and the break point is represented as line and
// column.
function ScriptBreakPoint(type, script_id_or_name, opt_line, opt_column,
                          opt_groupId, opt_position_alignment) {
  this.type_ = type;
  if (type == Debug.ScriptBreakPointType.ScriptId) {
    this.script_id_ = script_id_or_name;
  } else if (type == Debug.ScriptBreakPointType.ScriptName) {
    this.script_name_ = script_id_or_name;
  } else if (type == Debug.ScriptBreakPointType.ScriptRegExp) {
    this.script_regexp_object_ = new GlobalRegExp(script_id_or_name);
  } else {
    throw %make_error(kDebugger, "Unexpected breakpoint type " + type);
  }
  this.line_ = opt_line || 0;
  this.column_ = opt_column;
  this.groupId_ = opt_groupId;
  this.position_alignment_ = IS_UNDEFINED(opt_position_alignment)
      ? Debug.BreakPositionAlignment.Statement : opt_position_alignment;
  this.active_ = true;
  this.condition_ = null;
  this.break_points_ = [];
}


ScriptBreakPoint.prototype.number = function() {
  return this.number_;
};


ScriptBreakPoint.prototype.groupId = function() {
  return this.groupId_;
};


ScriptBreakPoint.prototype.type = function() {
  return this.type_;
};


ScriptBreakPoint.prototype.script_id = function() {
  return this.script_id_;
};


ScriptBreakPoint.prototype.script_name = function() {
  return this.script_name_;
};


ScriptBreakPoint.prototype.script_regexp_object = function() {
  return this.script_regexp_object_;
};


ScriptBreakPoint.prototype.line = function() {
  return this.line_;
};


ScriptBreakPoint.prototype.column = function() {
  return this.column_;
};


ScriptBreakPoint.prototype.actual_locations = function() {
  var locations = [];
  for (var i = 0; i < this.break_points_.length; i++) {
    locations.push(this.break_points_[i].actual_location);
  }
  return locations;
};


ScriptBreakPoint.prototype.update_positions = function(line, column) {
  this.line_ = line;
  this.column_ = column;
};


ScriptBreakPoint.prototype.active = function() {
  return this.active_;
};


ScriptBreakPoint.prototype.condition = function() {
  return this.condition_;
};


ScriptBreakPoint.prototype.enable = function() {
  this.active_ = true;
};


ScriptBreakPoint.prototype.disable = function() {
  this.active_ = false;
};


ScriptBreakPoint.prototype.setCondition = function(condition) {
  this.condition_ = condition;
};


// Check whether a script matches this script break point. Currently this is
// only based on script name.
ScriptBreakPoint.prototype.matchesScript = function(script) {
  if (this.type_ == Debug.ScriptBreakPointType.ScriptId) {
    return this.script_id_ == script.id;
  } else {
    // We might want to account columns here as well.
    if (!(script.line_offset <= this.line_  &&
          this.line_ < script.line_offset + %ScriptLineCount(script))) {
      return false;
    }
    if (this.type_ == Debug.ScriptBreakPointType.ScriptName) {
      return this.script_name_ == script.nameOrSourceURL();
    } else if (this.type_ == Debug.ScriptBreakPointType.ScriptRegExp) {
      return this.script_regexp_object_.test(script.nameOrSourceURL());
    } else {
      throw %make_error(kDebugger, "Unexpected breakpoint type " + this.type_);
    }
  }
};


// Set the script break point in a script.
ScriptBreakPoint.prototype.set = function (script) {
  var column = this.column();
  var line = this.line();
  // If the column is undefined the break is on the line. To help locate the
  // first piece of breakable code on the line try to find the column on the
  // line which contains some source.
  if (IS_UNDEFINED(column)) {
    var source_line = %ScriptSourceLine(script, line || script.line_offset);

    // Allocate array for caching the columns where the actual source starts.
    if (!script.sourceColumnStart_) {
      script.sourceColumnStart_ = new GlobalArray(%ScriptLineCount(script));
    }

    // Fill cache if needed and get column where the actual source starts.
    if (IS_UNDEFINED(script.sourceColumnStart_[line])) {
      script.sourceColumnStart_[line] =
          source_line.match(sourceLineBeginningSkip)[0].length;
    }
    column = script.sourceColumnStart_[line];
  }

  // Convert the line and column into an absolute position within the script.
  var position = Debug.findScriptSourcePosition(script, this.line(), column);

  // If the position is not found in the script (the script might be shorter
  // than it used to be) just ignore it.
  if (IS_NULL(position)) return;

  // Create a break point object and set the break point.
  var break_point = MakeBreakPoint(position, this);
  var actual_position = %SetScriptBreakPoint(script, position,
                                             this.position_alignment_,
                                             break_point);
  if (IS_UNDEFINED(actual_position)) {
    actual_position = position;
  }
  var actual_location = script.locationFromPosition(actual_position, true);
  break_point.actual_location = { line: actual_location.line,
                                  column: actual_location.column,
                                  script_id: script.id };
  this.break_points_.push(break_point);
  return break_point;
};


// Clear all the break points created from this script break point
ScriptBreakPoint.prototype.clear = function () {
  var remaining_break_points = [];
  for (var i = 0; i < break_points.length; i++) {
    if (break_points[i].script_break_point() &&
        break_points[i].script_break_point() === this) {
      %ClearBreakPoint(break_points[i]);
    } else {
      remaining_break_points.push(break_points[i]);
    }
  }
  break_points = remaining_break_points;
  this.break_points_ = [];
};


Debug.setListener = function(listener, opt_data) {
  if (!IS_FUNCTION(listener) && !IS_UNDEFINED(listener) && !IS_NULL(listener)) {
    throw %make_type_error(kDebuggerType);
  }
  %SetDebugEventListener(listener, opt_data);
};


// Returns a Script object. If the parameter is a function the return value
// is the script in which the function is defined. If the parameter is a string
// the return value is the script for which the script name has that string
// value.  If it is a regexp and there is a unique script whose name matches
// we return that, otherwise undefined.
Debug.findScript = function(func_or_script_name) {
  if (IS_FUNCTION(func_or_script_name)) {
    return %FunctionGetScript(func_or_script_name);
  } else if (%IsRegExp(func_or_script_name)) {
    var scripts = this.scripts();
    var last_result = null;
    var result_count = 0;
    for (var i in scripts) {
      var script = scripts[i];
      if (func_or_script_name.test(script.name)) {
        last_result = script;
        result_count++;
      }
    }
    // Return the unique script matching the regexp.  If there are more
    // than one we don't return a value since there is no good way to
    // decide which one to return.  Returning a "random" one, say the
    // first, would introduce nondeterminism (or something close to it)
    // because the order is the heap iteration order.
    if (result_count == 1) {
      return last_result;
    } else {
      return UNDEFINED;
    }
  } else {
    return %GetScript(func_or_script_name);
  }
};

// Returns the script source. If the parameter is a function the return value
// is the script source for the script in which the function is defined. If the
// parameter is a string the return value is the script for which the script
// name has that string value.
Debug.scriptSource = function(func_or_script_name) {
  return this.findScript(func_or_script_name).source;
};


Debug.source = function(f) {
  if (!IS_FUNCTION(f)) throw %make_type_error(kDebuggerType);
  return %FunctionGetSourceCode(f);
};


Debug.sourcePosition = function(f) {
  if (!IS_FUNCTION(f)) throw %make_type_error(kDebuggerType);
  return %FunctionGetScriptSourcePosition(f);
};


Debug.findFunctionSourceLocation = function(func, opt_line, opt_column) {
  var script = %FunctionGetScript(func);
  var script_offset = %FunctionGetScriptSourcePosition(func);
  return %ScriptLocationFromLine(script, opt_line, opt_column, script_offset);
};


// Returns the character position in a script based on a line number and an
// optional position within that line.
Debug.findScriptSourcePosition = function(script, opt_line, opt_column) {
  var location = %ScriptLocationFromLine(script, opt_line, opt_column, 0);
  return location ? location.position : null;
};


Debug.findBreakPoint = function(break_point_number, remove) {
  var break_point;
  for (var i = 0; i < break_points.length; i++) {
    if (break_points[i].number() == break_point_number) {
      break_point = break_points[i];
      // Remove the break point from the list if requested.
      if (remove) {
        break_points.splice(i, 1);
      }
      break;
    }
  }
  if (break_point) {
    return break_point;
  } else {
    return this.findScriptBreakPoint(break_point_number, remove);
  }
};

Debug.findBreakPointActualLocations = function(break_point_number) {
  for (var i = 0; i < script_break_points.length; i++) {
    if (script_break_points[i].number() == break_point_number) {
      return script_break_points[i].actual_locations();
    }
  }
  for (var i = 0; i < break_points.length; i++) {
    if (break_points[i].number() == break_point_number) {
      return [break_points[i].actual_location];
    }
  }
  return [];
};

Debug.setBreakPoint = function(func, opt_line, opt_column, opt_condition) {
  if (!IS_FUNCTION(func)) throw %make_type_error(kDebuggerType);
  // Break points in API functions are not supported.
  if (%FunctionIsAPIFunction(func)) {
    throw %make_error(kDebugger, 'Cannot set break point in native code.');
  }
  // Find source position.
  var source_position =
      this.findFunctionSourceLocation(func, opt_line, opt_column).position;
  // Find the script for the function.
  var script = %FunctionGetScript(func);
  // Break in builtin JavaScript code is not supported.
  if (script.type == Debug.ScriptType.Native) {
    throw %make_error(kDebugger, 'Cannot set break point in native code.');
  }
  // If the script for the function has a name convert this to a script break
  // point.
  if (script && script.id) {
    // Find line and column for the position in the script and set a script
    // break point from that.
    var location = script.locationFromPosition(source_position, false);
    return this.setScriptBreakPointById(script.id,
                                        location.line, location.column,
                                        opt_condition);
  } else {
    // Set a break point directly on the function.
    var break_point = MakeBreakPoint(source_position);
    var actual_position =
        %SetFunctionBreakPoint(func, source_position, break_point);
    var actual_location = script.locationFromPosition(actual_position, true);
    break_point.actual_location = { line: actual_location.line,
                                    column: actual_location.column,
                                    script_id: script.id };
    break_point.setCondition(opt_condition);
    return break_point.number();
  }
};


Debug.setBreakPointByScriptIdAndPosition = function(script_id, position,
                                                    condition, enabled,
                                                    opt_position_alignment)
{
  var break_point = MakeBreakPoint(position);
  break_point.setCondition(condition);
  if (!enabled) {
    break_point.disable();
  }
  var script = scriptById(script_id);
  if (script) {
    var position_alignment = IS_UNDEFINED(opt_position_alignment)
        ? Debug.BreakPositionAlignment.Statement : opt_position_alignment;
    break_point.actual_position = %SetScriptBreakPoint(script, position,
        position_alignment, break_point);
  }
  return break_point;
};


Debug.enableBreakPoint = function(break_point_number) {
  var break_point = this.findBreakPoint(break_point_number, false);
  // Only enable if the breakpoint hasn't been deleted:
  if (break_point) {
    break_point.enable();
  }
};


Debug.disableBreakPoint = function(break_point_number) {
  var break_point = this.findBreakPoint(break_point_number, false);
  // Only enable if the breakpoint hasn't been deleted:
  if (break_point) {
    break_point.disable();
  }
};


Debug.changeBreakPointCondition = function(break_point_number, condition) {
  var break_point = this.findBreakPoint(break_point_number, false);
  break_point.setCondition(condition);
};


Debug.clearBreakPoint = function(break_point_number) {
  var break_point = this.findBreakPoint(break_point_number, true);
  if (break_point) {
    return %ClearBreakPoint(break_point);
  } else {
    break_point = this.findScriptBreakPoint(break_point_number, true);
    if (!break_point) throw %make_error(kDebugger, 'Invalid breakpoint');
  }
};


Debug.clearAllBreakPoints = function() {
  for (var i = 0; i < break_points.length; i++) {
    var break_point = break_points[i];
    %ClearBreakPoint(break_point);
  }
  break_points = [];
};


Debug.disableAllBreakPoints = function() {
  // Disable all user defined breakpoints:
  for (var i = 1; i < next_break_point_number; i++) {
    Debug.disableBreakPoint(i);
  }
  // Disable all exception breakpoints:
  %ChangeBreakOnException(Debug.ExceptionBreak.Caught, false);
  %ChangeBreakOnException(Debug.ExceptionBreak.Uncaught, false);
};


Debug.findScriptBreakPoint = function(break_point_number, remove) {
  var script_break_point;
  for (var i = 0; i < script_break_points.length; i++) {
    if (script_break_points[i].number() == break_point_number) {
      script_break_point = script_break_points[i];
      // Remove the break point from the list if requested.
      if (remove) {
        script_break_point.clear();
        script_break_points.splice(i,1);
      }
      break;
    }
  }
  return script_break_point;
};


// Sets a breakpoint in a script identified through id or name at the
// specified source line and column within that line.
Debug.setScriptBreakPoint = function(type, script_id_or_name,
                                     opt_line, opt_column, opt_condition,
                                     opt_groupId, opt_position_alignment) {
  // Create script break point object.
  var script_break_point =
      new ScriptBreakPoint(type, script_id_or_name, opt_line, opt_column,
                           opt_groupId, opt_position_alignment);

  // Assign number to the new script break point and add it.
  script_break_point.number_ = next_break_point_number++;
  script_break_point.setCondition(opt_condition);
  script_break_points.push(script_break_point);

  // Run through all scripts to see if this script break point matches any
  // loaded scripts.
  var scripts = this.scripts();
  for (var i = 0; i < scripts.length; i++) {
    if (script_break_point.matchesScript(scripts[i])) {
      script_break_point.set(scripts[i]);
    }
  }

  return script_break_point.number();
};


Debug.setScriptBreakPointById = function(script_id,
                                         opt_line, opt_column,
                                         opt_condition, opt_groupId,
                                         opt_position_alignment) {
  return this.setScriptBreakPoint(Debug.ScriptBreakPointType.ScriptId,
                                  script_id, opt_line, opt_column,
                                  opt_condition, opt_groupId,
                                  opt_position_alignment);
};


Debug.setScriptBreakPointByName = function(script_name,
                                           opt_line, opt_column,
                                           opt_condition, opt_groupId) {
  return this.setScriptBreakPoint(Debug.ScriptBreakPointType.ScriptName,
                                  script_name, opt_line, opt_column,
                                  opt_condition, opt_groupId);
};


Debug.setScriptBreakPointByRegExp = function(script_regexp,
                                             opt_line, opt_column,
                                             opt_condition, opt_groupId) {
  return this.setScriptBreakPoint(Debug.ScriptBreakPointType.ScriptRegExp,
                                  script_regexp, opt_line, opt_column,
                                  opt_condition, opt_groupId);
};


Debug.enableScriptBreakPoint = function(break_point_number) {
  var script_break_point = this.findScriptBreakPoint(break_point_number, false);
  script_break_point.enable();
};


Debug.disableScriptBreakPoint = function(break_point_number) {
  var script_break_point = this.findScriptBreakPoint(break_point_number, false);
  script_break_point.disable();
};


Debug.changeScriptBreakPointCondition = function(
    break_point_number, condition) {
  var script_break_point = this.findScriptBreakPoint(break_point_number, false);
  script_break_point.setCondition(condition);
};


Debug.scriptBreakPoints = function() {
  return script_break_points;
};


Debug.clearStepping = function() {
  %ClearStepping();
};

Debug.setBreakOnException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Caught, true);
};

Debug.clearBreakOnException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Caught, false);
};

Debug.isBreakOnException = function() {
  return !!%IsBreakOnException(Debug.ExceptionBreak.Caught);
};

Debug.setBreakOnUncaughtException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Uncaught, true);
};

Debug.clearBreakOnUncaughtException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Uncaught, false);
};

Debug.isBreakOnUncaughtException = function() {
  return !!%IsBreakOnException(Debug.ExceptionBreak.Uncaught);
};

Debug.showBreakPoints = function(f, full, opt_position_alignment) {
  if (!IS_FUNCTION(f)) throw %make_error(kDebuggerType);
  var source = full ? this.scriptSource(f) : this.source(f);
  var offset = full ? 0 : this.sourcePosition(f);
  var position_alignment = IS_UNDEFINED(opt_position_alignment)
      ? Debug.BreakPositionAlignment.Statement : opt_position_alignment;
  var locations = %GetBreakLocations(f, position_alignment);
  if (!locations) return source;
  locations.sort(function(x, y) { return x - y; });
  var result = "";
  var prev_pos = 0;
  var pos;
  for (var i = 0; i < locations.length; i++) {
    pos = locations[i] - offset;
    result += source.slice(prev_pos, pos);
    result += "[B" + i + "]";
    prev_pos = pos;
  }
  pos = source.length;
  result += source.substring(prev_pos, pos);
  return result;
};


// Get all the scripts currently loaded. Locating all the scripts is based on
// scanning the heap.
Debug.scripts = function() {
  // Collect all scripts in the heap.
  return %DebugGetLoadedScripts();
};


// Get a specific script currently loaded. This is based on scanning the heap.
// TODO(clemensh): Create a runtime function for this.
function scriptById(scriptId) {
  var scripts = Debug.scripts();
  for (var script of scripts) {
    if (script.id == scriptId) return script;
  }
  return UNDEFINED;
};


Debug.debuggerFlags = function() {
  return debugger_flags;
};

Debug.MakeMirror = MakeMirror;

function MakeExecutionState(break_id) {
  return new ExecutionState(break_id);
}

function ExecutionState(break_id) {
  this.break_id = break_id;
  this.selected_frame = 0;
}

ExecutionState.prototype.prepareStep = function(action) {
  if (action === Debug.StepAction.StepIn ||
      action === Debug.StepAction.StepOut ||
      action === Debug.StepAction.StepNext ||
      action === Debug.StepAction.StepFrame) {
    return %PrepareStep(this.break_id, action);
  }
  throw %make_type_error(kDebuggerType);
};

ExecutionState.prototype.evaluateGlobal = function(source) {
  return MakeMirror(%DebugEvaluateGlobal(this.break_id, source));
};

ExecutionState.prototype.frameCount = function() {
  return %GetFrameCount(this.break_id);
};

ExecutionState.prototype.frame = function(opt_index) {
  // If no index supplied return the selected frame.
  if (opt_index == null) opt_index = this.selected_frame;
  if (opt_index < 0 || opt_index >= this.frameCount()) {
    throw %make_type_error(kDebuggerFrame);
  }
  return new FrameMirror(this.break_id, opt_index);
};

ExecutionState.prototype.setSelectedFrame = function(index) {
  var i = TO_NUMBER(index);
  if (i < 0 || i >= this.frameCount()) {
    throw %make_type_error(kDebuggerFrame);
  }
  this.selected_frame = i;
};

ExecutionState.prototype.selectedFrame = function() {
  return this.selected_frame;
};

ExecutionState.prototype.debugCommandProcessor = function(opt_is_running) {
  return new DebugCommandProcessor(this, opt_is_running);
};


function MakeBreakEvent(break_id, break_points_hit) {
  return new BreakEvent(break_id, break_points_hit);
}


function BreakEvent(break_id, break_points_hit) {
  this.frame_ = new FrameMirror(break_id, 0);
  this.break_points_hit_ = break_points_hit;
}


BreakEvent.prototype.eventType = function() {
  return Debug.DebugEvent.Break;
};


BreakEvent.prototype.func = function() {
  return this.frame_.func();
};


BreakEvent.prototype.sourceLine = function() {
  return this.frame_.sourceLine();
};


BreakEvent.prototype.sourceColumn = function() {
  return this.frame_.sourceColumn();
};


BreakEvent.prototype.sourceLineText = function() {
  return this.frame_.sourceLineText();
};


BreakEvent.prototype.breakPointsHit = function() {
  return this.break_points_hit_;
};


BreakEvent.prototype.toJSONProtocol = function() {
  var o = { seq: next_response_seq++,
            type: "event",
            event: "break",
            body: { invocationText: this.frame_.invocationText() }
          };

  // Add script related information to the event if available.
  var script = this.func().script();
  if (script) {
    o.body.sourceLine = this.sourceLine(),
    o.body.sourceColumn = this.sourceColumn(),
    o.body.sourceLineText = this.sourceLineText(),
    o.body.script = MakeScriptObject_(script, false);
  }

  // Add an Array of break points hit if any.
  if (this.breakPointsHit()) {
    o.body.breakpoints = [];
    for (var i = 0; i < this.breakPointsHit().length; i++) {
      // Find the break point number. For break points originating from a
      // script break point supply the script break point number.
      var breakpoint = this.breakPointsHit()[i];
      var script_break_point = breakpoint.script_break_point();
      var number;
      if (script_break_point) {
        number = script_break_point.number();
      } else {
        number = breakpoint.number();
      }
      o.body.breakpoints.push(number);
    }
  }
  return JSONStringify(ObjectToProtocolObject_(o));
};


function MakeExceptionEvent(break_id, exception, uncaught, promise) {
  return new ExceptionEvent(break_id, exception, uncaught, promise);
}


function ExceptionEvent(break_id, exception, uncaught, promise) {
  this.exec_state_ = new ExecutionState(break_id);
  this.exception_ = exception;
  this.uncaught_ = uncaught;
  this.promise_ = promise;
}


ExceptionEvent.prototype.eventType = function() {
  return Debug.DebugEvent.Exception;
};


ExceptionEvent.prototype.exception = function() {
  return this.exception_;
};


ExceptionEvent.prototype.uncaught = function() {
  return this.uncaught_;
};


ExceptionEvent.prototype.promise = function() {
  return this.promise_;
};


ExceptionEvent.prototype.func = function() {
  return this.exec_state_.frame(0).func();
};


ExceptionEvent.prototype.sourceLine = function() {
  return this.exec_state_.frame(0).sourceLine();
};


ExceptionEvent.prototype.sourceColumn = function() {
  return this.exec_state_.frame(0).sourceColumn();
};


ExceptionEvent.prototype.sourceLineText = function() {
  return this.exec_state_.frame(0).sourceLineText();
};


ExceptionEvent.prototype.toJSONProtocol = function() {
  var o = new ProtocolMessage();
  o.event = "exception";
  o.body = { uncaught: this.uncaught_,
             exception: MakeMirror(this.exception_)
           };

  // Exceptions might happen whithout any JavaScript frames.
  if (this.exec_state_.frameCount() > 0) {
    o.body.sourceLine = this.sourceLine();
    o.body.sourceColumn = this.sourceColumn();
    o.body.sourceLineText = this.sourceLineText();

    // Add script information to the event if available.
    var script = this.func().script();
    if (script) {
      o.body.script = MakeScriptObject_(script, false);
    }
  } else {
    o.body.sourceLine = -1;
  }

  return o.toJSONProtocol();
};


function MakeCompileEvent(script, type) {
  return new CompileEvent(script, type);
}


function CompileEvent(script, type) {
  this.script_ = MakeMirror(script);
  this.type_ = type;
}


CompileEvent.prototype.eventType = function() {
  return this.type_;
};


CompileEvent.prototype.script = function() {
  return this.script_;
};


CompileEvent.prototype.toJSONProtocol = function() {
  var o = new ProtocolMessage();
  o.running = true;
  switch (this.type_) {
    case Debug.DebugEvent.BeforeCompile:
      o.event = "beforeCompile";
      break;
    case Debug.DebugEvent.AfterCompile:
      o.event = "afterCompile";
      break;
    case Debug.DebugEvent.CompileError:
      o.event = "compileError";
      break;
  }
  o.body = {};
  o.body.script = this.script_;

  return o.toJSONProtocol();
};


function MakeScriptObject_(script, include_source) {
  var o = { id: script.id(),
            name: script.name(),
            lineOffset: script.lineOffset(),
            columnOffset: script.columnOffset(),
            lineCount: script.lineCount(),
          };
  if (!IS_UNDEFINED(script.data())) {
    o.data = script.data();
  }
  if (include_source) {
    o.source = script.source();
  }
  return o;
}


function MakeAsyncTaskEvent(type, id) {
  return new AsyncTaskEvent(type, id);
}


function AsyncTaskEvent(type, id) {
  this.type_ = type;
  this.id_ = id;
}


AsyncTaskEvent.prototype.type = function() {
  return this.type_;
}


AsyncTaskEvent.prototype.id = function() {
  return this.id_;
}


function DebugCommandProcessor(exec_state, opt_is_running) {
  this.exec_state_ = exec_state;
  this.running_ = opt_is_running || false;
}


DebugCommandProcessor.prototype.processDebugRequest = function (request) {
  return this.processDebugJSONRequest(request);
};


function ProtocolMessage(request) {
  // Update sequence number.
  this.seq = next_response_seq++;

  if (request) {
    // If message is based on a request this is a response. Fill the initial
    // response from the request.
    this.type = 'response';
    this.request_seq = request.seq;
    this.command = request.command;
  } else {
    // If message is not based on a request it is a dabugger generated event.
    this.type = 'event';
  }
  this.success = true;
  // Handler may set this field to control debugger state.
  this.running = UNDEFINED;
}


ProtocolMessage.prototype.setOption = function(name, value) {
  if (!this.options_) {
    this.options_ = {};
  }
  this.options_[name] = value;
};


ProtocolMessage.prototype.failed = function(message, opt_details) {
  this.success = false;
  this.message = message;
  if (IS_OBJECT(opt_details)) {
    this.error_details = opt_details;
  }
};


ProtocolMessage.prototype.toJSONProtocol = function() {
  // Encode the protocol header.
  var json = {};
  json.seq= this.seq;
  if (this.request_seq) {
    json.request_seq = this.request_seq;
  }
  json.type = this.type;
  if (this.event) {
    json.event = this.event;
  }
  if (this.command) {
    json.command = this.command;
  }
  if (this.success) {
    json.success = this.success;
  } else {
    json.success = false;
  }
  if (this.body) {
    // Encode the body part.
    var bodyJson;
    var serializer = MakeMirrorSerializer(true, this.options_);
    if (this.body instanceof Mirror) {
      bodyJson = serializer.serializeValue(this.body);
    } else if (this.body instanceof GlobalArray) {
      bodyJson = [];
      for (var i = 0; i < this.body.length; i++) {
        if (this.body[i] instanceof Mirror) {
          bodyJson.push(serializer.serializeValue(this.body[i]));
        } else {
          bodyJson.push(ObjectToProtocolObject_(this.body[i], serializer));
        }
      }
    } else {
      bodyJson = ObjectToProtocolObject_(this.body, serializer);
    }
    json.body = bodyJson;
    json.refs = serializer.serializeReferencedObjects();
  }
  if (this.message) {
    json.message = this.message;
  }
  if (this.error_details) {
    json.error_details = this.error_details;
  }
  json.running = this.running;
  return JSONStringify(json);
};


DebugCommandProcessor.prototype.createResponse = function(request) {
  return new ProtocolMessage(request);
};


DebugCommandProcessor.prototype.processDebugJSONRequest = function(
    json_request) {
  var request;  // Current request.
  var response;  // Generated response.
  try {
    try {
      // Convert the JSON string to an object.
      request = JSONParse(json_request);

      // Create an initial response.
      response = this.createResponse(request);

      if (!request.type) {
        throw %make_error(kDebugger, 'Type not specified');
      }

      if (request.type != 'request') {
        throw %make_error(kDebugger,
                        "Illegal type '" + request.type + "' in request");
      }

      if (!request.command) {
        throw %make_error(kDebugger, 'Command not specified');
      }

      if (request.arguments) {
        var args = request.arguments;
        // TODO(yurys): remove request.arguments.compactFormat check once
        // ChromeDevTools are switched to 'inlineRefs'
        if (args.inlineRefs || args.compactFormat) {
          response.setOption('inlineRefs', true);
        }
        if (!IS_UNDEFINED(args.maxStringLength)) {
          response.setOption('maxStringLength', args.maxStringLength);
        }
      }

      var key = request.command.toLowerCase();
      var handler = DebugCommandProcessor.prototype.dispatch_[key];
      if (IS_FUNCTION(handler)) {
        %_Call(handler, this, request, response);
      } else {
        throw %make_error(kDebugger,
                        'Unknown command "' + request.command + '" in request');
      }
    } catch (e) {
      // If there is no response object created one (without command).
      if (!response) {
        response = this.createResponse();
      }
      response.success = false;
      response.message = TO_STRING(e);
    }

    // Return the response as a JSON encoded string.
    try {
      if (!IS_UNDEFINED(response.running)) {
        // Response controls running state.
        this.running_ = response.running;
      }
      response.running = this.running_;
      return response.toJSONProtocol();
    } catch (e) {
      // Failed to generate response - return generic error.
      return '{"seq":' + response.seq + ',' +
              '"request_seq":' + request.seq + ',' +
              '"type":"response",' +
              '"success":false,' +
              '"message":"Internal error: ' + TO_STRING(e) + '"}';
    }
  } catch (e) {
    // Failed in one of the catch blocks above - most generic error.
    return '{"seq":0,"type":"response","success":false,"message":"Internal error"}';
  }
};


DebugCommandProcessor.prototype.continueRequest_ = function(request, response) {
  // Check for arguments for continue.
  if (request.arguments) {
    var action = Debug.StepAction.StepIn;

    // Pull out arguments.
    var stepaction = request.arguments.stepaction;

    // Get the stepaction argument.
    if (stepaction) {
      if (stepaction == 'in') {
        action = Debug.StepAction.StepIn;
      } else if (stepaction == 'next') {
        action = Debug.StepAction.StepNext;
      } else if (stepaction == 'out') {
        action = Debug.StepAction.StepOut;
      } else {
        throw %make_error(kDebugger,
                        'Invalid stepaction argument "' + stepaction + '".');
      }
    }

    // Set up the VM for stepping.
    this.exec_state_.prepareStep(action);
  }

  // VM should be running after executing this request.
  response.running = true;
};


DebugCommandProcessor.prototype.breakRequest_ = function(request, response) {
  // Ignore as break command does not do anything when broken.
};


DebugCommandProcessor.prototype.setBreakPointRequest_ =
    function(request, response) {
  // Check for legal request.
  if (!request.arguments) {
    response.failed('Missing arguments');
    return;
  }

  // Pull out arguments.
  var type = request.arguments.type;
  var target = request.arguments.target;
  var line = request.arguments.line;
  var column = request.arguments.column;
  var enabled = IS_UNDEFINED(request.arguments.enabled) ?
      true : request.arguments.enabled;
  var condition = request.arguments.condition;
  var groupId = request.arguments.groupId;

  // Check for legal arguments.
  if (!type || IS_UNDEFINED(target)) {
    response.failed('Missing argument "type" or "target"');
    return;
  }

  // Either function or script break point.
  var break_point_number;
  if (type == 'function') {
    // Handle function break point.
    if (!IS_STRING(target)) {
      response.failed('Argument "target" is not a string value');
      return;
    }
    var f;
    try {
      // Find the function through a global evaluate.
      f = this.exec_state_.evaluateGlobal(target).value();
    } catch (e) {
      response.failed('Error: "' + TO_STRING(e) +
                      '" evaluating "' + target + '"');
      return;
    }
    if (!IS_FUNCTION(f)) {
      response.failed('"' + target + '" does not evaluate to a function');
      return;
    }

    // Set function break point.
    break_point_number = Debug.setBreakPoint(f, line, column, condition);
  } else if (type == 'handle') {
    // Find the object pointed by the specified handle.
    var handle = ParseInt(target, 10);
    var mirror = LookupMirror(handle);
    if (!mirror) {
      return response.failed('Object #' + handle + '# not found');
    }
    if (!mirror.isFunction()) {
      return response.failed('Object #' + handle + '# is not a function');
    }

    // Set function break point.
    break_point_number = Debug.setBreakPoint(mirror.value(),
                                             line, column, condition);
  } else if (type == 'script') {
    // set script break point.
    break_point_number =
        Debug.setScriptBreakPointByName(target, line, column, condition,
                                        groupId);
  } else if (type == 'scriptId') {
    break_point_number =
        Debug.setScriptBreakPointById(target, line, column, condition, groupId);
  } else if (type == 'scriptRegExp') {
    break_point_number =
        Debug.setScriptBreakPointByRegExp(target, line, column, condition,
                                          groupId);
  } else {
    response.failed('Illegal type "' + type + '"');
    return;
  }

  // Set additional break point properties.
  var break_point = Debug.findBreakPoint(break_point_number);
  if (!enabled) {
    Debug.disableBreakPoint(break_point_number);
  }

  // Add the break point number to the response.
  response.body = { type: type,
                    breakpoint: break_point_number };

  // Add break point information to the response.
  if (break_point instanceof ScriptBreakPoint) {
    if (break_point.type() == Debug.ScriptBreakPointType.ScriptId) {
      response.body.type = 'scriptId';
      response.body.script_id = break_point.script_id();
    } else if (break_point.type() == Debug.ScriptBreakPointType.ScriptName) {
      response.body.type = 'scriptName';
      response.body.script_name = break_point.script_name();
    } else if (break_point.type() == Debug.ScriptBreakPointType.ScriptRegExp) {
      response.body.type = 'scriptRegExp';
      response.body.script_regexp = break_point.script_regexp_object().source;
    } else {
      throw %make_error(kDebugger,
                      "Unexpected breakpoint type: " + break_point.type());
    }
    response.body.line = break_point.line();
    response.body.column = break_point.column();
    response.body.actual_locations = break_point.actual_locations();
  } else {
    response.body.type = 'function';
    response.body.actual_locations = [break_point.actual_location];
  }
};


DebugCommandProcessor.prototype.changeBreakPointRequest_ = function(
    request, response) {
  // Check for legal request.
  if (!request.arguments) {
    response.failed('Missing arguments');
    return;
  }

  // Pull out arguments.
  var break_point = TO_NUMBER(request.arguments.breakpoint);
  var enabled = request.arguments.enabled;
  var condition = request.arguments.condition;

  // Check for legal arguments.
  if (!break_point) {
    response.failed('Missing argument "breakpoint"');
    return;
  }

  // Change enabled state if supplied.
  if (!IS_UNDEFINED(enabled)) {
    if (enabled) {
      Debug.enableBreakPoint(break_point);
    } else {
      Debug.disableBreakPoint(break_point);
    }
  }

  // Change condition if supplied
  if (!IS_UNDEFINED(condition)) {
    Debug.changeBreakPointCondition(break_point, condition);
  }
};


DebugCommandProcessor.prototype.clearBreakPointGroupRequest_ = function(
    request, response) {
  // Check for legal request.
  if (!request.arguments) {
    response.failed('Missing arguments');
    return;
  }

  // Pull out arguments.
  var group_id = request.arguments.groupId;

  // Check for legal arguments.
  if (!group_id) {
    response.failed('Missing argument "groupId"');
    return;
  }

  var cleared_break_points = [];
  var new_script_break_points = [];
  for (var i = 0; i < script_break_points.length; i++) {
    var next_break_point = script_break_points[i];
    if (next_break_point.groupId() == group_id) {
      cleared_break_points.push(next_break_point.number());
      next_break_point.clear();
    } else {
      new_script_break_points.push(next_break_point);
    }
  }
  script_break_points = new_script_break_points;

  // Add the cleared break point numbers to the response.
  response.body = { breakpoints: cleared_break_points };
};


DebugCommandProcessor.prototype.clearBreakPointRequest_ = function(
    request, response) {
  // Check for legal request.
  if (!request.arguments) {
    response.failed('Missing arguments');
    return;
  }

  // Pull out arguments.
  var break_point = TO_NUMBER(request.arguments.breakpoint);

  // Check for legal arguments.
  if (!break_point) {
    response.failed('Missing argument "breakpoint"');
    return;
  }

  // Clear break point.
  Debug.clearBreakPoint(break_point);

  // Add the cleared break point number to the response.
  response.body = { breakpoint: break_point };
};


DebugCommandProcessor.prototype.listBreakpointsRequest_ = function(
    request, response) {
  var array = [];
  for (var i = 0; i < script_break_points.length; i++) {
    var break_point = script_break_points[i];

    var description = {
      number: break_point.number(),
      line: break_point.line(),
      column: break_point.column(),
      groupId: break_point.groupId(),
      active: break_point.active(),
      condition: break_point.condition(),
      actual_locations: break_point.actual_locations()
    };

    if (break_point.type() == Debug.ScriptBreakPointType.ScriptId) {
      description.type = 'scriptId';
      description.script_id = break_point.script_id();
    } else if (break_point.type() == Debug.ScriptBreakPointType.ScriptName) {
      description.type = 'scriptName';
      description.script_name = break_point.script_name();
    } else if (break_point.type() == Debug.ScriptBreakPointType.ScriptRegExp) {
      description.type = 'scriptRegExp';
      description.script_regexp = break_point.script_regexp_object().source;
    } else {
      throw %make_error(kDebugger,
                      "Unexpected breakpoint type: " + break_point.type());
    }
    array.push(description);
  }

  response.body = {
    breakpoints: array,
    breakOnExceptions: Debug.isBreakOnException(),
    breakOnUncaughtExceptions: Debug.isBreakOnUncaughtException()
  };
};


DebugCommandProcessor.prototype.disconnectRequest_ =
    function(request, response) {
  Debug.disableAllBreakPoints();
  this.continueRequest_(request, response);
};


DebugCommandProcessor.prototype.setExceptionBreakRequest_ =
    function(request, response) {
  // Check for legal request.
  if (!request.arguments) {
    response.failed('Missing arguments');
    return;
  }

  // Pull out and check the 'type' argument:
  var type = request.arguments.type;
  if (!type) {
    response.failed('Missing argument "type"');
    return;
  }

  // Initialize the default value of enable:
  var enabled;
  if (type == 'all') {
    enabled = !Debug.isBreakOnException();
  } else if (type == 'uncaught') {
    enabled = !Debug.isBreakOnUncaughtException();
  }

  // Pull out and check the 'enabled' argument if present:
  if (!IS_UNDEFINED(request.arguments.enabled)) {
    enabled = request.arguments.enabled;
    if ((enabled != true) && (enabled != false)) {
      response.failed('Illegal value for "enabled":"' + enabled + '"');
    }
  }

  // Now set the exception break state:
  if (type == 'all') {
    %ChangeBreakOnException(Debug.ExceptionBreak.Caught, enabled);
  } else if (type == 'uncaught') {
    %ChangeBreakOnException(Debug.ExceptionBreak.Uncaught, enabled);
  } else {
    response.failed('Unknown "type":"' + type + '"');
  }

  // Add the cleared break point number to the response.
  response.body = { 'type': type, 'enabled': enabled };
};


DebugCommandProcessor.prototype.backtraceRequest_ = function(
    request, response) {
  // Get the number of frames.
  var total_frames = this.exec_state_.frameCount();

  // Create simple response if there are no frames.
  if (total_frames == 0) {
    response.body = {
      totalFrames: total_frames
    };
    return;
  }

  // Default frame range to include in backtrace.
  var from_index = 0;
  var to_index = kDefaultBacktraceLength;

  // Get the range from the arguments.
  if (request.arguments) {
    if (request.arguments.fromFrame) {
      from_index = request.arguments.fromFrame;
    }
    if (request.arguments.toFrame) {
      to_index = request.arguments.toFrame;
    }
    if (request.arguments.bottom) {
      var tmp_index = total_frames - from_index;
      from_index = total_frames - to_index;
      to_index = tmp_index;
    }
    if (from_index < 0 || to_index < 0) {
      return response.failed('Invalid frame number');
    }
  }

  // Adjust the index.
  to_index = MathMin(total_frames, to_index);

  if (to_index <= from_index) {
    var error = 'Invalid frame range';
    return response.failed(error);
  }

  // Create the response body.
  var frames = [];
  for (var i = from_index; i < to_index; i++) {
    frames.push(this.exec_state_.frame(i));
  }
  response.body = {
    fromFrame: from_index,
    toFrame: to_index,
    totalFrames: total_frames,
    frames: frames
  };
};


DebugCommandProcessor.prototype.frameRequest_ = function(request, response) {
  // No frames no source.
  if (this.exec_state_.frameCount() == 0) {
    return response.failed('No frames');
  }

  // With no arguments just keep the selected frame.
  if (request.arguments) {
    var index = request.arguments.number;
    if (index < 0 || this.exec_state_.frameCount() <= index) {
      return response.failed('Invalid frame number');
    }

    this.exec_state_.setSelectedFrame(request.arguments.number);
  }
  response.body = this.exec_state_.frame();
};


DebugCommandProcessor.prototype.resolveFrameFromScopeDescription_ =
    function(scope_description) {
  // Get the frame for which the scope or scopes are requested.
  // With no frameNumber argument use the currently selected frame.
  if (scope_description && !IS_UNDEFINED(scope_description.frameNumber)) {
    var frame_index = scope_description.frameNumber;
    if (frame_index < 0 || this.exec_state_.frameCount() <= frame_index) {
      throw %make_type_error(kDebuggerFrame);
    }
    return this.exec_state_.frame(frame_index);
  } else {
    return this.exec_state_.frame();
  }
};


// Gets scope host object from request. It is either a function
// ('functionHandle' argument must be specified) or a stack frame
// ('frameNumber' may be specified and the current frame is taken by default).
DebugCommandProcessor.prototype.resolveScopeHolder_ =
    function(scope_description) {
  if (scope_description && "functionHandle" in scope_description) {
    if (!IS_NUMBER(scope_description.functionHandle)) {
      throw %make_error(kDebugger, 'Function handle must be a number');
    }
    var function_mirror = LookupMirror(scope_description.functionHandle);
    if (!function_mirror) {
      throw %make_error(kDebugger, 'Failed to find function object by handle');
    }
    if (!function_mirror.isFunction()) {
      throw %make_error(kDebugger,
                      'Value of non-function type is found by handle');
    }
    return function_mirror;
  } else {
    // No frames no scopes.
    if (this.exec_state_.frameCount() == 0) {
      throw %make_error(kDebugger, 'No scopes');
    }

    // Get the frame for which the scopes are requested.
    var frame = this.resolveFrameFromScopeDescription_(scope_description);
    return frame;
  }
}


DebugCommandProcessor.prototype.scopesRequest_ = function(request, response) {
  var scope_holder = this.resolveScopeHolder_(request.arguments);

  // Fill all scopes for this frame or function.
  var total_scopes = scope_holder.scopeCount();
  var scopes = [];
  for (var i = 0; i < total_scopes; i++) {
    scopes.push(scope_holder.scope(i));
  }
  response.body = {
    fromScope: 0,
    toScope: total_scopes,
    totalScopes: total_scopes,
    scopes: scopes
  };
};


DebugCommandProcessor.prototype.scopeRequest_ = function(request, response) {
  // Get the frame or function for which the scope is requested.
  var scope_holder = this.resolveScopeHolder_(request.arguments);

  // With no scope argument just return top scope.
  var scope_index = 0;
  if (request.arguments && !IS_UNDEFINED(request.arguments.number)) {
    scope_index = TO_NUMBER(request.arguments.number);
    if (scope_index < 0 || scope_holder.scopeCount() <= scope_index) {
      return response.failed('Invalid scope number');
    }
  }

  response.body = scope_holder.scope(scope_index);
};


// Reads value from protocol description. Description may be in form of type
// (for singletons), raw value (primitive types supported in JSON),
// string value description plus type (for primitive values) or handle id.
// Returns raw value or throws exception.
DebugCommandProcessor.resolveValue_ = function(value_description) {
  if ("handle" in value_description) {
    var value_mirror = LookupMirror(value_description.handle);
    if (!value_mirror) {
      throw %make_error(kDebugger, "Failed to resolve value by handle, ' #" +
                                 value_description.handle + "# not found");
    }
    return value_mirror.value();
  } else if ("stringDescription" in value_description) {
    if (value_description.type == MirrorType.BOOLEAN_TYPE) {
      return TO_BOOLEAN(value_description.stringDescription);
    } else if (value_description.type == MirrorType.NUMBER_TYPE) {
      return TO_NUMBER(value_description.stringDescription);
    } if (value_description.type == MirrorType.STRING_TYPE) {
      return TO_STRING(value_description.stringDescription);
    } else {
      throw %make_error(kDebugger, "Unknown type");
    }
  } else if ("value" in value_description) {
    return value_description.value;
  } else if (value_description.type == MirrorType.UNDEFINED_TYPE) {
    return UNDEFINED;
  } else if (value_description.type == MirrorType.NULL_TYPE) {
    return null;
  } else {
    throw %make_error(kDebugger, "Failed to parse value description");
  }
};


DebugCommandProcessor.prototype.setVariableValueRequest_ =
    function(request, response) {
  if (!request.arguments) {
    response.failed('Missing arguments');
    return;
  }

  if (IS_UNDEFINED(request.arguments.name)) {
    response.failed('Missing variable name');
  }
  var variable_name = request.arguments.name;

  var scope_description = request.arguments.scope;

  // Get the frame or function for which the scope is requested.
  var scope_holder = this.resolveScopeHolder_(scope_description);

  if (IS_UNDEFINED(scope_description.number)) {
    response.failed('Missing scope number');
  }
  var scope_index = TO_NUMBER(scope_description.number);

  var scope = scope_holder.scope(scope_index);

  var new_value =
      DebugCommandProcessor.resolveValue_(request.arguments.newValue);

  scope.setVariableValue(variable_name, new_value);

  var new_value_mirror = MakeMirror(new_value);

  response.body = {
    newValue: new_value_mirror
  };
};


DebugCommandProcessor.prototype.evaluateRequest_ = function(request, response) {
  if (!request.arguments) {
    return response.failed('Missing arguments');
  }

  // Pull out arguments.
  var expression = request.arguments.expression;
  var frame = request.arguments.frame;
  var global = request.arguments.global;

  // The expression argument could be an integer so we convert it to a
  // string.
  try {
    expression = TO_STRING(expression);
  } catch(e) {
    return response.failed('Failed to convert expression argument to string');
  }

  // Check for legal arguments.
  if (!IS_UNDEFINED(frame) && global) {
    return response.failed('Arguments "frame" and "global" are exclusive');
  }

  // Global evaluate.
  if (global) {
    // Evaluate in the native context.
    response.body = this.exec_state_.evaluateGlobal(expression);
    return;
  }

  // No frames no evaluate in frame.
  if (this.exec_state_.frameCount() == 0) {
    return response.failed('No frames');
  }

  // Check whether a frame was specified.
  if (!IS_UNDEFINED(frame)) {
    var frame_number = TO_NUMBER(frame);
    if (frame_number < 0 || frame_number >= this.exec_state_.frameCount()) {
      return response.failed('Invalid frame "' + frame + '"');
    }
    // Evaluate in the specified frame.
    response.body = this.exec_state_.frame(frame_number).evaluate(expression);
    return;
  } else {
    // Evaluate in the selected frame.
    response.body = this.exec_state_.frame().evaluate(expression);
    return;
  }
};


DebugCommandProcessor.prototype.lookupRequest_ = function(request, response) {
  if (!request.arguments) {
    return response.failed('Missing arguments');
  }

  // Pull out arguments.
  var handles = request.arguments.handles;

  // Check for legal arguments.
  if (IS_UNDEFINED(handles)) {
    return response.failed('Argument "handles" missing');
  }

  // Set 'includeSource' option for script lookup.
  if (!IS_UNDEFINED(request.arguments.includeSource)) {
    var includeSource = TO_BOOLEAN(request.arguments.includeSource);
    response.setOption('includeSource', includeSource);
  }

  // Lookup handles.
  var mirrors = {};
  for (var i = 0; i < handles.length; i++) {
    var handle = handles[i];
    var mirror = LookupMirror(handle);
    if (!mirror) {
      return response.failed('Object #' + handle + '# not found');
    }
    mirrors[handle] = mirror;
  }
  response.body = mirrors;
};


DebugCommandProcessor.prototype.referencesRequest_ =
    function(request, response) {
  if (!request.arguments) {
    return response.failed('Missing arguments');
  }

  // Pull out arguments.
  var type = request.arguments.type;
  var handle = request.arguments.handle;

  // Check for legal arguments.
  if (IS_UNDEFINED(type)) {
    return response.failed('Argument "type" missing');
  }
  if (IS_UNDEFINED(handle)) {
    return response.failed('Argument "handle" missing');
  }
  if (type != 'referencedBy' && type != 'constructedBy') {
    return response.failed('Invalid type "' + type + '"');
  }

  // Lookup handle and return objects with references the object.
  var mirror = LookupMirror(handle);
  if (mirror) {
    if (type == 'referencedBy') {
      response.body = mirror.referencedBy();
    } else {
      response.body = mirror.constructedBy();
    }
  } else {
    return response.failed('Object #' + handle + '# not found');
  }
};


DebugCommandProcessor.prototype.sourceRequest_ = function(request, response) {
  // No frames no source.
  if (this.exec_state_.frameCount() == 0) {
    return response.failed('No source');
  }

  var from_line;
  var to_line;
  var frame = this.exec_state_.frame();
  if (request.arguments) {
    // Pull out arguments.
    from_line = request.arguments.fromLine;
    to_line = request.arguments.toLine;

    if (!IS_UNDEFINED(request.arguments.frame)) {
      var frame_number = TO_NUMBER(request.arguments.frame);
      if (frame_number < 0 || frame_number >= this.exec_state_.frameCount()) {
        return response.failed('Invalid frame "' + frame + '"');
      }
      frame = this.exec_state_.frame(frame_number);
    }
  }

  // Get the script selected.
  var script = frame.func().script();
  if (!script) {
    return response.failed('No source');
  }

  var raw_script = script.value();

  // Sanitize arguments and remove line offset.
  var line_offset = raw_script.line_offset;
  var line_count = %ScriptLineCount(raw_script);
  from_line = IS_UNDEFINED(from_line) ? 0 : from_line - line_offset;
  to_line = IS_UNDEFINED(to_line) ? line_count : to_line - line_offset;

  if (from_line < 0) from_line = 0;
  if (to_line > line_count) to_line = line_count;

  if (from_line >= line_count || to_line < 0 || from_line > to_line) {
    return response.failed('Invalid line interval');
  }

  // Fill in the response.

  response.body = {};
  response.body.fromLine = from_line + line_offset;
  response.body.toLine = to_line + line_offset;
  response.body.fromPosition = %ScriptLineStartPosition(raw_script, from_line);
  response.body.toPosition =
    (to_line == 0) ? 0 : %ScriptLineEndPosition(raw_script, to_line - 1);
  response.body.totalLines = %ScriptLineCount(raw_script);

  response.body.source = %_SubString(raw_script.source,
                                     response.body.fromPosition,
                                     response.body.toPosition);
};


DebugCommandProcessor.prototype.scriptsRequest_ = function(request, response) {
  var types = ScriptTypeFlag(Debug.ScriptType.Normal);
  var includeSource = false;
  var idsToInclude = null;
  if (request.arguments) {
    // Pull out arguments.
    if (!IS_UNDEFINED(request.arguments.types)) {
      types = TO_NUMBER(request.arguments.types);
      if (IsNaN(types) || types < 0) {
        return response.failed('Invalid types "' +
                               request.arguments.types + '"');
      }
    }

    if (!IS_UNDEFINED(request.arguments.includeSource)) {
      includeSource = TO_BOOLEAN(request.arguments.includeSource);
      response.setOption('includeSource', includeSource);
    }

    if (IS_ARRAY(request.arguments.ids)) {
      idsToInclude = {};
      var ids = request.arguments.ids;
      for (var i = 0; i < ids.length; i++) {
        idsToInclude[ids[i]] = true;
      }
    }

    var filterStr = null;
    var filterNum = null;
    if (!IS_UNDEFINED(request.arguments.filter)) {
      var num = TO_NUMBER(request.arguments.filter);
      if (!IsNaN(num)) {
        filterNum = num;
      }
      filterStr = request.arguments.filter;
    }
  }

  // Collect all scripts in the heap.
  var scripts = Debug.scripts();

  response.body = [];

  for (var i = 0; i < scripts.length; i++) {
    if (idsToInclude && !idsToInclude[scripts[i].id]) {
      continue;
    }
    if (filterStr || filterNum) {
      var script = scripts[i];
      var found = false;
      if (filterNum && !found) {
        if (script.id && script.id === filterNum) {
          found = true;
        }
      }
      if (filterStr && !found) {
        if (script.name && script.name.indexOf(filterStr) >= 0) {
          found = true;
        }
      }
      if (!found) continue;
    }
    if (types & ScriptTypeFlag(scripts[i].type)) {
      response.body.push(MakeMirror(scripts[i]));
    }
  }
};


DebugCommandProcessor.prototype.suspendRequest_ = function(request, response) {
  response.running = false;
};


// TODO(5510): remove this.
DebugCommandProcessor.prototype.versionRequest_ = function(request, response) {
  response.body = {
    V8Version: %GetV8Version()
  };
};


DebugCommandProcessor.prototype.changeLiveRequest_ = function(
    request, response) {
  if (!request.arguments) {
    return response.failed('Missing arguments');
  }
  var script_id = request.arguments.script_id;
  var preview_only = !!request.arguments.preview_only;

  var the_script = scriptById(script_id);
  if (!the_script) {
    response.failed('Script not found');
    return;
  }

  var change_log = new GlobalArray();

  if (!IS_STRING(request.arguments.new_source)) {
    throw "new_source argument expected";
  }

  var new_source = request.arguments.new_source;

  var result_description;
  try {
    result_description = Debug.LiveEdit.SetScriptSource(the_script,
        new_source, preview_only, change_log);
  } catch (e) {
    if (e instanceof Debug.LiveEdit.Failure && "details" in e) {
      response.failed(e.message, e.details);
      return;
    }
    throw e;
  }
  response.body = {change_log: change_log, result: result_description};

  if (!preview_only && !this.running_ && result_description.stack_modified) {
    response.body.stepin_recommended = true;
  }
};


DebugCommandProcessor.prototype.restartFrameRequest_ = function(
    request, response) {
  if (!request.arguments) {
    return response.failed('Missing arguments');
  }
  var frame = request.arguments.frame;

  // No frames to evaluate in frame.
  if (this.exec_state_.frameCount() == 0) {
    return response.failed('No frames');
  }

  var frame_mirror;
  // Check whether a frame was specified.
  if (!IS_UNDEFINED(frame)) {
    var frame_number = TO_NUMBER(frame);
    if (frame_number < 0 || frame_number >= this.exec_state_.frameCount()) {
      return response.failed('Invalid frame "' + frame + '"');
    }
    // Restart specified frame.
    frame_mirror = this.exec_state_.frame(frame_number);
  } else {
    // Restart selected frame.
    frame_mirror = this.exec_state_.frame();
  }

  var result_description = frame_mirror.restart();
  response.body = {result: result_description};
};


DebugCommandProcessor.prototype.debuggerFlagsRequest_ = function(request,
                                                                 response) {
  // Check for legal request.
  if (!request.arguments) {
    response.failed('Missing arguments');
    return;
  }

  // Pull out arguments.
  var flags = request.arguments.flags;

  response.body = { flags: [] };
  if (!IS_UNDEFINED(flags)) {
    for (var i = 0; i < flags.length; i++) {
      var name = flags[i].name;
      var debugger_flag = debugger_flags[name];
      if (!debugger_flag) {
        continue;
      }
      if ('value' in flags[i]) {
        debugger_flag.setValue(flags[i].value);
      }
      response.body.flags.push({ name: name, value: debugger_flag.getValue() });
    }
  } else {
    for (var name in debugger_flags) {
      var value = debugger_flags[name].getValue();
      response.body.flags.push({ name: name, value: value });
    }
  }
};


DebugCommandProcessor.prototype.v8FlagsRequest_ = function(request, response) {
  var flags = request.arguments.flags;
  if (!flags) flags = '';
  %SetFlags(flags);
};


DebugCommandProcessor.prototype.gcRequest_ = function(request, response) {
  var type = request.arguments.type;
  if (!type) type = 'all';

  var before = %GetHeapUsage();
  %CollectGarbage(type);
  var after = %GetHeapUsage();

  response.body = { "before": before, "after": after };
};


DebugCommandProcessor.prototype.dispatch_ = (function() {
  var proto = DebugCommandProcessor.prototype;
  return {
    "continue":             proto.continueRequest_,
    "break"   :             proto.breakRequest_,
    "setbreakpoint" :       proto.setBreakPointRequest_,
    "changebreakpoint":     proto.changeBreakPointRequest_,
    "clearbreakpoint":      proto.clearBreakPointRequest_,
    "clearbreakpointgroup": proto.clearBreakPointGroupRequest_,
    "disconnect":           proto.disconnectRequest_,
    "setexceptionbreak":    proto.setExceptionBreakRequest_,
    "listbreakpoints":      proto.listBreakpointsRequest_,
    "backtrace":            proto.backtraceRequest_,
    "frame":                proto.frameRequest_,
    "scopes":               proto.scopesRequest_,
    "scope":                proto.scopeRequest_,
    "setvariablevalue":     proto.setVariableValueRequest_,
    "evaluate":             proto.evaluateRequest_,
    "lookup":               proto.lookupRequest_,
    "references":           proto.referencesRequest_,
    "source":               proto.sourceRequest_,
    "scripts":              proto.scriptsRequest_,
    "suspend":              proto.suspendRequest_,
    "version":              proto.versionRequest_,
    "changelive":           proto.changeLiveRequest_,
    "restartframe":         proto.restartFrameRequest_,
    "flags":                proto.debuggerFlagsRequest_,
    "v8flag":               proto.v8FlagsRequest_,
    "gc":                   proto.gcRequest_,
  };
})();


// Check whether the previously processed command caused the VM to become
// running.
DebugCommandProcessor.prototype.isRunning = function() {
  return this.running_;
};


DebugCommandProcessor.prototype.systemBreak = function(cmd, args) {
  return %SystemBreak();
};


/**
 * Convert an Object to its debugger protocol representation. The representation
 * may be serilized to a JSON object using JSON.stringify().
 * This implementation simply runs through all string property names, converts
 * each property value to a protocol value and adds the property to the result
 * object. For type "object" the function will be called recursively. Note that
 * circular structures will cause infinite recursion.
 * @param {Object} object The object to format as protocol object.
 * @param {MirrorSerializer} mirror_serializer The serializer to use if any
 *     mirror objects are encountered.
 * @return {Object} Protocol object value.
 */
function ObjectToProtocolObject_(object, mirror_serializer) {
  var content = {};
  for (var key in object) {
    // Only consider string keys.
    if (typeof key == 'string') {
      // Format the value based on its type.
      var property_value_json = ValueToProtocolValue_(object[key],
                                                      mirror_serializer);
      // Add the property if relevant.
      if (!IS_UNDEFINED(property_value_json)) {
        content[key] = property_value_json;
      }
    }
  }

  return content;
}


/**
 * Convert an array to its debugger protocol representation. It will convert
 * each array element to a protocol value.
 * @param {Array} array The array to format as protocol array.
 * @param {MirrorSerializer} mirror_serializer The serializer to use if any
 *     mirror objects are encountered.
 * @return {Array} Protocol array value.
 */
function ArrayToProtocolArray_(array, mirror_serializer) {
  var json = [];
  for (var i = 0; i < array.length; i++) {
    json.push(ValueToProtocolValue_(array[i], mirror_serializer));
  }
  return json;
}


/**
 * Convert a value to its debugger protocol representation.
 * @param {*} value The value to format as protocol value.
 * @param {MirrorSerializer} mirror_serializer The serializer to use if any
 *     mirror objects are encountered.
 * @return {*} Protocol value.
 */
function ValueToProtocolValue_(value, mirror_serializer) {
  // Format the value based on its type.
  var json;
  switch (typeof value) {
    case 'object':
      if (value instanceof Mirror) {
        json = mirror_serializer.serializeValue(value);
      } else if (IS_ARRAY(value)){
        json = ArrayToProtocolArray_(value, mirror_serializer);
      } else {
        json = ObjectToProtocolObject_(value, mirror_serializer);
      }
      break;

    case 'boolean':
    case 'string':
    case 'number':
      json = value;
      break;

    default:
      json = null;
  }
  return json;
}


// -------------------------------------------------------------------
// Exports

utils.InstallConstants(global, [
  "Debug", Debug,
  "DebugCommandProcessor", DebugCommandProcessor,
  "BreakEvent", BreakEvent,
  "CompileEvent", CompileEvent,
  "BreakPoint", BreakPoint,
]);

// Functions needed by the debugger runtime.
utils.InstallFunctions(utils, DONT_ENUM, [
  "MakeExecutionState", MakeExecutionState,
  "MakeExceptionEvent", MakeExceptionEvent,
  "MakeBreakEvent", MakeBreakEvent,
  "MakeCompileEvent", MakeCompileEvent,
  "MakeAsyncTaskEvent", MakeAsyncTaskEvent,
  "IsBreakPointTriggered", IsBreakPointTriggered,
]);

})
