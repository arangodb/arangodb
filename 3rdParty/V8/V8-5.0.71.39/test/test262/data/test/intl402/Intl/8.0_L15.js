// Copyright 2012 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
es5id: 8.0_L15
description: >
    Tests that Intl meets the requirements for built-in objects
    defined by the introduction of chapter 17 of the ECMAScript
    Language Specification.
author: Norbert Lindenberg
includes:
    - fnGlobalObject.js
    - testBuiltInObject.js
---*/

testBuiltInObject(fnGlobalObject().Intl, false, false, []);
testBuiltInObject(Intl, false, false, ["Collator", "NumberFormat", "DateTimeFormat"]);
