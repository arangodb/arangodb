// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.1
description: >
    `this` value of mapping function in strict mode (traversed via iterator)
info: >
    [...]
    2. If mapfn is undefined, let mapping be false.
    3. else
       a. If IsCallable(mapfn) is false, throw a TypeError exception.
       b. If thisArg was supplied, let T be thisArg; else let T be undefined.
       c. Let mapping be true
    [...]
    6. If usingIterator is not undefined, then
       [...]
       g. Repeat
          [...]
          vii. If mapping is true, then
               1. Let mappedValue be Call(mapfn, T, «nextValue, k»).
features: [Symbol.iterator]
flags: [onlyStrict]
---*/

var thisVals = [];
var nextResult = { done: false, value: {} };
var nextNextResult = { done: false, value: {} };
var mapFn = function() {
  thisVals.push(this);
};
var items = {};

items[Symbol.iterator] = function() {
  return {
    next: function() {
      var result = nextResult;
      nextResult = nextNextResult;
      nextNextResult = { done: true };

      return result;
    }
  };
};

Array.from(items, mapFn);

assert.sameValue(thisVals.length, 2);
assert.sameValue(thisVals[0], undefined, 'First iteration `this` value');
assert.sameValue(thisVals[1], undefined, 'Second iteration `this` value');
