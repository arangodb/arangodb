// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 13.6.4.13
description: >
    If retrieving an iterator's `return` method generates an error while
    closing the iterator, the error should be forwarded to the runtime.
features: [Symbol.iterator]
---*/

var iterable = {};
var iterationCount = 0;

iterable[Symbol.iterator] = function() {
  return {
    next: function() {
      return { done: false, value: null };
    },
    get return() {
      throw new Test262Error();
    }
  };
};

assert.throws(Test262Error, function() {
  for (var x of iterable) {
    iterationCount += 1;
    break;
  }
});

assert.sameValue(iterationCount, 1, 'The loop body is evaluated');
