// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 13.6.4.13 S5.i.i
description: >
    If the left-hand side requires a DestructuringAssignment operation and that
    operation produces an error, the iterator should be closed and the error
    forwarded to the runtime.
features: [Symbol.iterator]
---*/

var callCount = 0;
var iterationCount = 0;
var iterable = {};
var x = {
  set attr(_) {
    throw new Test262Error();
  }
};

iterable[Symbol.iterator] = function() {
  return {
    next: function() {
      return { done: false, value: [0] };
    },
    return: function() {
      callCount += 1;
    }
  }
};

assert.throws(Test262Error, function() {
  for ([x.attr] of iterable) {
    iterationCount += 1;
  }
});

assert.sameValue(iterationCount, 0, 'The loop body is not evaluated');
assert.sameValue(callCount, 1, 'Iterator is closed');
