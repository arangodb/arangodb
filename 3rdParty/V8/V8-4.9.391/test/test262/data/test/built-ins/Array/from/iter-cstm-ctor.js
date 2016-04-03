// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.1
description: Creating object with custom constructor (traversed via iterator)
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

var thisVal, args;
var callCount = 0;
var C = function() {
  thisVal = this;
  args = arguments;
  callCount += 1;
};
var result;
var items = {};
items[Symbol.iterator] = function() {
  return {
    next: function() {
      return { done: true };
    }
  };
};

result = Array.from.call(C, items);

assert(
  result instanceof C, 'Constructed value is an instance of the constructor'
);
assert.sameValue(
  result.constructor,
  C,
  'Constructed value correctly defines a `constructor` property'
);
assert.sameValue(callCount, 1, 'Constructor invoked exactly once');
assert.sameValue(thisVal, result, 'Constructed value is returned');
assert.sameValue(args.length, 0, 'Constructor invoked without arguments');
