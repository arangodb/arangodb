// Copyright (C) 2014 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator ++x calls PutValue(lhs, newValue)
es5id: S11.4.4_A5_T4
description: >
    Evaluating LeftHandSideExpression lhs returns Reference type; Reference
    base value is an environment record and environment record kind is
    object environment record. PutValue(lhs, newValue) uses the initially
    created Reference even if the environment binding is no longer present.
    No ReferenceError is thrown when '++x' is in strict-mode code and the
    original binding is no longer present.
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
    ++x;
  })();
}

if (scope.x !== 3) {
  $ERROR('#1: scope.x === 3. Actual: ' + (scope.x));
}
