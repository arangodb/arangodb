// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: B.2.1
description: >
    Object.getOwnPropertyDescriptor returns data desc for functions on
    built-ins (Global.escape)
includes:
    - fnGlobalObject.js
    - propertyHelper.js
---*/

var global = fnGlobalObject();

verifyWritable(global, "escape");
verifyNotEnumerable(global, "escape");
verifyConfigurable(global, "escape");
