// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.2.12
description: >
  Provide unmapped `arguments` if any parameter has a default value initializer.
info: >
  9.2.12 FunctionDeclarationInstantiation(func, argumentsList)

  22. If argumentsObjectNeeded is true, then
    a. If strict is true or if simpleParameterList is false, then
      i. Let ao be CreateUnmappedArgumentsObject(argumentsList).
    ...
    d. If strict is true, then
      ...
    e. Else,
      i. Let status be envRec.CreateMutableBinding("arguments").
flags: [noStrict]
---*/

var _c;

function fn(a, b = 1, c = 1, d = 1) {
  a = false;
  arguments[2] = false;
  _c = c;
  return arguments;
}

var result = fn(42, undefined, 43);

assert.sameValue(
  result[0], 42,
  'unmapped `arguments` are not bound to their parameters values'
);

assert.sameValue(
  result[1], undefined,
  'unmapped `arguments` preserve the given arguments values'
);

assert.sameValue(
  _c, 43,
  'parameters names are not mapped to be bound'
);

assert.sameValue(
  Object.hasOwnProperty(result, '3'), false,
  'unmapped `arguments` will only contain values from the arguments list'
);
