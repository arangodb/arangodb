// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.3.5-1gs
description: >
    StrictMode - error is thrown when reading the 'caller' property of
    a function object
flags: [onlyStrict]
---*/

"use strict";
function _15_3_5_1_gs() {}

assert.throws(TypeError, function() {
  _15_3_5_1_gs.caller;
});
