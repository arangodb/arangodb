// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Assignment of function `name` attribute (ClassExpression)
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
features: [class]
---*/

var xCls, cls;

({ xCls = class x {} } = {});
({ cls = class {} } = {});

assert(xCls.name !== 'xCls');

assert.sameValue(cls.name, 'cls');
verifyNotEnumerable(cls, 'name');
verifyNotWritable(cls, 'name');
verifyConfigurable(cls, 'name');
