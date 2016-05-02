// Copyright 2012 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
es5id: 11.3.2_L15
description: >
    Tests that the getter for Intl.NumberFormat.prototype.format
    meets the requirements for built-in objects defined by the
    introduction of chapter 17 of the ECMAScript Language
    Specification.
author: Norbert Lindenberg
includes: [testBuiltInObject.js]
---*/

testBuiltInObject(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, "format").get , true, false, [], 0);
