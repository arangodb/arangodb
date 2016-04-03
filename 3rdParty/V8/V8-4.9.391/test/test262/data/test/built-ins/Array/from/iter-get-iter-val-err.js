// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.1
description: Error retrieving value of iterator result
info: >
    [...]
    6. If usingIterator is not undefined, then
       [...]
       g. Repeat
          [...]
          v. Let nextValue be IteratorValue(next).
          vi. ReturnIfAbrupt(nextValue).
features: [Symbol.iterator]
---*/

var items = {};
var poisonedValue = {};
Object.defineProperty(poisonedValue, 'value', {
  get: function() {
    throw new Test262Error();
  }
});
items[Symbol.iterator] = function() {
  return {
    next: function() {
      return poisonedValue;
    }
  };
};

assert.throws(Test262Error, function() {
  Array.from(items);
});
