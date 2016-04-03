// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: B.3.1
description: The value of the `__proto__` property key is assigned to the [[Prototype]] internal slot.
info: >
  __proto__ Property Names in Object Initializers

  ...
  6. If propKey is the String value "__proto__" and if IsComputedPropertyKey(propKey) is false, then
    a. If Type(propValue) is either Object or Null, then
      i. Return object.[[SetPrototypeOf]](propValue).
    b. Return NormalCompletion(empty).
  ...
---*/

var proto = {};

var object = {
  __proto__: proto
};

assert.sameValue(Object.getPrototypeOf(object), proto);
