// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.1
description: >
    Error creating object with custom constructor (traversed via iterator)
info: >
    [...]
    6. If usingIterator is not undefined, then
       a. If IsConstructor(C) is true, then
          i. Let A be Construct(C).
       b. Else,
          i. Let A be ArrayCreate(0).
       c. ReturnIfAbrupt(A).
features: [Symbol.iterator]
---*/

var C = function() {
  throw new Test262Error();
};
var items = {};
items[Symbol.iterator] = function() {};

assert.throws(Test262Error, function() {
  Array.from.call(C, items);
});
