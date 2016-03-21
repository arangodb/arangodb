// Copyright (C) Copyright 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: B.3.1
description: Function name is not assigned based on the property name
info: >
    [...]
    6. If propKey is the String value "__proto__" and if
       IsComputedPropertyKey(propKey) is false, then
       a. If Type(propValue) is either Object or Null, then
          i. Return object.[[SetPrototypeOf]](propValue).
       b. Return NormalCompletion(empty).
    7. If IsAnonymousFunctionDefinition(AssignmentExpression) is true, then
       a. Let hasNameProperty be HasOwnProperty(propValue, "name").
       b. ReturnIfAbrupt(hasNameProperty).
       c. If hasNameProperty is false, perform SetFunctionName(propValue, propKey).
includes: [propertyHelper.js]
---*/

var o;

o = {
  __proto__: function() {}
};

assert(o.__proto__.name !== '__proto__');
