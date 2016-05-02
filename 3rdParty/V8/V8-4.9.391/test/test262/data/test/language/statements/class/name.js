// Copyright (C) Copyright 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 14.5.15
description: Assignment of function `name` attribute
info: >
    ClassDeclaration : class BindingIdentifier ClassTail

    [...]
    6. If hasNameProperty is false, then perform SetFunctionName(value,
       className).
includes: [propertyHelper.js]
---*/

class Test262 {}

assert.sameValue(Test262.name, 'Test262');
verifyNotEnumerable(Test262, 'name');
verifyNotWritable(Test262, 'name');
verifyConfigurable(Test262, 'name');
