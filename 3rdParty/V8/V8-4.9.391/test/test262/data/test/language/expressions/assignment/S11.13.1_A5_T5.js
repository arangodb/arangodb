// Copyright (C) 2014 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Assignment Operator calls PutValue(lref, rval)
es5id: S11.13.1_A5_T5
description: >
    Evaluating LeftHandSideExpression lref returns Reference type; Reference
    base value is an environment record and environment record kind is
    object environment record. PutValue(lref, rval) uses the initially
    created Reference even if the environment binding is no longer present.
    No ReferenceError is thrown when assignment is in strict-mode code and the
    original binding is no longer present.
includes:
    - fnGlobalObject.js
---*/

Object.defineProperty(fnGlobalObject(), "x", {
  configurable: true,
  value: 1
});

(function() {
  "use strict";
  x = (delete fnGlobalObject().x, 2);
})();

if (fnGlobalObject().x !== 2) {
  $ERROR('#1: fnGlobalObject().x === 2. Actual: ' + (fnGlobalObject().x));
}
