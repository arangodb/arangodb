// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Assignment of function `name` attribute (ClassExpression)
es6id: 12.14.5.3
info: >
    AssignmentElement[Yield] : DestructuringAssignmentTarget Initializeropt

    [...]
    7. If Initializer is present and value is undefined and
       IsAnonymousFunctionDefinition(Initializer) and IsIdentifierRef of
       DestructuringAssignmentTarget are both true, then
       a. Let hasNameProperty be HasOwnProperty(v, "name").
       b. ReturnIfAbrupt(hasNameProperty).
       c. If hasNameProperty is false, perform SetFunctionName(v,
          GetReferencedName(lref)).
includes: [propertyHelper.js]
features: [class]
---*/

var xCls, cls;

[ xCls = class x {} ] = [];;
[ cls = class {} ] = [];

assert(xCls.name !== 'xCls');

assert.sameValue(cls.name, 'cls');
verifyNotEnumerable(cls, 'name');
verifyNotWritable(cls, 'name');
verifyConfigurable(cls, 'name');
