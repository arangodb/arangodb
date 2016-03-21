// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.1
description: Error advancing iterator
info: >
    [...]
    6. If usingIterator is not undefined, then
       [...]
       g. Repeat
          i. Let Pk be ToString(k).
          ii. Let next be IteratorStep(iterator).
          iii. ReturnIfAbrupt(next).
features: [Symbol.iterator]
---*/

var items = {};
items[Symbol.iterator] = function() {
  return {
    next: function() {
      throw new Test262Error();
    }
  };
};

assert.throws(Test262Error, function() {
  Array.from(items);
});
