// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.1
description: Error setting property on result value (traversed via iterator)
info: >
    [...]
    6. If usingIterator is not undefined, then
       [...]
       g. Repeat
          [...]
          ix. Let defineStatus be CreateDataPropertyOrThrow(A, Pk,
              mappedValue).
          x. If defineStatus is an abrupt completion, return
             IteratorClose(iterator, defineStatus).
features: [Symbol.iterator]
---*/

var C = function() {
  Object.defineProperty(this, '0', { configurable: false });
};
var closeCount = 0;
var items = {};
var nextResult = { done: false };

items[Symbol.iterator] = function() {
  return {
    return: function() {
      closeCount += 1;
    },
    next: function() {
      var result = nextResult;

      nextResult = { done: true };

      return result;
    }
  };
};

assert.throws(TypeError, function() {
  Array.from.call(C, items);
});

assert.sameValue(closeCount, 1);
