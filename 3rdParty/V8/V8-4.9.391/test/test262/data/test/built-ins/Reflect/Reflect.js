// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1
description: >
  The Reflect object is an ordinary object.
info: >
  26.1 The Reflect Object

  The Reflect object is the %Reflect% intrinsic object and the initial value of
  the Reflect property of the global object. The Reflect object is an ordinary
  object.

  The Reflect object is not a function object. It does not have a [[Construct]]
  internal method; it is not possible to use the Reflect object as a constructor
  with the new operator. The Reflect object also does not have a [[Call]]
  internal method; it is not possible to invoke the Reflect object as a
  function.
---*/

assert.sameValue(typeof Reflect, 'object', '`typeof Reflect` is `"object"`');

// Reflect is not callable
assert.throws(TypeError, function() {
  Reflect();
});

// Reflect doesn't have a constructor
assert.throws(TypeError, function() {
  new Reflect();
});
