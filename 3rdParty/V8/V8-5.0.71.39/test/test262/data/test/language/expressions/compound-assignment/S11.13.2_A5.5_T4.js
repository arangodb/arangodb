// Copyright (C) 2014 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Compound Assignment Operator calls PutValue(lref, v)
es5id: S11.13.2_A5.5_T4
description: >
    Evaluating LeftHandSideExpression lref returns Reference type; Reference
    base value is an environment record and environment record kind is
    object environment record. PutValue(lref, v) uses the initially
    created Reference even if the environment binding is no longer present.
    No ReferenceError is thrown when 'x -= y' is in strict-mode code and the
    original binding is no longer present.
    Check operator is "x -= y".
flags: [noStrict]
---*/

var scope = {
  get x() {
    delete this.x;
    return 2;
  }
};

with (scope) {
  (function() {
    "use strict";
    x -= 1;
  })();
}

if (scope.x !== 1) {
  $ERROR('#1: scope.x === 1. Actual: ' + (scope.x));
}
