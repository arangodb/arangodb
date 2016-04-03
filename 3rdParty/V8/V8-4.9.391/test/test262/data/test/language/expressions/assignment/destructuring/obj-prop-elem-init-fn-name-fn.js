// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Assignment of function `name` attribute (FunctionExpression)
es6id: 12.14.5.4
info: >
    AssignmentElement[Yield] : DestructuringAssignmentTarget Initializeropt

    [...]
    7. If Initializer is present and v is undefined and
       IsAnonymousFunctionDefinition(Initializer) and IsIdentifierRef of
       DestructuringAssignmentTarget are both true, then
       a. Let hasNameProperty be HasOwnProperty(rhsValue, "name").
       b. ReturnIfAbrupt(hasNameProperty).
       c. If hasNameProperty is false, perform SetFunctionName(rhsValue,
          GetReferencedName(lref)).
includes: [propertyHelper.js]
---*/

var xFn, fn;

({ x: xFn = function x() {} } = {});
({ x: fn = function() {} } = {});

assert(xFn.name !== 'xFn');

assert.sameValue(fn.name, 'fn');
verifyNotEnumerable(fn, 'name');
verifyNotWritable(fn, 'name');
verifyConfigurable(fn, 'name');
