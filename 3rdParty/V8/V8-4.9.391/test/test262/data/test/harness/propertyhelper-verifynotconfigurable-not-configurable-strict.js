// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Objects whose specified property is not configurable satisfy the assertion
    in strict mode.
includes: [propertyHelper.js]
flags: [onlyStrict]
---*/

var obj = {};
Object.defineProperty(obj, 'a', {
  configurable: false
});

verifyNotConfigurable(obj, 'a');
