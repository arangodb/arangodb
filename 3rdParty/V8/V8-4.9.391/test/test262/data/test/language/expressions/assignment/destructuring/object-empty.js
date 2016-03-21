// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    An ObjectAssignmentPattern without an AssignmentPropertyList requires
    object-coercible values and throws for other values.
es6id: 12.14.5.2
---*/

var value, result;

assert.throws(TypeError, function() {
  0, {} = undefined;
});

assert.throws(TypeError, function() {
  0, {} = null;
});

result = {} = true;

assert.sameValue(result, true);

result = {} = 1;

assert.sameValue(result, 1);

result = {} = 'string literal';

assert.sameValue(result, 'string literal');

value = Symbol('s');
result = {} = value;

assert.sameValue(result, value);

value = {};
result = {} = value;

assert.sameValue(result, value);
