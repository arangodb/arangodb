// Copyright (C) Copyright 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 14.5.16
description: Assignment of function `name` attribute
info: >
    ClassExpression : class BindingIdentifieropt ClassTail

    5. If className is not undefined, then
       a. Let hasNameProperty be HasOwnProperty(value, "name").
       b. ReturnIfAbrupt(hasNameProperty).
       c. If hasNameProperty is false, then
          i. Perform SetFunctionName(value, className).
includes: [propertyHelper.js]
---*/

assert.sameValue(Object.hasOwnProperty.call(class {}, 'name'), false);

assert.sameValue(class cls {}.name, 'cls');
verifyNotEnumerable(class cls {}, 'name');
verifyNotWritable(class cls {}, 'name');
verifyConfigurable(class cls {}, 'name');

assert.sameValue(
  Object.hasOwnProperty.call(class { constructor() {} }, 'name'), false
);

assert.sameValue(class cls { constructor() {} }.name, 'cls');
verifyNotEnumerable(class cls { constructor() {} }, 'name');
verifyNotWritable(class cls { constructor() {} }, 'name');
verifyConfigurable(class cls { constructor() {} }, 'name');
