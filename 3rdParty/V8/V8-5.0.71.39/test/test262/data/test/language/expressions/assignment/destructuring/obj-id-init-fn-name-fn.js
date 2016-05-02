// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Assignment of function `name` attribute (FunctionExpression)
es6id: 12.14.5.2
info: >
    AssignmentProperty : IdentifierReference Initializeropt

    [...]
    6. If Initializeropt is present and v is undefined, then
       [...]
       d. If IsAnonymousFunctionDefinition(Initializer) is true, then
          i. Let hasNameProperty be HasOwnProperty(v, "name").
          ii. ReturnIfAbrupt(hasNameProperty).
          iii. If hasNameProperty is false, perform SetFunctionName(v, P).
includes: [propertyHelper.js]
---*/

var xFn, fn;

({ xFn = function x() {} } = {});
({ fn = function() {} } = {});

assert(xFn.name !== 'xFn');

assert.sameValue(fn.name, 'fn');
verifyNotEnumerable(fn, 'name');
verifyNotWritable(fn, 'name');
verifyConfigurable(fn, 'name');
