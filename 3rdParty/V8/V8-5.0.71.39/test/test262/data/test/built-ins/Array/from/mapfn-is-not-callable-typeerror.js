// Copyright 2015 Leonardo Balter. All rights reserved.
// This code is governed by the license found in the LICENSE file.
/*---
es6id: 22.1.2.1
description: Throws a TypeError if mapFn is not callable
info: >
  22.1.2.1 Array.from ( items [ , mapfn [ , thisArg ] ] )

  ...
  2. If mapfn is undefined, let mapping be false.
  3. else
    a. If IsCallable(mapfn) is false, throw a TypeError exception.
    ...
---*/

assert.throws(TypeError, function() {
  Array.from([], null);
});

assert.throws(TypeError, function() {
  Array.from([], {});
});

assert.throws(TypeError, function() {
  Array.from([], 'string');
});

assert.throws(TypeError, function() {
  Array.from([], true);
});

assert.throws(TypeError, function() {
  Array.from([], 42);
});
