// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.1.1
description: >
  Throws a TypeError if iterable itens are not Objects.
info: >
  WeakMap ( [ iterable ] )

  ...
  9. Repeat
    ...
    d. Let nextItem be IteratorValue(next).
    e. ReturnIfAbrupt(nextItem).
    f. If Type(nextItem) is not Object,
      i. Let error be Completion{[[type]]: throw, [[value]]: a newly created
      TypeError object, [[target]]:empty}.
      ii. Return IteratorClose(iter, error).
features: [Symbol]
---*/

assert.throws(TypeError, function() {
  new WeakMap([1, 1]);
});

assert.throws(TypeError, function() {
  new WeakMap(['', 1]);
});

assert.throws(TypeError, function() {
  new WeakMap([true, 1]);
});

assert.throws(TypeError, function() {
  new WeakMap([null, 1]);
});

assert.throws(TypeError, function() {
  new WeakMap([Symbol('a'), 1]);
});

assert.throws(TypeError, function() {
  new WeakMap([undefined, 1]);
});

assert.throws(TypeError, function() {
  new WeakMap([['a', 1], 2]);
});
