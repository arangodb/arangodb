// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Debug = debug.Debug
var counter = 0;
var exception = null;
function f() {
  if (++counter > 5) return;
  debugger;
  return counter;
};

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var original = 'debugger;';
    var patch = 'debugger;\n';
    %LiveEditPatchScript(f, Debug.scriptSource(f).replace(original, patch));
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);
f();
Debug.setListener(null);
assertNull(exception);
assertEquals(6, counter);
