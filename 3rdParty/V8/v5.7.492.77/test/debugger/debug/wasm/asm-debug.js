// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --validate-asm

Debug = debug.Debug

// Initialized in setup().
var exception;
var break_count;
var num_wasm_scripts;
var module;

var filename = '(?:[^ ]+/)?test/mjsunit/wasm/asm-debug.js';
filename = filename.replace(/\//g, '[/\\\\]');

var expected_stack_entries = [];

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      ++break_count;
      // Request frame details.
      var num_frames = exec_state.frameCount();
      assertEquals(
          expected_stack_entries.length, num_frames, 'number of frames');
      print('Stack Trace (length ' + num_frames + '):');
      for (var i = 0; i < num_frames; ++i) {
        var frame = exec_state.frame(i);
        var script = frame.script();
        assertNotNull(script);
        var line = frame.sourceLine() + 1;
        var column = frame.sourceColumn() + 1;
        var funcName = frame.func().name();
        var name = script.name();
        print(
            '  [' + i + '] ' + funcName + ' (' + name + ':' + line + ':' +
            column + ')');
        assertMatches(filename, name, 'name');
        assertEquals(
            expected_stack_entries[i][0], funcName, 'function name at ' + i);
        assertEquals(expected_stack_entries[i][1], line, 'line at ' + i);
        assertEquals(expected_stack_entries[i][2], column, 'column at ' + i);
      }
    }
  } catch (e) {
    print('exception: ' + e);
    exception = e;
  }
};

function generateWasmFromAsmJs(stdlib, foreign, heap) {
  'use asm';
  var debugger_fun = foreign.call_debugger;
  function callDebugger() {
    debugger_fun();
  }
  function redirectFun() {
    callDebugger();
  }
  return redirectFun;
}

function call_debugger() {
  debugger;
}

function setup() {
  exception = null;
  break_count = 0;
}

(function FrameInspection() {
  setup();
  var fun =
      generateWasmFromAsmJs(this, {'call_debugger': call_debugger}, undefined);
  expected_stack_entries = [
    ['call_debugger', 66, 3],    // --
    ['callDebugger', 57, 5],     // --
    ['redirectFun', 60, 5],      // --
    ['FrameInspection', 86, 3],  // --
    ['', 89, 3]
  ];
  Debug.setListener(listener);
  fun();
  Debug.setListener(null);
  assertEquals(1, break_count);
})();
