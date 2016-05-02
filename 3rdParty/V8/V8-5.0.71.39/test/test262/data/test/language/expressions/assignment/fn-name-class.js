// Copyright (C) Copyright 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 12.14.4
description: Assignment of function `name` attribute (ClassExpression)
info: >
    AssignmentExpression[In, Yield] :
        LeftHandSideExpression[?Yield] = AssignmentExpression[?In, ?Yield]

    1. If LeftHandSideExpression is neither an ObjectLiteral nor an
       ArrayLiteral, then
       [...]
       e. If IsAnonymousFunctionDefinition(AssignmentExpression) and
          IsIdentifierRef of LeftHandSideExpression are both true, then

          i. Let hasNameProperty be HasOwnProperty(rval, "name").
          ii. ReturnIfAbrupt(hasNameProperty).
          iii. If hasNameProperty is false, perform SetFunctionName(rval,
               GetReferencedName(lref)).
includes: [propertyHelper.js]
features: [class]
---*/

var xCls, cls;

xCls = class x {};
cls = class {};

assert(xCls.name !== 'xCls');

assert.sameValue(cls.name, 'cls');
verifyNotEnumerable(cls, 'name');
verifyNotWritable(cls, 'name');
verifyConfigurable(cls, 'name');
