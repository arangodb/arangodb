// Copyright (C) 2016 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Object.getOwnPropertyDescriptors should have length 1
esid: pending
author: Jordan Harband
includes: [propertyHelper.js]
---*/

assert.sameValue(Object.getOwnPropertyDescriptors.length, 1, 'Expected Object.getOwnPropertyDescriptors.length to be 1');

verifyNotEnumerable(Object.getOwnPropertyDescriptors, 'length');
verifyNotWritable(Object.getOwnPropertyDescriptors, 'length');
verifyConfigurable(Object.getOwnPropertyDescriptors, 'length');
