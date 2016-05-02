// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When DestructuringAssignmentTarget is an array literal and the value is not
    defined, a TypeError should be thrown.
es6id: 12.14.5.4
---*/

var x;

assert.throws(TypeError, function() {
  ({ x: [ x ] } = {});
});
