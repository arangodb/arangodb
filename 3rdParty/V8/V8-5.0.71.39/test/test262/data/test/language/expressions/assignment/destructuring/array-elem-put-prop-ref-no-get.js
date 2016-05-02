// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    If the DestructuringAssignmentTarget of an AssignmentElement is a
    PropertyReference, it should not be evaluated.
es6id: 12.14.5.3
---*/

var value = [23];
var x, setValue, result;
x = {
  get y() {
    $ERROR('The property should not be accessed.');
  },
  set y(val) {
    setValue = val;
  }
};

result = [x.y] = value;

assert.sameValue(result, value);
assert.sameValue(setValue, 23);
