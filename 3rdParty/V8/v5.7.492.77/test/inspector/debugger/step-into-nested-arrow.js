// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log(
    'Checks that stepInto nested arrow function doesn\'t produce crash.');

InspectorTest.setupScriptMap();
InspectorTest.addScript(`
const rec = (x) => (y) =>
rec();
//# sourceURL=test.js`);

Protocol.Debugger.onPaused(message => {
  InspectorTest.log("paused");
  InspectorTest.logCallFrames(message.params.callFrames);
  Protocol.Debugger.stepInto();
})

Protocol.Debugger.enable();
Protocol.Debugger.setBreakpointByUrl({ url: 'test.js', lineNumber: 2 })
  .then(() => Protocol.Runtime.evaluate({ expression: 'rec(5)(4)' }))
  .then(InspectorTest.completeTest);
