// Copyright (C) 2016 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Object.getOwnPropertyDescriptors should have name property with value 'getOwnPropertyDescriptors'
esid: pending
author: Jordan Harband
includes: [propertyHelper.js]
---*/

assert.sameValue(
    Object.getOwnPropertyDescriptors.name,
    'getOwnPropertyDescriptors',
    'Expected Object.getOwnPropertyDescriptors.name to be "getOwnPropertyDescriptors"'
);

verifyNotEnumerable(Object.getOwnPropertyDescriptors, 'name');
verifyNotWritable(Object.getOwnPropertyDescriptors, 'name');
verifyConfigurable(Object.getOwnPropertyDescriptors, 'name');
